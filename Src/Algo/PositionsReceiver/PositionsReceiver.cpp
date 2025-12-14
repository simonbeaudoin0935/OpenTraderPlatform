#include "PositionsReceiver.h"
#include "TSClient.h"
#include "Logging.h"

#include <QFutureWatcher>

#define LOGGING_CATEGORY PositionsReceiverLog
Q_LOGGING_CATEGORY(PositionsReceiverLog, "PositionsReceiver");

PositionsReceiver::PositionsReceiver(const QString &account, QObject *parent) :
    QObject(parent),
    m_account(account)
{
    this->setObjectName("PositionReceiver");
    
    DEBUG <<  "Starting Positions stream for account " << account;
    
    m_stream = TSClient::getInstance()->openStreamPositions(account);
    Q_CHECK_PTR(m_stream);

    connect(m_stream, &StreamPositions::newPositionReceived, this, &PositionsReceiver::onReceivedNewPosition);
    connect(m_stream, &StreamPositions::positionDeleted, this, &PositionsReceiver::onPositionDeleted);
    connect(m_stream, &StreamPositions::endSnapshotReceived, this,
        [this](){
            INFO << "Received EndSnapshot for Orders stream";
        });


    m_stream->future().then(this,
        [this](){
            CRITICAL << "Positions Receiver future finished";
        }
    ).onFailed(this,
        [this](const std::exception& e){

            CRITICAL << "Positions Receiver future failed for" << m_account
                     << "- Exception:" << QString::fromStdString(e.what());

            Q_ASSERT(false);

            //TODO attempt to restart the stream
        }
    );
}

PositionsReceiver::~PositionsReceiver() {
    Q_ASSERT(m_stream != nullptr);

    TSClient::getInstance()->closeStream(m_stream);
}

void PositionsReceiver::onReceivedNewPosition(Position position)
{
    qCDebug(PositionsReceiverLog).noquote() << "New position for account (" << m_account << ") : " << position.toJsonString();
    emit receivedNewPosition(m_account, position);
}

void PositionsReceiver::onPositionDeleted(QString positionID)
{
    qCDebug(PositionsReceiverLog) << "Position deleted for account (" << m_account << ") : " << positionID;
    emit positionDeleted(m_account, positionID);
}
