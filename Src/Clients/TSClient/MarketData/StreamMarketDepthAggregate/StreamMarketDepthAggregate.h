#pragma once

#include <atomic>

#include "Core/Models/Level2.h"
#include "MarketData/StreamMarketData.h"

class StreamMarketDepthAggregate final : public StreamMarketData
{
    Q_OBJECT

  public:
    explicit StreamMarketDepthAggregate(const QString& symbol, QNetworkReply* reply, QObject* parent = nullptr);
    ~StreamMarketDepthAggregate() override;
    Q_DISABLE_COPY_MOVE(StreamMarketDepthAggregate)

    [[nodiscard]] QString getSymbol() const
    {
        return m_symbol;
    }

    static size_t getNumberOfMarketDepthAggregateStreams()
    {
        return s_numberOfMarketDepthAggregateStreams.load();
    }

    static bool canOpenStream();

  signals:
    void newLevel2Received(Level2 level2);

  private:
    void processJsonObject(const QJsonObject& jsonObj) override;

    QString m_symbol;
    static std::atomic<size_t> s_numberOfMarketDepthAggregateStreams;
};
