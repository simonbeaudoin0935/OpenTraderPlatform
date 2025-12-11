#pragma once

#include "Stream.h"
#include "MarketDepthQuote.h"

class StreamMarketDepthQuote final : public Stream {
    Q_OBJECT

public:
    explicit StreamMarketDepthQuote(const QString &symbol, QNetworkReply * reply, QObject *parent = nullptr);
    ~StreamMarketDepthQuote() {};
    StreamMarketDepthQuote(const StreamMarketDepthQuote&) = delete;
    StreamMarketDepthQuote& operator=(const StreamMarketDepthQuote&) = delete;

    QFuture<MarketDepthQuote> future() const { return m_promise.future(); }

private:
    void processJsonObject(const QJsonObject& jsonObj) override;
    QString m_symbol;
    QPromise<MarketDepthQuote> m_promise;
};
