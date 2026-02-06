#pragma once

#include "StreamMarketData.h"
#include "MarketDepthQuote.h"

class StreamMarketDepthQuote final : public StreamMarketData
{
    Q_OBJECT

  public:
    explicit StreamMarketDepthQuote(const QString& symbol, QNetworkReply* reply, QObject* parent = nullptr);
    ~StreamMarketDepthQuote() = default;
    Q_DISABLE_COPY_MOVE(StreamMarketDepthQuote)

    QString getSymbol() const
    {
        return m_symbol;
    }
  signals:
    void newMarketDepthQuoteReceived(MarketDepthQuote quote);

  private:
    void processJsonObject(const QJsonObject& jsonObj) override;
    QString m_symbol;
};
