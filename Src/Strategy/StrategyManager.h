#pragma once

#include <QObject>
#include <QThread>
#include <QMap>
#include <QString>
#include <memory>
#include <expected>

#include "StrategyBase.h"
#include "StrategySDK.h"
#include "StrategyLoader.h"
#include "StrategyConfig.h"
#include "StrategyConfigLoader.h"

class MainAlgo;

/// @brief Adapter to call StrategyBase methods from Qt slots
/// Lives on strategy's thread and provides thread-safe callback invocation
class StrategyCallbackAdapter : public QObject
{
    Q_OBJECT

  public:
    explicit StrategyCallbackAdapter(StrategyBase* p_strategy, const QVector<QString>& p_symbols)
        : m_strategy(p_strategy), m_monitoredSymbols(p_symbols)
    {
    }

  public slots:
    void onBar(const QString& symbol, const Bar& bar) const
    {
        // Only call if strategy monitors this symbol
        if (m_monitoredSymbols.contains(symbol) && m_strategy)
            m_strategy->onBar(bar);
    }

    void onMarketDepth(const MarketDepthQuote& quote) const
    {
        if (m_strategy)
            m_strategy->onMarketDepth(quote);
    }

    void onOrderUpdated(const Order& order) const
    {
        if (m_strategy)
            m_strategy->onOrderUpdated(order);
    }

    void onPositionUpdated(const Position& position) const
    {
        if (m_strategy)
            m_strategy->onPositionUpdated(position);
    }

    void onBalanceUpdated(double balance) const
    {
        if (m_strategy)
            m_strategy->onBalanceUpdated(balance);
    }

  private:
    StrategyBase* m_strategy;
    QVector<QString> m_monitoredSymbols;
};

/*
 * StrategyManager - Orchestrates strategy plugin lifecycle and execution
 *
 * Owned by MainAlgo via std::unique_ptr. Manages:
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
    /// @brief Create StrategyManager
    /// @param p_mainAlgo MainAlgo dispatcher instance (for routing operations)
    explicit StrategyManager(MainAlgo* p_mainAlgo);
    ~StrategyManager();

    Q_DISABLE_COPY(StrategyManager)

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

    /*
     * Get strategy instance by ID (for direct callback routing)
     */
    [[nodiscard]] StrategyBase* getStrategy(const QString& p_strategyID) const;

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
     * Called when an order is updated (any status change)
     * Routes to strategy that placed the order
     */
    void onOrderUpdated(const Order& p_order);

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
        StrategyBase* p_strategy;            // Strategy instance
        StrategySDK* p_sdk;                  // SDK instance
        StrategyCallbackAdapter* p_adapter;  // Callback adapter (lives on strategy thread)
        QThread m_thread;                    // Dedicated thread
        QVector<QString> monitoredSymbols;   // Symbols being watched
    };

    MainAlgo* m_mainAlgo;
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
