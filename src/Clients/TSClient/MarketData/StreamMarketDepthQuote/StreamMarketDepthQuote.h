#pragma once

#include <QObject>

#include "Stream.h"
#include "MarketDepthQuote.h"

class StreamMarketDepthQuote : public Stream {
    Q_OBJECT

public:
    explicit StreamMarketDepthQuote(const QString &symbol, QObject *parent = nullptr);
    ~StreamMarketDepthQuote();
    StreamMarketDepthQuote(const StreamMarketDepthQuote&) = delete;
    StreamMarketDepthQuote& operator=(const StreamMarketDepthQuote&) = delete;

signals:
    void receivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote);

private:
    bool processJsonObject(const QJsonObject& jsonObj);
};
