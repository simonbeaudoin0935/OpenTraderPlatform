#pragma once

#include <QString>
#include <QObject>
#include <QFuture>
#include <QVector>
#include <expected>

#include "Order.h"
#include "Position.h"
#include "PlaceOrder.h"
#include "CancelOrder.h"
#include "TSClient.h"

/// @brief Log level enumeration
enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error
};

/// @brief Strategy configuration passed to strategy on creation
struct StrategyConfig
{
    QString name;        ///< Strategy name
    QString soPath;      ///< Path to .so file (for info only)
    QStringList symbols; ///< Symbols to monitor
    int positionSize;    ///< Default position size (shares)
    double riskLimit;    ///< Maximum loss per trade
};

/// @brief High-level SDK provided to strategies
/// Strategies use this to place orders, query positions, and log messages.
/// All methods are thread-safe - strategies call them from their own QThread
/// and the SDK serializes requests to MainAlgo thread for execution.
class StrategySDK : public QObject
{
    Q_OBJECT

  public:
    virtual ~StrategySDK() = default;

    /// @brief Place an order (async)
    /// Strategy populates a PlaceOrderRequest with desired order details.
    /// MainAlgo will validate it (check risk limits, portfolio constraints) and route to TSClient.
    /// @param orderRequest The order request to place
    /// @return QFuture that resolves to PlaceOrderResult on success, or TSClient::Error on failure
    /// @note Returns immediately without blocking. Check future for result when ready.
    [[nodiscard]]
    virtual QFuture<std::expected<PlaceOrderResult, TSClient::Error>>
    placeOrder(const PlaceOrderRequest& orderRequest) = 0;

    /// @brief Cancel an order (async)
    /// @param orderID ID of order to cancel (as QString matching TSClient)
    /// @return QFuture that resolves to CancelOrderResult on success, or TSClient::Error on failure
    [[nodiscard]]
    virtual QFuture<std::expected<CancelOrderResult, TSClient::Error>> cancelOrder(const QString& orderID) = 0;

    /// @brief Get current positions for this strategy (thread-safe)
    /// @return Vector of positions currently held by this strategy
    [[nodiscard]]
    virtual QVector<Position> getPositions() const = 0;

    /// @brief Get current orders for this strategy (thread-safe)
    /// @return Vector of active orders placed by this strategy
    [[nodiscard]]
    virtual QVector<Order> getOrders() const = 0;

    /// @brief Get current account balance (thread-safe)
    /// @return Account balance in dollars
    [[nodiscard]]
    virtual double getAccountBalance() const = 0;

    /// @brief Log a message (thread-safe)
    /// Logs are captured per-strategy and can be viewed in the Strategies tab.
    /// @param message Log message
    /// @param level Log level (Debug, Info, Warning, Error)
    virtual void log(const QString& message, LogLevel level = LogLevel::Info) = 0;

    /// @brief Get strategy configuration (thread-safe)
    /// @return Reference to strategy config passed at initialization
    [[nodiscard]]
    virtual const StrategyConfig& getConfig() const = 0;

    /// @brief Get strategy name (thread-safe)
    /// @return Strategy name from config
    [[nodiscard]]
    virtual const QString& getStrategyName() const = 0;
};
