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
    
    createPositionsStream();
}

PositionsReceiver::~PositionsReceiver() {
    Q_ASSERT(m_stream != nullptr);

    TSClient::getInstance()->closeStream(m_stream);
}

void PositionsReceiver::createPositionsStream()
{
    DEBUG <<  "Starting Positions stream for account " << m_account;
    
    m_stream = TSClient::getInstance()->openStreamPositions(m_account);
    Q_CHECK_PTR(m_stream);

    auto c1 = connect(m_stream, &StreamPositions::newPositionReceived, this, &PositionsReceiver::onReceivedNewPosition, Qt::UniqueConnection);
    Q_ASSERT(c1);
    
    auto c2 = connect(m_stream, &StreamPositions::positionDeleted, this, &PositionsReceiver::onPositionDeleted, Qt::UniqueConnection);
    Q_ASSERT(c2);
    
    auto c3 = connect(m_stream, &StreamPositions::endSnapshotReceived, this,
        [this](){
            INFO << "Received EndSnapshot for Positions stream";
        }, Qt::UniqueConnection);
    Q_ASSERT(c3);


    m_stream->future().then(this,
        [this](){
            // We get here when the stream is gracefully closed by the receiving side.
            // This should never happen for the positions stream, its supposed to operate during
            // all the program execution.
            CRITICAL << "Not supposed to voluntarily close the positions stream with TSClient::closeStream()";
            Q_ASSERT(false);
        }
    ).onFailed(this,
        [this](const std::exception& e){

            CRITICAL << "Positions Receiver future failed for" << m_account
                     << "- Exception:" << QString::fromStdString(e.what());

            Q_ASSERT(false);

            // Since we are in the failed path, it means the stream on the other end
            // will have called deleteLater() on itself after throwing an exception at us.
            // Its safe to then just re-execute this function, since we don't have to worry amout
            // freeing the current stream variable. 
            QTimer::singleShot(300, this, &PositionsReceiver::createPositionsStream);
        }
    );
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
