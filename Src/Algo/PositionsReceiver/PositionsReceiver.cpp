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
    m_autoReconnect = false; // Disable auto-reconnect when intentionally stopping
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

    m_stream->future().then(this,
                            [this](std::optional<QString> error)
                            {
                                // We get here when the stream is closed (either gracefully or with error)

                                if (error.has_value())
                                {
                                    CRITICAL << "Positions stream for account" << m_account
                                             << "finished with error:" << error.value();
                                }
                                else
                                {
                                    DEBUG << "Positions stream for account" << m_account << "finished without error";
                                }

                                // Only auto-reconnect if not intentionally stopped (e.g., for replay mode)
                                if (m_autoReconnect)
                                {
                                    // Since we are in the failed path, it means the stream on the other end
                                    // will have called deleteLater() on itself after throwing an exception at
                                    // us. Its safe to then just re-execute this function.
                                    QTimer::singleShot(300, this, &PositionsReceiver::createPositionsStream);
                                }
                                else
                                {
                                    DEBUG << "Auto-reconnect disabled, not recreating positions stream";
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
