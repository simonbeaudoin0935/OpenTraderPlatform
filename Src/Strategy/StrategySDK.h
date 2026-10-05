#pragma once

#include <QString>
#include <QStringList>
#include <QObject>
#include <QFuture>
#include <QVector>
#include <expected>
#include <QPromise>
#include <optional>

#include "Order.h"
#include "Position.h"
#include "PlaceOrder.h"
#include "ClosePositions.h"
#include "CancelOrder.h"
#include "TSClient.h"
#include "StrategyOrderValidator.h"
#include "StrategyConfig.h"
#include "Balance.h"
#include "TimeFrame.h"

/// @brief Log level enumeration
enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error
};

enum class StrategyManualOrderExecutionMode : quint8
{
    Fixed,
    MarketableOnAccept
};

enum class StrategyBracketSide : quint8
{
    Long,
    Short
};

enum class StrategyBracketExecutionPolicy : quint8
{
    Auto,
    VirtualOnly,
    NativeOnly
};

// Forward declaration
class MainAlgo;
class StrategyLogger;
class Bar;

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
    [[nodiscard]] QFuture<std::expected<PlaceOrderResult, TSClient::Error>>
    placeOrder(const PlaceOrderRequest& orderRequest);

    /// @brief Queue an order for manual user confirmation before placement (async)
    /// @param p_strategyRequestID Strategy-defined request identifier for decision callbacks
    /// @param p_orderRequest The order request to place upon user approval
    /// @param p_promptText Optional UI prompt text shown to the user
    /// @return QFuture that resolves when the order is eventually placed or rejected
    [[nodiscard]] QFuture<std::expected<PlaceOrderResult, TSClient::Error>> placeOrderWithUserConfirmation(
        const QString& p_strategyRequestID,
        const PlaceOrderRequest& p_orderRequest,
        const QString& p_promptText = {},
        StrategyManualOrderExecutionMode p_executionMode = StrategyManualOrderExecutionMode::Fixed);

    /// @brief Cancel an order (async)
    /// @param orderID ID of order to cancel (as QString matching TSClient)
    /// @return QFuture that resolves to CancelOrderResult on success, or TSClient::Error on failure
    [[nodiscard]] QFuture<std::expected<CancelOrderResult, TSClient::Error>> cancelOrder(const QString& orderID);

    /// @brief Close claimed-symbol positions for one account (async)
    /// Empty p_symbols means "all claimed symbols" for the provided account.
    /// @param p_accountID Account to flatten positions in
    /// @param p_symbols Optional narrower claimed-symbol subset
    /// @return QFuture resolving to a structured batch result, or a QString error on request rejection
    [[nodiscard]] QFuture<std::expected<ClosePositionsResult, QString>>
    closePositions(const QString& p_accountID, const QStringList& p_symbols = {});

    /// @brief Claim exclusive trading authority over a list of symbols.
    /// Strategies must claim symbols before subscribing to data feeds or placing orders.
    /// The platform grants an approved subset (symbols not already owned by another strategy).
    /// For each approved symbol: data feeds are subscribed and trading authority is granted.
    /// Rejected symbols (already claimed by another strategy) are excluded from the result.
    /// @param symbols List of symbols the strategy wants to trade (e.g., {"NVDA", "SPY"})
    /// @return QFuture resolving to the approved subset of symbols
    [[nodiscard]] QFuture<QStringList> claimSymbols(const QStringList& symbols);

    /// @brief Subscribe to data feed for a symbol (bars, Level2, trades)
    /// In replay mode: validates that data exists for the current replay date/symbol.
    /// In live/sim mode: adds symbol to monitored set (live stream support is a future enhancement).
    /// @param symbol Stock symbol to subscribe to (e.g., "SPY")
    /// @return QFuture<true> if subscription accepted, QFuture<false> if rejected
    [[nodiscard]] QFuture<bool> subscribeToSymbol(const QString& symbol);

    /// @brief Get current positions for this strategy (thread-safe)
    /// @return Vector of positions currently held by this strategy
    [[nodiscard]] QVector<Position> getPositions() const;

    /// @brief Get current orders for this strategy (thread-safe)
    /// @return Vector of active orders placed by this strategy
    [[nodiscard]] QVector<Order> getOrders() const;

    /// @brief Get current account balance (thread-safe)
    /// @return Account balance in dollars
    [[nodiscard]] double getAccountBalance() const;

    // -------------------------------------------------------------------------
    // Internal state update methods — called by the active strategy runtime backend
    // to keep SDK state in sync with order/position/balance events.
    // Not part of the strategy-facing public API.
    // -------------------------------------------------------------------------

    /// @brief Update internal order state (insert or replace)
    /// @note Called from the strategy runtime backend
    void updateOrder(const Order& order);

    /// @brief Update internal position state (insert or replace)
    /// @note Called from the strategy runtime backend
    void updatePosition(const Position& position);

    /// @brief Update internal balance state
    /// @note Called from the strategy runtime backend
    void updateBalance(double balance);

    /// @brief Set the list of symbols this SDK instance has been granted exclusive authority over.
    /// Called by StrategyManager on the strategy thread after claim processing.
    void setClaimedSymbols(const QStringList& symbols);

    /// @brief Clear all claimed symbols (called on strategy stop/unload).
    void clearClaimedSymbols();

    /// @brief Log a message (thread-safe)
    /// Logs are captured per-strategy and can be viewed in the Strategies tab.
    /// @param message Log message
    /// @param level Log level (Debug, Info, Warning, Error)
    void log(const QString& message, LogLevel level = LogLevel::Info);

    /// @brief Emit a chart log marker for a symbol (thread-safe).
    /// A small marker will appear on the chart at the current time for the given symbol.
    /// The user can hover over the marker to read the message.
    /// @param symbol Stock symbol the log is associated with (must be a claimed symbol)
    /// @param message Log message to display in the chart tooltip
    void logToChart(const QString& symbol, const QString& message);

    /// @brief Politely request that the GUI switch the displayed chart symbol (thread-safe).
    /// The platform may ignore this request (for example while strategy manual-confirm mute mode is enabled).
    /// @param symbol Claimed symbol to display
    /// @param reason Optional debug reason for logs
    /// @return true when accepted locally for dispatch
    [[nodiscard]] bool requestChartDisplaySwitch(const QString& symbol, const QString& reason = {});

    /// @brief Upsert a managed bracket order owned by the platform (thread-safe).
    /// @param symbol Claimed symbol
    /// @param accountID Target account
    /// @param side Long/short bracket side
    /// @param stopPrice Stop-loss trigger level
    /// @param takePrice Take-profit trigger level
    /// @param executionPolicy Auto/VirtualOnly/NativeOnly
    /// @param referenceEntryPrice Optional bracket entry reference used for chart-side ratio/stop wheel adjustments
    /// @return true when the request was accepted locally for dispatch
    [[nodiscard]] bool
    upsertManagedBracket(const QString& symbol,
                         const QString& accountID,
                         StrategyBracketSide side,
                         double stopPrice,
                         double takePrice,
                         StrategyBracketExecutionPolicy executionPolicy = StrategyBracketExecutionPolicy::Auto,
                         const std::optional<double>& referenceEntryPrice = std::nullopt);

    /// @brief Cancel a managed bracket for a claimed symbol/account pair (thread-safe).
    /// @param symbol Claimed symbol
    /// @param accountID Target account
    /// @return true when the request was accepted locally for dispatch
    [[nodiscard]] bool cancelManagedBracket(const QString& symbol, const QString& accountID);

    /// @brief Get strategy configuration (thread-safe)
    /// @return Reference to strategy config passed at initialization
    [[nodiscard]] const StrategyConfig& getConfig() const;
    void setConfig(const StrategyConfig& p_config);

    /// @brief Get strategy name (thread-safe)
    /// @return Strategy name from config
    [[nodiscard]] const QString& getStrategyName() const;

    /// @brief Get historical bars for a symbol (blocking)
    /// Fetches bars from the cache or API, waiting synchronously for the result.
    /// @param symbol Stock symbol (e.g., "AAPL")
    /// @param day Trading day (in America/New_York timezone)
    /// @param first Start time of bar range (in America/New_York timezone)
    /// @param last End time of bar range (in America/New_York timezone)
    /// @param tf Timescale of bars to retrieve (default: ONE_MINUTE)
    /// @return Shared vector of bars, or empty vector on error
    [[nodiscard]] std::shared_ptr<QVector<Bar>> getHistoricalBars(const QString& symbol,
                                                                  const QDate& day,
                                                                  const QTime& first,
                                                                  const QTime& last,
                                                                  TimeFrame tf = TimeFrame::ONE_MINUTE);

    /// @brief Get current application time (thread-safe)
    /// Returns the current time in America/New_York timezone
    /// @return Current QDateTime in New York timezone
    [[nodiscard]] QDateTime getCurrentTime() const;

    // -------------------------------------------------------------------------
    // Activity metrics — read-only, thread-safe, updated in real time
    // -------------------------------------------------------------------------

    /// @brief EMA-smoothed trade print rate for a symbol (events per second).
    /// @param symbol Symbol to query (must be a monitored symbol)
    /// @return Trades per second, or 0 if no data yet / symbol unknown
    [[nodiscard]] double getTradeRate(const QString& symbol) const;

    /// @brief EMA-smoothed Level2 update rate for a symbol (events per second).
    /// @param symbol Symbol to query (must be a monitored symbol)
    /// @return L2 updates per second, or 0 if no data yet / symbol unknown
    [[nodiscard]] double getL2UpdateRate(const QString& symbol) const;

    /// @brief Whether a symbol is currently considered active (combined rate above threshold).
    /// Uses hysteresis: becomes active above 0.5 events/sec, inactive below 0.1 events/sec.
    /// @param symbol Symbol to query
    /// @return true if the symbol is producing data at a meaningful rate
    [[nodiscard]] bool isSymbolActive(const QString& symbol) const;

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
    QStringList m_claimedSymbols; // Symbols granted exclusive trading authority
};
