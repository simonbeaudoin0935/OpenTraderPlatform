#pragma once

#include "Strategy/StrategyBase.h"
#include "Strategy/StrategyConfig.h"
#include <QObject>
#include <QTimer>
#include <memory>

/**
 * @brief CrashTestStrategy - Test strategy that intentionally crashes
 *
 * This strategy is designed to test:
 * - Signal handler installation
 * - Segfault detection and logging
 * - Error recovery and UI updates
 * - Log capture during crash
 *
 * Behavior: Logs startup message, waits 5 seconds, then intentionally causes a segfault
 * by dereferencing a null pointer.
 */
class CrashTestStrategy : public QObject, public StrategyBase
{
    Q_OBJECT

  public:
    explicit CrashTestStrategy(const StrategyConfig& p_config, StrategySDK* p_sdk);
    ~CrashTestStrategy() override = default;

    // Lifecycle hooks
    void onStart(StrategySDK* p_sdk) override;
    void onStop() override;
    void onPause() override;

    // Data callbacks (unused for this test)
    void onBar(const Bar& p_bar) override;
    void onLevel2(const Level2& p_level2) override;
    void onOrderUpdated(const Order& p_order) override;
    void onOrderFilled(const Order& p_order) override;
    void onOrderCancelled(const Order& p_order, const std::string& p_reason) override;
    void onOrderRejected(const Order& p_order, const std::string& p_reason) override;
    void onPositionUpdated(const Position& p_position) override;
    void onBalanceUpdated(double p_balance) override;
    void onError(const std::string& p_error) override;

  private slots:
    void triggerCrash();

  private:
    QTimer* m_crashTimer;
};
