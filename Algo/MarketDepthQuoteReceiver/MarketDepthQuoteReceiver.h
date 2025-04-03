#pragma once

#include <QObject>

#include "Clients/TSClient/MarketData/StreamMarketDepthQuote/StreamMarketDepthQuote.h"

Q_DECLARE_LOGGING_CATEGORY(MarketDepthQuoteReceiverLog)

class MarketDepthQuoteReceiver : public QObject
{
    Q_OBJECT
public:
    explicit MarketDepthQuoteReceiver(QObject *parent = nullptr);

    void startStream(QString &symbol);
    void startStream(const char* symbol);

    void stopStream(QString &symbol);
    void stopStream(const char* symbol);

signals:
    void currentHighlightedReceivedMarketDepthQuote(QString symbol, MarketDepthQuote marketDepthQuote);

private slots:
    void onReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote marketDepthQuote);
    void onStreamError(Stream::StreamError error, QString errorMessage);

private:
    QMap<QString, StreamMarketDepthQuote*> streams;
};
