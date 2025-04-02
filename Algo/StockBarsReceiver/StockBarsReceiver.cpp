#include "StockBarsReceiver.h"
#include "Clients/TSClient/TSClient.h"

Q_LOGGING_CATEGORY(StockBarsReceiverLog, "StockBarsReceiver")

StockBarsReceiver::StockBarsReceiver(QObject *parent)
    : QObject{parent}
{}

void StockBarsReceiver::startStream(QString &symbol)
{
    qCDebug(StockBarsReceiverLog) << Q_FUNC_INFO << "Starting stream for " << symbol;

    StreamBars *stream = TSClient::getInstance().openStreamBars(symbol,
                                                                1,
                                                                TSClient::StreamBarsUnit::Minute,
                                                                1000,
                                                                TSClient::StreamBarsSessionTemplate::USEQ24Hour);

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
    qCDebug(StockBarsReceiverLog) << Q_FUNC_INFO << "Stopping stream for " << symbol;

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
    qDebug().noquote() << bar.toJsonString();

    emit currentHighlightedReceivedNewBar(symbol, bar);
}

void StockBarsReceiver::onStreamError(Stream::StreamError error, QString errorMessage)
{
    qCCritical(StockBarsReceiverLog) << "Stream fucked";
#warning TODO complete this by retreiving the stream and close it
}
