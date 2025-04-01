#pragma once

#include <QObject>

#include "../Stream.h"
#include "MarketDepthQuote.h"

class StreamMarketDepthQuote : public Stream {
    Q_OBJECT
public:
    explicit StreamMarketDepthQuote(QNetworkReply *reply);
    ~StreamMarketDepthQuote();
    StreamMarketDepthQuote(const StreamMarketDepthQuote&) = delete;
    StreamMarketDepthQuote& operator=(const StreamMarketDepthQuote&) = delete;

signals:
    void receivedNewQuote(MarketDepthQuote quote);
private:


    void processJson(QByteArray &json);
};
