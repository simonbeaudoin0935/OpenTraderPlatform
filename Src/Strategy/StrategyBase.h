#pragma once

#include <QString>
#include <string>
#include <memory>

#include "Bar.h"
#include "Level2.h"
#include "Trade.h"
#include "Order.h"
#include "Position.h"

// Forward declarations
class StrategySDK;

/// @brief Abstract base class for trading strategies
///
/// Strategies inherit from this class and implement the callback methods
/// to react to market data and order/position updates. Provides lifecycle
/// management, common helpers, and thread-safe SDK access.
///
/// Strategy instances run on dedicated QThreads and receive callbacks
/// via Qt signals. No manual thread locking is needed - Qt handles all
/// cross-thread communication.
///
/// Lifecycle:
/// 1. onStart() - Called when strategy thread starts (one-time initialization)
/// 2. onBar/onLevel2/onTrade/onOrderUpdated/etc - Data callbacks during execution
/// 3. onStop() - Called when strategy stops (cleanup resources)
/// 4. onPause() - Called when strategy is paused (optional state preservation)
class StrategyBase
{
  public:
    virtual ~StrategyBase() = default;

    /// @brief Called when strategy is started (one-time initialization)
    /// Happens on the strategy's dedicated thread before any data callbacks.
    /// Use this for resource allocation, state initialization, etc.
    /// @param p_sdk Pointer to StrategySDK for placing orders, querying positions, logging
    /// @note Called on strategy's QThread
    virtual void onStart(StrategySDK* p_sdk) = 0;

    /// @brief Called when strategy is stopped (cleanup)
    /// Happens when strategy is unloaded or platform shuts down.
    /// Use this to close positions, cancel orders, free resources.
    /// @note Called on strategy's QThread
    virtual void onStop() = 0;

    /// @brief Called when strategy is paused (optional)
    /// Happens when user pauses strategy without unloading it.
    /// Default implementation does nothing; override to preserve state.
    /// @note Called on strategy's QThread
    virtual void onPause() {}

    /// @brief Called when a new bar is received for the monitored symbol
    /// @param bar The new bar (OHLCV data)
    /// @note Called on strategy's QThread
    virtual void onBar(const Bar& bar) = 0;

    /// @brief Called when market depth (L2) data is received
    /// @param level2 The Level 2 book snapshot
    /// @note Called on strategy's QThread
    virtual void onLevel2(const Level2& /*level2*/) {}

    /// @brief Called when Level 1 (BBO) data is received
    /// @param level1 The best bid/ask snapshot
    /// @note Called on strategy's QThread
    virtual void onLevel1(const Level1& /*level1*/) {}

    /// @brief Called when a trade print is received
    /// @param trade The individual trade execution
    /// @note Called on strategy's QThread
    virtual void onTrade(const Trade& /*trade*/) {}

    /// @brief Called when an order placed by this strategy is updated
    /// Receives all order status changes (ACK, OPN, FLL, REJ, CAN, etc.)
    /// Check order.getStatus() to determine the specific state change.
    /// This replaces separate onOrderFilled/onOrderCancelled/onOrderRejected callbacks.
    /// @param order The updated order with current status
    /// @note Called on strategy's QThread
    virtual void onOrderUpdated(const Order& order) = 0;

    /// @brief Called when an order placed by this strategy is filled
    /// @param order The filled order with updated status
    /// @deprecated Use onOrderUpdated(const Order&) instead to handle all status changes
    /// @note Called on strategy's QThread
    virtual void onOrderFilled(const Order& order) = 0;

    /// @brief Called when an order placed by this strategy is cancelled
    /// @param order The cancelled order
    /// @param reason Optional reason for cancellation
    /// @deprecated Use onOrderUpdated(const Order&) instead to handle all status changes
    /// @note Called on strategy's QThread
    virtual void onOrderCancelled(const Order& order, const std::string& reason) = 0;

    /// @brief Called when an order placed by this strategy is rejected
    /// @param order The rejected order
    /// @param reason Reason for rejection
    /// @deprecated Use onOrderUpdated(const Order&) instead to handle all status changes
    /// @note Called on strategy's QThread
    virtual void onOrderRejected(const Order& order, const std::string& reason) = 0;

    /// @brief Called when a position changes (updated, closed, or modified)
    /// @param position The updated position
    /// @note Called on strategy's QThread
    virtual void onPositionUpdated(const Position& position) = 0;

    /// @brief Called when account balance is updated
    /// @param newBalance The new account balance
    /// @note Called on strategy's QThread
    virtual void onBalanceUpdated(double newBalance) = 0;

    /// @brief Called when an error occurs in the strategy or platform
    /// @param error Error message
    /// @note Called on strategy's QThread
    virtual void onError(const std::string& error) = 0;

  protected:
    /// @brief Get access to Strategy SDK for order placement and querying
    /// @return Pointer to StrategySDK instance (set during onStart)
    /// @note Valid only after onStart() is called
    [[nodiscard]] StrategySDK* getSdk() const
    {
        return m_sdk;
    }

    /// @brief Log a message from the strategy
    /// @param message Log message
    /// @param level Log level (Debug, Info, Warning, Error)
    /// @note Thread-safe - can be called from strategy thread
    void log(const QString& message, int level = 1) const;

  private:
    friend class StrategyManager;

    /// @brief Set SDK pointer (called by StrategyManager during initialization)
    void setSdk(StrategySDK* p_sdk)
    {
        m_sdk = p_sdk;
    }

    StrategySDK* m_sdk = nullptr;
};
