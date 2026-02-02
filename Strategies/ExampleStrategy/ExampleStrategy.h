#pragma once

#include "Strategy/StrategyBase.h"
#include "Strategy/StrategyConfig.h"
#include <QObject>

/**
 * @brief ExampleStrategy - Minimal example strategy for testing
 *
 * This strategy demonstrates:
 * - Receiving bar updates
 * - Logging information
 * - Receiving order updates
 * - Interacting with the SDK
 *
 * It simply logs incoming bars and orders without executing trades.
 */
class ExampleStrategy : public StrategyBase
{
  public:
    explicit ExampleStrategy(const StrategyConfig& p_config, StrategySDK* p_sdk);
    ~ExampleStrategy() override = default;

    // Lifecycle hooks
    void onStart(StrategySDK* p_sdk) override;
    void onStop() override;
    void onPause() override;

    // Data callbacks
    void onBar(const Bar& p_bar) override;
    void onMarketDepth(const MarketDepthQuote& p_quote) override;
    void onOrderUpdated(const Order& p_order) override;
    void onOrderFilled(const Order& p_order) override;
    void onOrderCancelled(const Order& p_order, const std::string& p_reason) override;
    void onOrderRejected(const Order& p_order, const std::string& p_reason) override;
    void onPositionUpdated(const Position& p_position) override;
    void onBalanceUpdated(double p_balance) override;
    void onError(const std::string& p_error) override;

  private:
    int m_barCount;
    double m_lastBalance;
};
