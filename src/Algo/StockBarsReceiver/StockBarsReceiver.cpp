#include "StockBarsReceiver.h"
#include "TSClient.h"

Q_LOGGING_CATEGORY(StockBarsReceiverLog, "StockBarsReceiver")

StockBarsReceiver::StockBarsReceiver(QObject *parent)
    : QObject{parent}
{}

void StockBarsReceiver::startStream(QString &symbol)
{
    qCDebug(StockBarsReceiverLog) << Q_FUNC_INFO << "Starting Bars stream for " << symbol;

    StreamBars *stream = TSClient::getInstance().openStreamBars(symbol,
                                                                1,
                                                                Bar::BarUnit::Minute,
                                                                100,
                                                                Bar::BarSessionTemplate::USEQ24Hour);

    connect(stream, &StreamBars::receivedNewBar, this, &StockBarsReceiver::onReceivedNewBar);
    connect(stream, &Stream::streamErrorOccurred, this, &StockBarsReceiver::onStreamError);

    streams.insert(symbol, stream);
}

void StockBarsReceiver::startStream(const char *symbol)
{
    QString symbolStr(symbol);
    startStream(symbolStr);
}

void StockBarsReceiver::stopStream(QString &symbol)
{
    qCDebug(StockBarsReceiverLog) << Q_FUNC_INFO << "Stopping StockBars stream for " << symbol;

    Q_ASSERT(streams.contains(symbol));

    disconnect(streams[symbol]);

    TSClient::getInstance().closeStreamBars(streams[symbol]);

    bool removed = streams.remove(symbol);
    Q_ASSERT(removed);
}

void StockBarsReceiver::stopStream(const char* symbol) {
    QString symbolStr(symbol);
    stopStream(symbolStr);
}

void StockBarsReceiver::onReceivedNewBar(QString symbol, Bar bar)
{
    qCDebug(StockBarsReceiverLog).noquote() << bar.toJsonString();

    emit currentHighlightedReceivedNewBar(symbol, bar);
}

void StockBarsReceiver::onStreamError(Stream::StreamError error, QString errorMessage)
{
    Q_UNUSED(error);
    Q_UNUSED(errorMessage);

    qCCritical(StockBarsReceiverLog) << "Stream fucked";

    // FIXME complete this by retreiving the stream and close it
}
