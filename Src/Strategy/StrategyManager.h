#pragma once

#include <QObject>
#include <QThread>
#include <QMap>
#include <QString>
#include <QStringList>
#include <memory>
#include <expected>

#include "StrategyBase.h"
#include "StrategySDK.h"
#include "StrategyLoader.h"
#include "StrategyConfig.h"
#include "StrategyConfigLoader.h"
#include "StrategyRegistry.h"
#include "StrategyLogger.h"
#include "StrategySignalHandler.h"
#include "Balance.h"
#include "Assume.h"

class MainAlgo;
class StockInstruments;
class ReplayEngine;

/// @brief Adapter to call StrategyBase methods from Qt slots
/// Lives on strategy's thread and provides thread-safe callback invocation
class StrategyCallbackAdapter : public QObject
{
    Q_OBJECT

  public:
    explicit StrategyCallbackAdapter(StrategyBase* p_strategy, StrategySDK* p_sdk, const QVector<QString>& p_symbols)
        : m_strategy(p_strategy), m_sdk(p_sdk), m_monitoredSymbols(p_symbols)
    {
    }

  public slots:
    void onBar(const QString& symbol, const Bar& bar) const
    {
        // Only call if strategy monitors this symbol
        if (m_monitoredSymbols.contains(symbol))
        {
            ASSUME_DIFF(m_strategy, nullptr);
            m_strategy->onBar(bar);
        }
    }

    void onLevel2(const QString& symbol, const Level2& level2) const
    {
        if (m_monitoredSymbols.contains(symbol))
        {
            ASSUME_DIFF(m_strategy, nullptr);
            m_strategy->onLevel2(level2);
        }
    }

    void onTrade(const QString& symbol, const Trade& trade) const
    {
        if (m_monitoredSymbols.contains(symbol))
        {
            ASSUME_DIFF(m_strategy, nullptr);
            m_strategy->onTrade(trade);
        }
    }

    void onOrderUpdated(const Order& order) const
    {
        ASSUME_DIFF(m_strategy, nullptr);
        qDebug() << "[StrategyCallbackAdapter::onOrderUpdated] orderID=" << order.getOrderID()
                 << "status=" << static_cast<int>(order.getOrderStatus());
        // Update SDK state before notifying strategy
        if (m_sdk)
        {
            m_sdk->updateOrder(order);
        }
        m_strategy->onOrderUpdated(order);
    }

    void onPositionUpdated(const Position& position) const
    {
        ASSUME_DIFF(m_strategy, nullptr);
        // Update SDK state before notifying strategy
        if (m_sdk)
        {
            m_sdk->updatePosition(position);
        }
        m_strategy->onPositionUpdated(position);
    }

    void onBalanceUpdated(double balance) const
    {
        ASSUME_DIFF(m_strategy, nullptr);
        // Update SDK state before notifying strategy
        if (m_sdk)
        {
            m_sdk->updateBalance(balance);
        }
        m_strategy->onBalanceUpdated(balance);
    }

    void callOnStop() const
    {
        ASSUME_DIFF(m_strategy, nullptr);
        m_strategy->onStop();
    }

    /// @brief Add a symbol to the monitored set so its data callbacks are forwarded
    /// @note Called from MainAlgo thread via Qt::BlockingQueuedConnection
    void addMonitoredSymbol(const QString& symbol)
    {
        if (!m_monitoredSymbols.contains(symbol))
        {
            m_monitoredSymbols.append(symbol);
        }
    }

  private:
    StrategyBase* m_strategy;
    StrategySDK* m_sdk;
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
     * Start a previously loaded strategy
     *
     * - Changes state from LOADED to RUNNING
     * - Starts strategy thread
     * - Calls strategy's onStart() callback
     *
     * @param p_strategyID - Strategy instance ID (from loadStrategy)
     * @return Error message on failure, empty string on success
     */
    [[nodiscard]] QString startStrategy(const QString& p_strategyID);

    /*
     * Mark a strategy as crashed/failed
     * Called by signal handlers when strategy thread crashes
     *
     * - Sets error state
     * - Emits strategyStatusChanged signal
     * - Updates UI to show ERROR status
     *
     * @param p_strategyID - Strategy instance ID
     * @param p_errorMessage - Error message describing the crash
     */
    void markStrategyFailed(const QString& p_strategyID, const QString& p_errorMessage);

