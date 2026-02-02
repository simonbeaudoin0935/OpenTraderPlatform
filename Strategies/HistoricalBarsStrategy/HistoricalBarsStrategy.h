#pragma once

#include "Strategy/StrategyBase.h"
#include "Strategy/StrategyConfig.h"
#include <QObject>
#include <QTimer>
#include <QDate>

/**
 * @brief HistoricalBarsStrategy - Test strategy that fetches historical bars
 *
 * This strategy fetches bars from the previous day, then every 2 seconds fetches
 * bars from the day before that, continuing for multiple days.
 * Useful for testing multi-strategy concurrent bar fetching.
 *
 * Configuration: symbol (e.g., "AAPL", "MSFT") via config file
 */
class HistoricalBarsStrategy : public QObject, public StrategyBase
{
    Q_OBJECT

  public:
    explicit HistoricalBarsStrategy(const StrategyConfig& p_config, StrategySDK* p_sdk);
    ~HistoricalBarsStrategy() override;

    void onStart(StrategySDK* sdk) override;
    void onStop() override;
    void onBar(const Bar& bar) override;
    void onOrderUpdated(const Order& order) override;
    void onOrderFilled(const Order& order) override;
    void onOrderCancelled(const Order& order, const std::string& reason) override;
    void onOrderRejected(const Order& order, const std::string& reason) override;
    void onPositionUpdated(const Position& pos) override;
    void onBalanceUpdated(double newBalance) override;
    void onMarketDepth(const MarketDepthQuote& depth) override;
    void onError(const std::string& error) override;

  private slots:
    void fetchNextDay();

  private:
    StrategyConfig m_config;
    StrategySDK* m_sdk;
    QTimer* m_timer;
    QDate m_currentDate;
    int m_daysBack;
    static constexpr int MAX_DAYS_BACK = 5;
};
