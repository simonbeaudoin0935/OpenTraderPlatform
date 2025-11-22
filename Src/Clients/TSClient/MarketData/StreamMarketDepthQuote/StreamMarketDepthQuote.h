#pragma once

#include <QObject>

#include "Stream.h"
#include "MarketDepthQuote.h"
#include "Ticker.h"

class StreamMarketDepthQuote : public Stream {
    Q_OBJECT

public:
    explicit StreamMarketDepthQuote(const Ticker &symbol, QObject *parent = nullptr);
    ~StreamMarketDepthQuote();
    StreamMarketDepthQuote(const StreamMarketDepthQuote&) = delete;
    StreamMarketDepthQuote& operator=(const StreamMarketDepthQuote&) = delete;

signals:
    void receivedNewMarketDepthQuote(Ticker symbol, MarketDepthQuote quote);

private:
    bool processJsonObject(const QJsonObject& jsonObj);
};