    /*
     * Stop and unload all running strategies
     * Used during mode transitions (Live <-> Replay) for clean separation
     */
    void stopAllStrategies();

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

    /*
     * Get the strategy registry (available strategies from configs)
     */
    [[nodiscard]] StrategyRegistry* getRegistry()
    {
        return m_registry.get();
    }

    /*
     * Get the strategy registry (const version)
     */
    [[nodiscard]] const StrategyRegistry* getRegistry() const
    {
        return m_registry.get();
    }

    /*
     * Get strategy logger by strategy ID
     */
    [[nodiscard]] StrategyLogger* getStrategyLogger(const QString& p_strategyID);

    /*
     * Get strategy logger (const version)
     */
    [[nodiscard]] const StrategyLogger* getStrategyLogger(const QString& p_strategyID) const;

    /*
     * Get number of open positions for a strategy
     * Returns 0 if strategy not found
     */
    [[nodiscard]] int getStrategyPositionCount(const QString& p_strategyID) const;

    /*
     * Get thread ID (TID) for a strategy's execution thread
     * Returns 0 if strategy not found
     */
    [[nodiscard]] qint64 getStrategyThreadId(const QString& p_strategyID) const;

    /*
     * Get current balance for a strategy
     * Returns 0 if strategy not found
     */
    [[nodiscard]] double getStrategyBalance(const QString& p_strategyID) const;

    /*
     * Get recent orders for a strategy (last 20)
     * Returns empty vector if strategy not found
     */
    [[nodiscard]] QVector<Order> getStrategyRecentOrders(const QString& p_strategyID, int limit = 20) const;

    /*
     * Get open positions for a strategy
     * Returns empty vector if strategy not found
     */
    [[nodiscard]] QVector<Position> getStrategyOpenPositions(const QString& p_strategyID) const;

    /*
     * Connect a specific symbol's data sources to a strategy's adapter.
     * Called by MainAlgo when a strategy calls subscribeToSymbol().
     *
     * @param p_strategyID Strategy requesting the subscription
     * @param p_symbol Symbol to subscribe to
     * @param p_instrument StockInstruments for the symbol (nullptr = use displayed-stock signals)
     * @param p_replayEngine Secondary ReplayEngine for the symbol (nullptr if not applicable)
     */
    void connectSymbolToStrategy(const QString& p_strategyID,
                                 const QString& p_symbol,
                                 StockInstruments* p_instrument,
                                 ReplayEngine* p_replayEngine);

    /*
     * Process a symbol claim request from a strategy.
     * Called by MainAlgo::processClaimSymbols() on the MainAlgo thread.
     *
     * - Grants exclusive authority over symbols not already claimed by another strategy.
     * - For each approved symbol: subscribes data feeds and updates SDK's m_claimedSymbols.
     * - Returns the approved subset via the promise.
     * - Emits symbolsClaimed(strategyID, approvedSymbols) for StrategyQuickView.
     *
     * @param p_strategyID Strategy requesting the claim
     * @param p_symbols Symbols the strategy wants to claim
     * @param p_promise Resolved with the approved subset
     */
    void processClaimSymbols(const QString& p_strategyID,
                             const QStringList& p_symbols,
                             std::shared_ptr<QPromise<QStringList>> p_promise);

    /*
     * Release all symbols claimed by a strategy.
     * Called on strategy stop and unload to free the registry entries.
     *
     * @param p_strategyID Strategy whose claims are being released
     */
    void releaseSymbols(const QString& p_strategyID);

    /*
     * Restore previously loaded strategies from StrategiesState.ini.
     * Called once during startup (MainAlgo::onThreadStarted) after all data
     * source connections are established.
     * Failures (missing .so, corrupt config) are logged as warnings and skipped.
     */
    void restoreStrategiesState();

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
    void onLevel2Received(const QString& p_symbol, const Level2& p_level2);

    /*
     * Called when an order is updated (any status change)
     * Routes to strategy that placed the order
     */
    void onOrderUpdated(const Order& p_order);

