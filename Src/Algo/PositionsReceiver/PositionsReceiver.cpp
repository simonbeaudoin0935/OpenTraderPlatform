#include "PositionsReceiver.h"
#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"

#include <QFutureWatcher>

#define LOGGING_CATEGORY PositionsReceiverLog
Q_LOGGING_CATEGORY(PositionsReceiverLog, "PositionsReceiver");

PositionsReceiver::PositionsReceiver(const QString& account, QObject* parent)
    : StreamReceiver(parent), m_account(account)
{
    this->setObjectName("PositionReceiver");

    createPositionsStream();
}

PositionsReceiver::~PositionsReceiver()
{
    // Stream may be null if stopStream() was called before destruction
    if (m_stream != nullptr)
    {
        TSClient::getInstance()->closeStream(m_stream);
    }
}

void PositionsReceiver::stopStream(const QString& account)
{
    Q_UNUSED(account);
    if (m_stream != nullptr)
    {
        TSClient::getInstance()->closeStream(m_stream);
        m_stream = nullptr;
        DEBUG << "Positions stream stopped for account" << m_account;
    }
}

void PositionsReceiver::stopStream(const char* account)
{
    stopStream(QString(account));
}

void PositionsReceiver::createPositionsStream()
{
    DEBUG << "Starting Positions stream for account " << m_account;

    m_stream = TSClient::getInstance()->openStreamPositions(m_account);
    Q_CHECK_PTR(m_stream);

    connect(m_stream, &StreamPositions::newPositionReceived, this, &PositionsReceiver::onReceivedNewPosition);
    connect(m_stream, &StreamPositions::positionDeleted, this, &PositionsReceiver::onPositionDeleted);
    connect(m_stream,
            &StreamPositions::endSnapshotReceived,
            this,
            [this]() { INFO << "Received EndSnapshot for Positions stream"; });

    connect(m_stream,
            &Stream::streamClosed,
            this,
            [this](Stream::StreamError reason, QString message)
            {
                if (reason != Stream::StreamError::Closed)
                {
                    CRITICAL << "Positions stream for account" << m_account << "finished with error:" << message;
                    m_stream = nullptr;
                    QTimer::singleShot(300, this, &PositionsReceiver::createPositionsStream);
                }
                else
                {
                    DEBUG << "Positions stream for account" << m_account << "intentionally closed";
                }
            });
}


void PositionsReceiver::onReceivedNewPosition(Position position)
{
    qCDebug(PositionsReceiverLog).noquote()
        << "New position for account (" << m_account << ") : " << position.toJsonString();
    emit receivedNewPosition(m_account, position);
}

void PositionsReceiver::onPositionDeleted(QString positionID)
{
    qCDebug(PositionsReceiverLog) << "Position deleted for account (" << m_account << ") : " << positionID;
    emit positionDeleted(m_account, positionID);
}
