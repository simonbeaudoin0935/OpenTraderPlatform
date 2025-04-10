#include "PositionsReceiver.h"
#include "Clients/TSClient/TSClient.h"

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
    qCCritical(PositionsReceiverLog) << "Stream fucked";
#warning TODO complete this by retreiving the stream and close it
}
