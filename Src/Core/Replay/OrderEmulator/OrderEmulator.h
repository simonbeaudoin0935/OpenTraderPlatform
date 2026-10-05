#pragma once

#include <QJsonObject>
#include <QLoggingCategory>
#include <QMap>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QVector>

#include "Order.h"
#include "PlaceOrder.h"
#include "Position.h"
#include "Level2.h"

Q_DECLARE_LOGGING_CATEGORY(OrderEmulatorLog)

/**
 * @class OrderEmulator
 * @brief Emulates order execution and position tracking in replay mode
 *
 * OrderEmulator receives order requests from TSClient (via MockNetworkAccessManager)
 * and simulates realistic order lifecycle:
 * - Reception delay (100-500ms random, scaled by replay speed)
 * - Order book walking for market orders
 * - Limit order fill monitoring
 * - Position tracking with P&L calculation
 *
 * Key responsibilities:
 * - Receive and validate order requests
 * - Simulate reception/execution delays
 * - Track latest market depth snapshots per symbol
 * - Generate order status updates (OPN → FLL)
 * - Track positions as orders fill
 * - Emit updates for streams to broadcast
 *
 * Threading: Created in TSClient thread, runs in TSClient thread
 */
class OrderEmulator : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Construct OrderEmulator
     * @param p_parent Parent object (typically TSClient)
     */
    explicit OrderEmulator(QObject* p_parent = nullptr);
    ~OrderEmulator() override;

    Q_DISABLE_COPY_MOVE(OrderEmulator)

    /**
     * @brief Place an order in the emulator
     * @param p_request The order request
     * @return The canonical order ID assigned by the emulator (use this as the OrderID everywhere)
     *
     * Starts reception delay timer. Order status updates emitted via signals using the returned ID.
     */
    [[nodiscard]] QString placeOrder(const PlaceOrderRequest& p_request);

    /**
     * @brief Cancel an open order
     * @param p_orderID The order ID to cancel
     * @param p_requestID Unique request ID for tracking
     *
     * If order is open (OPN), cancels it. If already filled or not found, returns error.
     */
    void cancelOrder(const QString& p_orderID, const QString& p_requestID);

    /**
     * @brief Update market depth snapshot for a symbol
     * @param p_symbol Stock ticker symbol
     * @param p_depth Latest market depth data
     *
     * Called by ReplayEngine on each depth tick. Used for:
     * - Calculating fill prices for market orders
     * - Monitoring limit order fill conditions
     * - Updating position P&L in real-time
     */
    void updateMarketDepth(const QString& p_symbol, const Level2& p_depth);

    /**
     * @brief Update bar close price for a symbol
     * @param p_symbol Stock ticker symbol
     * @param p_close Bar close price
     *
     * Called when a new bar is received. Used as "Last" price in mark-to-market calculation.
     */
    void updateBarClose(const QString& p_symbol, double p_close);

    /**
     * @brief Get all current orders
     * @return Vector of all orders (pending, open, filled)
     */
    [[nodiscard]] QVector<Order> getOrders() const;

    /**
     * @brief Get all current positions
     * @return Vector of all positions
     */
    [[nodiscard]] QVector<Position> getPositions() const;

    /**
     * @brief Set replay speed for delay scaling
     * @param p_speedPercent Speed as percentage (100 = normal, -1 = as fast as possible)
     */
    void setReplaySpeed(int p_speedPercent);

    /**
     * @brief Clear all orders and positions (called on replay stop)
     */
    void clear();

    /**
     * @brief Pause all pending order timers
     */
    void pause();

    /**
     * @brief Resume all paused order timers
     */
    void resume();

    /**
     * @brief Get the simulated account ID
     */
    [[nodiscard]] static QString getSimulatedAccountID()
    {
        return QStringLiteral("SIM123456");
    }

    /**
     * @brief Get the current simulated balance
     */
    [[nodiscard]] double getBalance() const
    {
        return m_balance;
    }

    /**
     * @brief Set the starting balance
     */
    void setStartingBalance(double p_balance)
    {
        m_balance = p_balance;
    }

    /**
     * @brief Get the total realized profit/loss from all closed positions
     */
    [[nodiscard]] double getRealizedProfitLoss() const
    {
        return m_realizedProfitLoss;
    }

    /**
     * @brief Get realized P&L summed from closed positions
     * Uses accumulated total for reliability (QMap had unexplained corruption issues)
     */
    [[nodiscard]] double getClosedPositionsPnL() const
    {
        return m_totalClosedPnL;
    }

  signals:
    /**
     * @brief Emitted when an order status changes
     * Thread context: Emitted from TSClient worker thread
     * @param p_jsonData JSON-formatted order data (for stream injection)
     */
    void orderStatusUpdate(const QByteArray& p_jsonData);

    /**
     * @brief Emitted when a position is created or updated
     * Thread context: Emitted from TSClient worker thread
     * @param p_jsonData JSON-formatted position data (for stream injection)
     */
    void positionUpdate(const QByteArray& p_jsonData);

  private:
    /**
     * @brief Pending order awaiting reception delay
     */
    struct PendingOrder
    {
        PlaceOrderRequest request;
        QString orderID; // Canonical ID assigned by generateOrderID() at placeOrder() call time
        qint64 submitTimeMs;
        qint64 remainingDelayMs = 0; // For pause/resume
    };

    /**
     * @brief Generate a unique order ID
     */
    [[nodiscard]] QString generateOrderID();

    /**
     * @brief Calculate reception delay based on replay speed
     * @return Delay in milliseconds
     */
    [[nodiscard]] int calculateReceptionDelay() const;

    /**
     * @brief Calculate execution delay based on replay speed
     * @return Delay in milliseconds
     */
    [[nodiscard]] int calculateExecutionDelay() const;

    /**
     * @brief Check if a limit order can fill at current depth
     * @param p_order The order to check
     * @param p_depth Current market depth
     * @return true if order can fill
     */
    [[nodiscard]] bool canFillLimitOrder(const Order& p_order, const Level2& p_depth) const;

    /**
     * @brief Check if a stop order can fill at current depth
     * @param p_order The order to check
     * @param p_depth Current market depth
     * @return true if order can fill
     */
    [[nodiscard]] bool canFillStopOrder(const Order& p_order, const Level2& p_depth) const;

    /**
     * @brief Calculate fill price for a market order (book walking)
     * @param p_order The order
     * @param p_depth Current market depth
     * @return Weighted average fill price
     */
    [[nodiscard]] double calculateMarketOrderFillPrice(const Order& p_order, const Level2& p_depth) const;

    /**
     * @brief Fill an order and update position
     * @param p_orderJson The order JSON to fill
     * @param p_fillPrice Fill price
     */
    void fillOrder(const QJsonObject& p_orderJson, double p_fillPrice);

    /**
     * @brief Update position after order fill
     * @param p_filledOrder The filled order
     * @param p_fillPrice The fill price
     */
    void updatePosition(const Order& p_filledOrder, double p_fillPrice);

    /**
     * @brief Recalculate position P&L and emit update
     * @param p_symbol Stock ticker symbol
     *
     * Uses TradeStation mark-to-market formula:
     * - If Last is between bid/ask: use Last
     * - Otherwise: use closest bid or ask
     */
    void recalculatePositionPnL(const QString& p_symbol);

    /**
     * @brief Convert order to JSON for stream emission
     */
    [[nodiscard]] QByteArray orderToJson(const Order& p_order) const;

    /**
     * @brief Convert order to QJsonObject (for internal use)
     */
    [[nodiscard]] QJsonObject orderToJsonObject(const Order& p_order) const;

    /**
     * @brief Convert position to JSON for stream emission
     */
    [[nodiscard]] QByteArray positionToJson(const Position& p_position) const;

    /**
     * @brief Validate an order request
     * @param p_request The request to validate
     * @param p_errorMessage Output: validation error message
     * @return true if valid, false otherwise
     */
    [[nodiscard]] bool validateOrder(const PlaceOrderRequest& p_request, QString& p_errorMessage) const;

    /**
     * @brief Create a QJsonObject for an Order
     * @param p_orderID The order ID
     * @param p_request The original request
     * @param p_status The order status
     * @return QJsonObject ready for Order constructor
     */
    [[nodiscard]] QJsonObject
    createOrderJson(const QString& p_orderID, const PlaceOrderRequest& p_request, Order::Status p_status) const;

  private slots:
    void onReceptionDelayElapsed();
    void onExecutionDelayElapsed();

  private:
    // Order tracking
    QMap<QString, Order> m_openOrders;   // orderID → Order (OPN status)
    QMap<QString, Order> m_filledOrders; // orderID → Order (FLL/CAN/REJ status)

    // Position tracking (ID-based for unique round trips)
    QMap<QString, Position> m_positions; // positionID → Position

    // Internal position data for calculation
    struct PositionData
    {
        QString positionID;
        QString symbol;
        int quantity = 0;
        double averagePrice = 0.0;
        double realizedPnL = 0.0; // Accumulated realized P&L for this position
        bool isLong = true;       // true = Long position, false = Short position
    };
    QMap<QString, PositionData> m_positionData;      // positionID → PositionData
    QMap<QString, QString> m_symbolToActivePosition; // symbol → active positionID

    // Track closed positions' realized P&L (positionID → realized P&L at close)
    // Note: QMap had unexplained corruption issues, so we use m_totalClosedPnL as primary source
    QMap<QString, double> m_closedPositionPnL;

    // Total realized P&L from all closed positions (primary source for balance calculation)
    double m_totalClosedPnL = 0.0;

    // Position ID counter - starts at 60000000 to generate 8-digit numeric IDs matching TradeStation format
    qint64 m_nextPositionID = 60000000;

    // Market depth snapshots
    QMap<QString, Level2> m_depthSnapshots; // symbol → latest depth

    // Bar close prices (for mark-to-market calculation)
    QMap<QString, double> m_latestBarClose; // symbol → latest bar close price

    // P&L throttle: last time positionUpdate was emitted per symbol (ms since epoch)
    // Prevents flooding the position pipeline on high-frequency Level2 ticks
    static constexpr qint64 PNL_THROTTLE_MS = 100;
    QMap<QString, qint64> m_lastPnLEmit; // symbol → last emit timestamp (ms)

    // Pending orders (awaiting reception delay)
    QVector<PendingOrder> m_pendingOrders;
    QTimer m_receptionTimer;

    // Orders awaiting execution delay (after OPN emitted)
    // Store as JSON since Order doesn't have default constructor
    struct ExecutingOrder
    {
        QJsonObject orderJson;
        double fillPrice = 0.0;
        qint64 remainingDelayMs = 0;
    };
    QVector<ExecutingOrder> m_executingOrders;
    QTimer m_executionTimer;

    // Order ID generation
    // Order ID counter - starts at 900000000 to generate 9-digit numeric IDs matching TradeStation format
    quint64 m_nextOrderID = 900000000;

    // Replay speed (percentage, -1 = as fast as possible)
    int m_replaySpeedPercent = 100;

    // Simulated account balance
    double m_balance = 100000.0;

    // Realized profit/loss (from closed positions)
    double m_realizedProfitLoss = 0.0; // Default $100k

    // Constants for delays (in ms at 100% speed)
    static constexpr int MIN_RECEPTION_DELAY_MS = 100;
    static constexpr int MAX_RECEPTION_DELAY_MS = 500;
    static constexpr int MIN_EXECUTION_DELAY_MS = 10;
    static constexpr int MAX_EXECUTION_DELAY_MS = 50;
};
