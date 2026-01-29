#pragma once

#include <QObject>
#include <QThread>
#include <QMap>
#include <QString>
#include <memory>
#include <expected>

#include "Strategy.h"
#include "StrategySDK.h"
#include "StrategyLoader.h"

class MainAlgo;

/*
 * StrategySDKImpl - Concrete implementation of StrategySDK
 * Delegates SDK method calls to StrategyManager
 * Lives on strategy's dedicated QThread
 */
class StrategySDKImpl : public StrategySDK
{
    Q_OBJECT

  public:
    explicit StrategySDKImpl(class StrategyManager* p_manager);

    QFuture<std::expected<PlaceOrderResult, TSClient::Error>> placeOrder(const PlaceOrderRequest& p_order) override;

    QFuture<std::expected<CancelOrderResult, TSClient::Error>> cancelOrder(const QString& p_orderID) override;

    QVector<Position> getPositions() const override;
    QVector<Order> getOrders() const override;
    double getAccountBalance() const override;

    void log(const QString& p_message, LogLevel p_level = LogLevel::Info) override;

    const StrategyConfig& getConfig() const override;
    const QString& getStrategyName() const override;

  private:
    class StrategyManager* mp_manager;
};

/*
 * StrategyManager - Orchestrates strategy plugin lifecycle and execution
 *
 * Singleton that manages:
 * - Loading/unloading strategy .so plugins
 * - Creating StrategySDK instances per strategy
 * - Spawning dedicated QThread per strategy
 * - Signal connections for data delivery (bars, market depth, orders, fills)
 * - Tracking active strategies and their resources
 * - Order/position isolation between strategies
 *
 * Threading Model:
 * - StrategyManager itself lives in MainAlgo thread
 * - Each strategy instance lives on its own dedicated QThread
 * - Strategy callbacks (onBar, onOrderFilled, etc.) executed on strategy thread
 * - All cross-thread communication via Qt::QueuedConnection signals
 */
class StrategyManager final : public QObject
{
    Q_OBJECT

  public:
    // Singleton access
    static StrategyManager* getInstance();
    static void destroyInstance();

    Q_DISABLE_COPY(StrategyManager)

    /*
     * Initialize StrategyManager with reference to MainAlgo
     * Must be called before loadStrategy()
     *
     * @param p_mainAlgo - MainAlgo singleton instance (for connecting signals)
     */
    void initialize(MainAlgo* p_mainAlgo);

    /*
     * Load and initialize a strategy plugin
     *
     * Creates:
     * - StrategySDK instance
     * - Strategy instance (via factory function)
     * - Dedicated QThread
     * - Signal connections for data delivery
     *
     * @param p_config - StrategyConfig with plugin path, symbols, etc.
     * @return Strategy instance ID on success, error on failure
     */
    [[nodiscard]] std::expected<QString, QString> loadStrategy(const StrategyConfig& p_config);

    /*
     * Unload a strategy plugin by ID
     *
     * - Stops strategy thread with quit/wait/terminate
     * - Calls destroyStrategy factory function
     * - Cleans up StrategySDK instance
     * - Unloads .so file
     *
     * @param p_strategyID - Strategy instance ID (from loadStrategy)
     * @return Error message on failure, empty string on success
     */
    [[nodiscard]] QString unloadStrategy(const QString& p_strategyID);

    /*
     * Get list of active strategy IDs
     */
    [[nodiscard]] QVector<QString> getActiveStrategies() const;

    /*
     * Get strategy configuration
     */
    [[nodiscard]] StrategyConfig getStrategyConfig(const QString& p_strategyID) const;

    /*
     * Get strategy execution state
     */
    [[nodiscard]] bool isStrategyRunning(const QString& p_strategyID) const;

  public slots:
    /*
     * Called when MainAlgo receives a new bar
     * Broadcasts bar to all strategies monitoring that symbol
     */
    void onBarReceived(const QString& p_symbol, const Bar& p_bar);

    /*
     * Called when MainAlgo receives market depth update
     * Broadcasts to all strategies monitoring that symbol
     */
    void onMarketDepthReceived(const QString& p_symbol, const MarketDepthQuote& p_quote);

    /*
     * Called when an order is filled
     * Routes to strategy that placed the order
     */
    void onOrderFilled(const Order& p_order);

    /*
     * Called when an order is cancelled
     * Routes to strategy that placed the order
     */
    void onOrderCancelled(const Order& p_order);

    /*
     * Called when an order is rejected
     * Routes to strategy that placed the order
     */
    void onOrderRejected(const Order& p_order, const QString& p_reason);

    /*
     * Called when a position is updated
     * Routes to all strategies or specific strategy if isolated
     */
    void onPositionUpdated(const Position& p_position);

    /*
     * Called when balance is updated
     */
    void onBalanceUpdated(double p_newBalance);

  private:
    struct StrategyInstance
    {
        QString strategyID;                  // Unique ID for this instance
        StrategyConfig config;               // Configuration
        StrategyLoader::LoadedPlugin plugin; // Loaded .so plugin
        Strategy* p_strategy;                // Strategy instance
        StrategySDK* p_sdk;                  // SDK instance
        QThread m_thread;                    // Dedicated thread
        QVector<QString> monitoredSymbols;   // Symbols being watched
    };

    StrategyManager();
    ~StrategyManager();

    static StrategyManager* s_instance;

    MainAlgo* mp_mainAlgo = nullptr;
    QMap<QString, StrategyInstance*> m_strategies;

    /*
     * Generate unique strategy instance ID
     */
    QString generateStrategyID();

    /*
     * Helper to find strategy instance by ID
     */
    StrategyInstance* findStrategy(const QString& p_strategyID);
    const StrategyInstance* findStrategy(const QString& p_strategyID) const;

    /*
     * Connect StrategyInstance to MainAlgo data sources
     * Sets up signal connections for bars, market depth, orders, etc.
     */
    void connectStrategyToDataSources(StrategyInstance* p_instance);

    /*
     * Disconnect StrategyInstance from data sources
     */
    void disconnectStrategyFromDataSources(StrategyInstance* p_instance);
};
