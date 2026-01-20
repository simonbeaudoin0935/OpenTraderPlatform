#include "PositionsReceiver.h"
#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"

#include <QFutureWatcher>

#define LOGGING_CATEGORY PositionsReceiverLog
Q_LOGGING_CATEGORY(PositionsReceiverLog, "PositionsReceiver");

PositionsReceiver::PositionsReceiver(const QString& account, QObject* parent) : QObject(parent), m_account(account)
{
    this->setObjectName("PositionReceiver");

    createPositionsStream();
}

PositionsReceiver::~PositionsReceiver()
{
    OBJ_ASSUME_TRUE(m_stream != nullptr);

    TSClient::getInstance()->closeStream(m_stream);
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
            [this]() { INFO << "Received EndSnapshot for Orders stream"; });

    m_stream->future().then(this,
                            [this](std::optional<QString> error)
                            {
                                // We get here when the stream is gracefully closed by the receiving side.
                                // This should never happen for the orders stream, its supposed to operate during
                                // all the program execution.

                                if (error.has_value())
                                {
                                    CRITICAL << "Positions stream for account" << m_account
                                             << "finished with error:" << error.value();
                                }
                                else
                                {
                                    DEBUG << "Positions stream for account" << m_account << "finished without error";
                                }

                                // Since we are in the failed path, it means the stream on the other end
                                // will have called deleteLater() on itself after throwing an exception at us.
                                // Its safe to then just re-execute this function, since we don't have to worry amout
                                // freeing the current stream variable.
                                QTimer::singleShot(300, this, &PositionsReceiver::createPositionsStream);
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
