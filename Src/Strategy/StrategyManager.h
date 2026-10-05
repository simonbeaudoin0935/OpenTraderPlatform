#pragma once

#include <QObject>
#include <QThread>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <memory>
#include <expected>

#include "StrategySDK.h"
#include "StrategyRuntimeBackend.h"
#include "StrategyConfig.h"
#include "StrategySymbolViewState.h"

#include "StrategyLogger.h"
#include "Balance.h"
#include "Assume.h"

class MainAlgo;
class SymbolContext;

/*
 * StrategyManager - Orchestrates external strategy lifecycle and execution
 *
 * Owned by MainAlgo via std::unique_ptr. Manages:
 * - Loading/unloading external strategy processes
 * - Creating StrategySDK instances per strategy
 * - Routing bars, market depth, orders, fills, and balances into process backends
 * - Tracking active strategies and their resources
 * - Order/position isolation between strategies
 *
 * Threading Model:
 * - StrategyManager itself lives in MainAlgo thread
 * - Each strategy instance is supervised as its own child process
 * - All cross-thread communication via Qt::QueuedConnection signals
 */
class StrategyManager final : public QObject
{
    Q_OBJECT

  public:
    enum class StrategyExecutionState : quint8
    {
        Loaded,
        Primed,
        Running,
        Paused,
        Stopped
    };

    /// @brief Create StrategyManager
    /// @param p_mainAlgo MainAlgo dispatcher instance (for routing operations)
    explicit StrategyManager(MainAlgo* p_mainAlgo);
    ~StrategyManager();

    Q_DISABLE_COPY(StrategyManager)

    /*
     * Load and initialize an external strategy runtime
     *
     * Creates:
     * - StrategySDK instance
     * - Process runtime backend
     * - Signal connections for data delivery
     *
     * @param p_config - StrategyConfig with executable path, symbols, etc.
     * @return Strategy instance ID on success, error on failure
     */
    [[nodiscard]] std::expected<QString, QString> loadStrategy(const StrategyConfig& p_config);

    /*
     * Unload an external strategy by ID
     *
     * - Stops the supervised process
     * - Cleans up StrategySDK instance
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
     * Stop a strategy process while keeping its loaded configuration/instance.
     *
     * - Sends stop to the process and waits for exit
     * - Clears symbol claims
     * - Keeps strategy available for Start / Edit / Unload
     *
     * @param p_strategyID - Strategy instance ID
     * @return Error message on failure, empty string on success
     */
    [[nodiscard]] QString stopStrategy(const QString& p_strategyID);

    /*
     * Update the stored configuration of a loaded strategy instance.
     * The strategy must be in a non-running state.
     *
     * @param p_strategyID - Strategy instance ID
     * @param p_config - Updated strategy configuration
     * @return Error message on failure, empty string on success
     */
    [[nodiscard]] QString updateStrategyConfig(const QString& p_strategyID, const StrategyConfig& p_config);

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
    [[nodiscard]] StrategyExecutionState getStrategyExecutionState(const QString& p_strategyID) const;

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
     * Get StrategyQuickView state for all currently claimed symbols of a strategy.
     * Returns empty vector if strategy not found.
     */
    [[nodiscard]] QVector<StrategySymbolViewState> getStrategySymbolViewStates(const QString& p_strategyID) const;

    /*
     * Connect a specific symbol's data sources to a strategy backend.
     * Called by MainAlgo when a strategy calls subscribeToSymbol().
     *
     * @param p_strategyID Strategy requesting the subscription
     * @param p_symbol Symbol to subscribe to
     * @param p_instrument SymbolContext for the symbol (nullptr = use displayed-stock signals)
     */
    void connectSymbolToStrategy(const QString& p_strategyID, const QString& p_symbol, SymbolContext* p_instrument);

    /*
     * Process a symbol claim request from a strategy.
     * Called by MainAlgo::processClaimSymbols() on the MainAlgo thread.
     *
     * - Grants exclusive authority over symbols not already claimed by another strategy.
     * - For each approved symbol: subscribes data feeds and updates SDK's m_claimedSymbols.
     * - Returns the approved subset via the promise.
     * - Emits symbolsClaimed(strategyID, activeClaimedSymbols) for StrategyQuickView.
     *
     * @param p_strategyID Strategy requesting the claim
     * @param p_symbols Symbols the strategy wants to claim
     * @param p_promise Resolved with the approved subset
     */
    void processClaimSymbols(const QString& p_strategyID,
                             const QStringList& p_symbols,
                             std::shared_ptr<QPromise<QStringList>> p_promise);

    /*
     * Remove one claimed symbol from a strategy.
     *
     * - Optional: block re-claim for this strategy session.
     * - Rejects if the strategy still has a non-flat position on the symbol.
     * - Updates symbol registry, SDK claim list, and StrategyQuickView state.
     *
     * @param p_strategyID Strategy that currently owns the symbol
     * @param p_symbol Symbol to remove from claims
     * @param p_blockForSession When true, deny re-claim by this strategy until stop/unload
     * @return Error message on failure, empty string on success
     */
    [[nodiscard]] QString unclaimSymbol(const QString& p_strategyID, const QString& p_symbol, bool p_blockForSession);