    /*
     * Called when an order update should be routed to a specific strategy only
     * Used by MainAlgo when the owning strategy is known from m_orderMappings
     */
    void onOrderUpdatedForStrategy(const QString& p_strategyID, const Order& p_order);

    /*
     * Called when MainAlgo receives a new order (ignores account parameter)
     */
    void onMainAlgoOrderUpdated(const QString& p_account, const Order& p_order);

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
     * Called when MainAlgo receives a new position (ignores account parameter)
     */
    void onMainAlgoPositionUpdated(const QString& p_account, const Position& p_position);

    /*
     * Called when balance is updated (double version for strategies)
     */
    void onBalanceUpdated(double p_newBalance);

    /*
     * Called when MainAlgo receives balance update (from TSClient)
     * Converts Balance object to double and broadcasts to strategies
     */
    void onMainAlgoBalanceUpdated(const Balance& p_balance);

  private slots:
    /*
     * Private slot called from signal handler to mark strategy as failed
     * Used with QMetaObject::invokeMethod from signal handler context
     */
    void markStrategyFailedFromSignal(const QString& p_strategyID, const QString& p_errorMessage);

  signals:
    /*
     * Emitted when a strategy is successfully loaded
     * strategyID: unique ID for this strategy instance
     * name: strategy name from config
     */
    void strategyLoaded(const QString& strategyID, const QString& name);

    /*
     * Emitted when a strategy is unloaded
     * strategyID: unique ID of the strategy being removed
     */
    void strategyUnloaded(const QString& strategyID);

    /*
     * Emitted when strategy status changes
     * strategyID: unique ID of the strategy
     * isRunning: true if running, false if stopped/error
     * errorMessage: non-empty if there's an error
     */
    void strategyStatusChanged(const QString& strategyID, bool isRunning, const QString& errorMessage);

    /*
     * Emitted when a strategy's balance updates
     * strategyID: unique ID of the strategy
     * newBalance: updated account balance for this strategy
     */
    void strategyBalanceUpdated(const QString& strategyID, double newBalance);

    /*
     * Emitted when a strategy has been granted exclusive authority over symbols.
     * strategyID: unique ID of the strategy
     * claimedSymbols: symbols approved by the platform (subset of what was requested)
     * Thread context: Emitted from MainAlgo thread
     */
    void symbolsClaimed(const QString& strategyID, const QStringList& claimedSymbols);

  private:
    enum class StrategyState
    {
        LOADED,  // Strategy loaded but not yet started
        RUNNING, // Strategy thread is running
        STOPPED  // Strategy has been stopped/unloaded
    };

  private:
    struct StrategyInstance
    {
        QString strategyID;                          // Unique ID for this instance
        StrategyConfig config;                       // Configuration
        StrategyLoader::LoadedPlugin plugin;         // Loaded .so plugin
        StrategyBase* p_strategy;                    // Strategy instance
        StrategySDK* p_sdk;                          // SDK instance
        StrategyCallbackAdapter* p_adapter;          // Callback adapter (lives on strategy thread)
        QThread m_thread;                            // Dedicated thread
        QVector<QString> monitoredSymbols;           // Symbols being watched
        std::unique_ptr<StrategyLogger> p_logger;    // Strategy logger (owned)
        Qt::HANDLE threadHandle;                     // Native thread handle for stats reading
        StrategyState state = StrategyState::LOADED; // Tracks: LOADED → RUNNING → STOPPED
    };

    MainAlgo* m_mainAlgo;
    QMap<QString, StrategyInstance*> m_strategies;
    QMap<QString, QString> m_symbolRegistry;      ///< symbol → ownerStrategyID (exclusive claim registry)
    std::unique_ptr<StrategyRegistry> m_registry; ///< Registry of available strategies

    /// Guards persistStrategiesState() from firing during destructor teardown
    /// or bulk stopAllStrategies() mode transitions.  Re-enabled at the start
    /// of restoreStrategiesState() so that individual loadStrategy() calls
    /// triggered by restore DO write back to the file.
    bool m_persistEnabled = true;

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

    /*
     * Persist the current set of loaded strategies to StrategiesState.ini.
     * Called after every load/start/unload operation.
     */
    void persistStrategiesState();
};
