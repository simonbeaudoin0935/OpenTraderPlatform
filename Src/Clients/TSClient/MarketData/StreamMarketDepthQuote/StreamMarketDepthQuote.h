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

    QString getSymbol() const { return m_symbol; }
signals :
    void newMarketDepthQuoteReceived(MarketDepthQuote quote);
    
private:
    void processJsonObject(const QJsonObject& jsonObj) override;
    QString m_symbol;
};
   