    /*
     * Release all symbols claimed by a strategy.
     * Called on strategy stop and unload to free the registry entries.
     *
     * @param p_strategyID Strategy whose claims are being released
     */
    void releaseSymbols(const QString& p_strategyID);

    /*
     * Publish a manual-order confirmation decision to one strategy runtime.
     * Called by MainAlgo when a queued user confirmation is accepted/rejected/timed-out/cancelled.
     */
    void publishManualOrderDecision(const QString& p_strategyID,
                                    const QString& p_requestID,
                                    const QString& p_symbol,
                                    StrategyManualOrderDecision p_decision,
                                    const QString& p_reason);

    /*
     * Restore previously loaded strategies from StrategiesState.ini.
     * Called once during startup (MainAlgo::onThreadStarted) after all data
     * source connections are established.
     * Failures (missing executable, corrupt config) are logged as warnings and skipped.
     */
    void restoreStrategiesState();
    [[nodiscard]] QString startQueuedReplayStrategies();

  public slots:
    /*
     * Called when an order update should be routed to a specific strategy only
     * Used by MainAlgo when the owning strategy is known from m_orderMappings
     */
    void onOrderUpdatedForStrategy(const QString& p_strategyID, const Order& p_order);

    /*
     * Called when MainAlgo receives a new position
     * Dispatches to all strategies via backend publishing
     */
    void onMainAlgoPositionUpdated(const QString& p_account, const Position& p_position);

    /*
     * Called when MainAlgo receives balance update (from TSClient)
     * Dispatches to all strategies via backend publishing
     */
    void onMainAlgoBalanceUpdated(const Balance& p_balance);
    void onReplayPaused();
    void onReplayResumed();

  private slots:
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
     * state: current execution state
     * errorMessage: non-empty if there's an error
     */
    void strategyStatusChanged(const QString& strategyID, StrategyExecutionState state, const QString& errorMessage);

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

    /**
     * @brief Emitted when a strategy releases its claim on a symbol.
     * Thread context: Emitted from MainAlgo thread (releaseSymbols)
     * @param symbol The symbol being released
     */
    void symbolReleased(const QString& symbol);

  private:
    enum class StrategyState
    {
        LOADED,  // Strategy loaded but not yet started
        QUEUED,  // Strategy is primed to start on first replay play
        RUNNING, // Strategy process is running while replay/live is active
        PAUSED,  // Strategy process is started but replay is currently paused
        STOPPED  // Strategy has been stopped/unloaded
    };

  private:
    struct StrategyInstance
    {
        struct StrategySymbolLedger
        {
            StrategySymbolViewState viewState;
            bool hasSeenRawPosition = false;
            qint64 lastRawQuantity = 0;
            double lastMarkPrice = 0.0;
            std::optional<double> latestExecutionPrice;
        };

        QString strategyID;                                 // Unique ID for this instance
        StrategyConfig config;                              // Configuration
        std::unique_ptr<IStrategyRuntimeBackend> p_backend; // Current runtime backend
        QVector<QString> monitoredSymbols;                  // Symbols being watched
        QSet<QString> blockedSymbols;                       // Session blocklist for re-claim attempts
        StrategyState state = StrategyState::LOADED;        // Tracks: LOADED → RUNNING → STOPPED

        // Track per-symbol signal connections for explicit disconnection on unload
        QVector<QMetaObject::Connection> m_connections;
        QMap<QString, StrategySymbolLedger> symbolLedgers;
    };

    MainAlgo* m_mainAlgo;
    QMap<QString, StrategyInstance*> m_strategies;
    QMap<QString, QString> m_symbolRegistry; ///< symbol → ownerStrategyID (exclusive claim registry)
    quint64 m_nextSymbolActivitySequence = 1;

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

    StrategyInstance::StrategySymbolLedger*
    ensureSymbolLedger(StrategyInstance* p_instance, const QString& p_symbol, int p_originalOrder = -1);
    void
    applyLedgerFillDelta(StrategyInstance::StrategySymbolLedger& p_ledger, qint64 p_deltaQuantity, double p_fillPrice);
    [[nodiscard]] QString queueStrategyStart(StrategyInstance* p_instance);
    [[nodiscard]] QString startStrategyInternal(StrategyInstance* p_instance, bool p_startPaused);
    void setStrategyState(StrategyInstance* p_instance, StrategyState p_state, const QString& p_errorMessage = {});
    [[nodiscard]] static StrategyExecutionState toExecutionState(StrategyState p_state);
    void pauseReplayStrategies();
    void resumeReplayStrategies();

    /*
     * Persist the current set of loaded strategies to StrategiesState.ini.
     * Called after every load/start/unload operation.
     */
    void persistStrategiesState();
};
