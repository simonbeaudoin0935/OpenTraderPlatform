#pragma once

#include <QString>
#include <QObject>
#include <QFuture>
#include <QVector>
#include <expected>
#include <QPromise>

#include "Order.h"
#include "Position.h"
#include "PlaceOrder.h"
#include "CancelOrder.h"
#include "TSClient.h"
#include "StrategyOrderValidator.h"
#include "StrategyConfig.h"
#include "Balance.h"

/// @brief Log level enumeration
enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error
};

// Forward declaration
class MainAlgo;
class StrategyLogger;

/// @brief High-level SDK provided to strategies
/// Strategies use this to place orders, query positions, and log messages.
/// All methods are thread-safe - strategies call them from their own QThread
/// and the SDK serializes requests to MainAlgo thread for execution.
class StrategySDK : public QObject
{
    Q_OBJECT

  public:
    /// @brief Create SDK instance for a strategy
    /// @param p_mainAlgo Pointer to MainAlgo dispatcher
    /// @param p_strategyID Unique ID of the strategy using this SDK
    /// @param p_config Strategy configuration
    /// @param p_logger Pointer to strategy logger (can be nullptr)
    /// @note Lives on strategy's dedicated QThread
    explicit StrategySDK(MainAlgo* p_mainAlgo,
                         const QString& p_strategyID,
                         const StrategyConfig& p_config,
                         StrategyLogger* p_logger = nullptr);
    ~StrategySDK() override = default;

    /// @brief Place an order (async)
    /// Strategy populates a PlaceOrderRequest with desired order details.
    /// MainAlgo will validate it (check risk limits, portfolio constraints) and route to TSClient.
    /// @param orderRequest The order request to place
    /// @return QFuture that resolves to PlaceOrderResult on success, or TSClient::Error on failure
    /// @note Returns immediately without blocking. Check future for result when ready.
    [[nodiscard]]
    QFuture<std::expected<PlaceOrderResult, TSClient::Error>> placeOrder(const PlaceOrderRequest& orderRequest);

    /// @brief Cancel an order (async)
    /// @param orderID ID of order to cancel (as QString matching TSClient)
    /// @return QFuture that resolves to CancelOrderResult on success, or TSClient::Error on failure
    [[nodiscard]]
    QFuture<std::expected<CancelOrderResult, TSClient::Error>> cancelOrder(const QString& orderID);

    /// @brief Subscribe to data feed for a symbol (bars, Level2, trades)
    /// In replay mode: validates that data exists for the current replay date/symbol.
    /// In live/sim mode: adds symbol to monitored set (live stream support is a future enhancement).
    /// @param symbol Stock symbol to subscribe to (e.g., "SPY")
    /// @return QFuture<true> if subscription accepted, QFuture<false> if rejected
    [[nodiscard]]
    QFuture<bool> subscribeToSymbol(const QString& symbol);

    /// @brief Get current positions for this strategy (thread-safe)
    /// @return Vector of positions currently held by this strategy
    [[nodiscard]]
    QVector<Position> getPositions() const;

    /// @brief Get current orders for this strategy (thread-safe)
    /// @return Vector of active orders placed by this strategy
    [[nodiscard]]
    QVector<Order> getOrders() const;

    /// @brief Get current account balance (thread-safe)
    /// @return Account balance in dollars
    [[nodiscard]]
    double getAccountBalance() const;

    // -------------------------------------------------------------------------
    // Internal state update methods — called by StrategyCallbackAdapter on the
    // strategy thread to keep SDK state in sync with order/position/balance events.
    // Not part of the strategy-facing public API.
    // -------------------------------------------------------------------------

    /// @brief Update internal order state (insert or replace)
    /// @note Called from StrategyCallbackAdapter on strategy thread
    void updateOrder(const Order& order);

    /// @brief Update internal position state (insert or replace)
    /// @note Called from StrategyCallbackAdapter on strategy thread
    void updatePosition(const Position& position);

    /// @brief Update internal balance state
    /// @note Called from StrategyCallbackAdapter on strategy thread
    void updateBalance(double balance);

    /// @brief Log a message (thread-safe)
    /// Logs are captured per-strategy and can be viewed in the Strategies tab.
    /// @param message Log message
    /// @param level Log level (Debug, Info, Warning, Error)
    void log(const QString& message, LogLevel level = LogLevel::Info);

    /// @brief Get strategy configuration (thread-safe)
    /// @return Reference to strategy config passed at initialization
    [[nodiscard]]
    const StrategyConfig& getConfig() const;

    /// @brief Get strategy name (thread-safe)
    /// @return Strategy name from config
    [[nodiscard]]
    const QString& getStrategyName() const;

    /// @brief Get historical bars for a symbol (blocking)
    /// Fetches bars from the cache or API, waiting synchronously for the result.
    /// @param symbol Stock symbol (e.g., "AAPL")
    /// @param day Trading day (in America/New_York timezone)
    /// @param first Start time of bar range (in America/New_York timezone)
    /// @param last End time of bar range (in America/New_York timezone)
    /// @return Shared vector of bars, or empty vector on error
    [[nodiscard]]
    std::shared_ptr<QVector<Bar>>
    getHistoricalBars(const QString& symbol, const QDate& day, const QTime& first, const QTime& last);

    /// @brief Get current application time (thread-safe)
    /// Returns the current time in America/New_York timezone
    /// @return Current QDateTime in New York timezone
    [[nodiscard]]
    QDateTime getCurrentTime() const;

  private:
    MainAlgo* m_mainAlgo;
    QString m_strategyID;
    StrategyConfig m_config;
    StrategyLogger* m_logger; // Can be nullptr

    // Per-strategy state — updated via updateOrder/updatePosition/updateBalance
    // All accessed on strategy thread (serialized by Qt event loop)
    QVector<Order> m_orders;
    QVector<Position> m_positions;
    double m_balance = 0.0;
};
