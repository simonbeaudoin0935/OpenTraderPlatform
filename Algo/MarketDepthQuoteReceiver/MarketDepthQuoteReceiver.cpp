#include "MarketDepthQuoteReceiver.h"
#include "Clients/TSClient/TSClient.h"

Q_LOGGING_CATEGORY(MarketDepthQuoteReceiverLog, "MarketDepthQuoteReceiver")

MarketDepthQuoteReceiver::MarketDepthQuoteReceiver(QObject *parent) :
    QObject{parent}
{

}

void MarketDepthQuoteReceiver::startStream(QString &symbol)
{
    qCDebug(MarketDepthQuoteReceiverLog) << Q_FUNC_INFO << "Starting Market Depth Quote stream for " << symbol;

    StreamMarketDepthQuote *stream = TSClient::getInstance().openStreamMarketDepthQuote(symbol, 10);

    void receivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote);

    connect(stream, &StreamMarketDepthQuote::receivedNewMarketDepthQuote, this, &MarketDepthQuoteReceiver::onReceivedNewMarketDepthQuote);
    connect(stream, &Stream::streamErrorOccurred, this, &MarketDepthQuoteReceiver::onStreamError);

    streams.insert(symbol, stream);
}

void MarketDepthQuoteReceiver::startStream(const char *symbol)
{
    QString symbolStr(symbol);
    startStream(symbolStr);
}

void MarketDepthQuoteReceiver::stopStream(QString &symbol)
{
    qCDebug(MarketDepthQuoteReceiverLog) << Q_FUNC_INFO << "Stopping MarketDepthQuote stream for " << symbol;

    Q_ASSERT(streams.contains(symbol));

    disconnect(streams[symbol]);

    TSClient::getInstance().closeStreamMarketDepthQuote(streams[symbol]);

    bool removed = streams.remove(symbol);
    Q_ASSERT(removed);
}

void MarketDepthQuoteReceiver::stopStream(const char* symbol) {
    QString symbolStr(symbol);
    stopStream(symbolStr);
}

void MarketDepthQuoteReceiver::onReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote marketDepthQuote)
{
    qCDebug(MarketDepthQuoteReceiverLog).noquote() << marketDepthQuote.toJsonString();

    emit currentHighlightedReceivedMarketDepthQuote(symbol, marketDepthQuote);
}

void MarketDepthQuoteReceiver::onStreamError(Stream::StreamError error, QString errorMessage)
{
    qCCritical(MarketDepthQuoteReceiverLog) << "Stream fucked";
#warning TODO complete this by retreiving the stream and close it
}
