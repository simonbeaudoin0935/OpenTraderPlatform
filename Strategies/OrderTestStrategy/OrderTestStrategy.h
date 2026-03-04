#pragma once

#include "Strategy/StrategyBase.h"
#include "Strategy/StrategyConfig.h"
#include "Strategy/StrategySDK.h"
#include <QObject>
#include <QTimer>
#include <QString>

/**
 * @brief OrderTestStrategy - Tests the full order lifecycle via the StrategySDK
 *
 * This strategy is a testbed for order placement, monitoring, and cancellation.
 * It runs a deterministic state-machine test:
 *
 *   1. Init         → log balance/orders/positions, place a market buy (1 share)
 *   2. WaitMarketFill → wait for FLL status, query getOrders()/getPositions()
 *   3. WaitLimitOPN  → place a limit buy at $1.00, wait for OPN status
 *   4. WaitCancel    → cancel the limit order, wait for CAN status
 *   5. Complete      → print pass/fail summary
 *
 * Designed to run in Replay or Simulation mode (not Live).
 *
 * Config parameters (in JSON customParams):
 *   "accountID"    : Account to trade on (default: "SIM123456" for replay)
 *   "limitPrice"   : Limit price for the far-from-market order (default: 1.0)
 *   "timeoutSecs"  : Seconds before giving up on any step (default: 60)
 */
class OrderTestStrategy : public QObject, public StrategyBase
{
    Q_OBJECT

  public:
    explicit OrderTestStrategy(const StrategyConfig& p_config, StrategySDK* p_sdk);
    ~OrderTestStrategy() override;

    // Lifecycle
    void onStart(StrategySDK* p_sdk) override;
    void onStop() override;
    void onPause() override {}

    // Data callbacks
    void onBar(const Bar& p_bar) override;
    void onOrderUpdated(const Order& p_order) override;
    void onOrderFilled(const Order& p_order) override;
    void onOrderCancelled(const Order& p_order, const std::string& p_reason) override;
    void onOrderRejected(const Order& p_order, const std::string& p_reason) override;
    void onPositionUpdated(const Position& p_position) override;
    void onBalanceUpdated(double p_balance) override;
    void onError(const std::string& p_error) override;

  private slots:
    void onTimeout();

  private:
    /// Test state machine
    enum class State
    {
        Init,
        WaitMarketFill, ///< Waiting for market order FLL/OPN
        WaitLimitOPN,   ///< Waiting for limit order to become OPN
        WaitCancel,     ///< Waiting for cancel confirmation (CAN)
        Complete        ///< All steps done
    };

    void placeMarketOrder();
    void placeLimitOrder();
    void cancelLimitOrder();
    void printSummary();

    StrategySDK* m_sdk = nullptr;
    StrategyConfig m_config;
    State m_state = State::Init;

    QString m_accountID;
    QString m_symbol;
    double m_limitPrice = 1.0;

    QString m_marketOrderID; ///< ID returned when market order placed
    QString m_limitOrderID;  ///< ID returned when limit order placed

    // Test result tracking
    bool m_marketOrderPlaced = false;
    bool m_marketFillReceived = false;
    bool m_limitOrderPlaced = false;
    bool m_limitOPNReceived = false;
    bool m_cancelSent = false;
    bool m_cancelConfirmed = false;

    QTimer* m_timeoutTimer = nullptr;
};
