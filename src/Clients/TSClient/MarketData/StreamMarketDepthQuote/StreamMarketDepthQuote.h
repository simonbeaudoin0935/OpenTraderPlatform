#pragma once

#include <QObject>

#include "Stream.h"
#include "MarketDepthQuote.h"

class StreamMarketDepthQuote : public Stream {
    Q_OBJECT

public:
    explicit StreamMarketDepthQuote(QString &symbol, QObject *parent = nullptr);
    ~StreamMarketDepthQuote();
    StreamMarketDepthQuote(const StreamMarketDepthQuote&) = delete;
    StreamMarketDepthQuote& operator=(const StreamMarketDepthQuote&) = delete;

signals:
    void receivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote);

private:
    QString symbol;
    bool processJsonObject(const QJsonObject& jsonObj);
};
