#pragma once

#include <QString>
#include <string>

#include "Bar.h"
#include "MarketDepthQuote.h"
#include "Order.h"
#include "Position.h"

/// @brief Abstract base class for trading strategies
/// Strategies inherit from this class and implement the callback methods
/// to react to market data and order/position updates.
///
/// Strategy instances run on dedicated QThreads and receive callbacks
/// via Qt signals. No manual thread locking is needed - Qt handles all
/// cross-thread communication.
class Strategy
{
  public:
    virtual ~Strategy() = default;

    /// @brief Called when a new bar is received for the monitored symbol
    /// @param bar The new bar (OHLCV data)
    virtual void onBar(const Bar& bar) = 0;

    /// @brief Called when market depth (L2) data is received
    /// @param depth The market depth quote with bid/ask levels
    virtual void onMarketDepth(const MarketDepthQuote& depth) = 0;

    /// @brief Called when an order placed by this strategy is updated
    /// Receives all order status changes (ACK, OPN, FLL, REJ, CAN, etc.)
    /// Check order.status to determine the specific state change.
    /// This replaces separate onOrderFilled/onOrderCancelled/onOrderRejected callbacks.
    /// @param order The updated order with current status
    virtual void onOrderUpdated(const Order& order) = 0;

    /// @brief Called when an order placed by this strategy is filled
    /// @param order The filled order with updated status
    /// @deprecated Use onOrderUpdated(const Order&) instead to handle all status changes
    virtual void onOrderFilled(const Order& order) = 0;

    /// @brief Called when an order placed by this strategy is cancelled
    /// @param order The cancelled order
    /// @param reason Optional reason for cancellation
    /// @deprecated Use onOrderUpdated(const Order&) instead to handle all status changes
    virtual void onOrderCancelled(const Order& order, const std::string& reason) = 0;

    /// @brief Called when an order placed by this strategy is rejected
    /// @param order The rejected order
    /// @param reason Reason for rejection
    /// @deprecated Use onOrderUpdated(const Order&) instead to handle all status changes
    virtual void onOrderRejected(const Order& order, const std::string& reason) = 0;

    /// @brief Called when a position changes (updated, closed, or modified)
    /// @param position The updated position
    virtual void onPositionUpdated(const Position& position) = 0;

    /// @brief Called when account balance is updated
    /// @param newBalance The new account balance
    virtual void onBalanceUpdated(double newBalance) = 0;

    /// @brief Called when an error occurs in the strategy or platform
    /// @param error Error message
    virtual void onError(const std::string& error) = 0;
};
