#include "PositionsReceiver.h"
#include "TSClient.h"

Q_LOGGING_CATEGORY(PositionsReceiverLog, "PositionsReceiver");

PositionsReceiver::PositionsReceiver(QObject *parent) :
    QObject(parent)
{

}

void PositionsReceiver::startStream(QString &account)
{
    qCDebug(PositionsReceiverLog) << Q_FUNC_INFO << "Starting Positions stream for account : " << account;

    StreamPositions *stream = TSClient::getInstance().openStreamPositions(account);

    void receivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote);

    connect(stream, &StreamPositions::receivedNewPosition, this, &PositionsReceiver::onReceivedNewPosition);
    connect(stream, &Stream::streamErrorOccurred, this, &PositionsReceiver::onStreamError);

    streams.insert(account, stream);
}

void PositionsReceiver::startStream(const char *account)
{
    QString accountStr(account);
    startStream(accountStr);
}

void PositionsReceiver::onReceivedNewPosition(QString account, Position position)
{
    qCDebug(PositionsReceiverLog).noquote() << "New position for account (" << account << ") : " << position.toJsonString();
    emit receivedNewPosition(account, position);
}

void PositionsReceiver::onStreamError(Stream::StreamError error, QString errorMessage)
{
    // Find which stream triggered the error by checking the sender
    StreamPositions* senderStream = qobject_cast<StreamPositions*>(sender());
    
    QString account;
    if (senderStream) {
        // Find the account associated with this stream
        for (auto it = streams.begin(); it != streams.end(); ++it) {
            if (it.value() == senderStream) {
                account = it.key();
                break;
            }
        }
    }

    qCCritical(PositionsReceiverLog) << "Stream error for account" << account
                                     << "- Error:" << static_cast<int>(error)
                                     << "Message:" << errorMessage;

    // Log specific error types for better diagnostics
    switch (error) {
        case Stream::StreamError::Timeout:
            qCWarning(PositionsReceiverLog) << "Stream timeout for account" << account << "- no data or heartbeat received";
            break;
        case Stream::StreamError::InvalidSymbol:
            qCCritical(PositionsReceiverLog) << "Invalid symbol error for account" << account;
            break;
        case Stream::StreamError::DualLogon:
            qCCritical(PositionsReceiverLog) << "Dual logon detected for account" << account;
            break;
        case Stream::StreamError::GoAway:
            qCWarning(PositionsReceiverLog) << "Server requested stream closure for account" << account;
            break;
        case Stream::StreamError::InternalServerError:
            qCCritical(PositionsReceiverLog) << "Internal server error for account" << account;
            break;
        case Stream::StreamError::BadRequest:
            qCCritical(PositionsReceiverLog) << "Bad request error for account" << account;
            break;
        case Stream::StreamError::Unknown:
            qCCritical(PositionsReceiverLog) << "Unknown stream error for account" << account;
            break;
    }

    // For critical errors, close the stream
    // Note: The stream is already in error state, but we should clean up our reference
    if (senderStream && !account.isEmpty()) {
        qCWarning(PositionsReceiverLog) << "Stream for account" << account << "is in error state";
        // The stream will be cleaned up when stopStream is called or the object is destroyed
        // Automatic recovery could be implemented here if needed
    }
}
