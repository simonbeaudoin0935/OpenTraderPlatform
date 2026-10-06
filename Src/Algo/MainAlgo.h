#pragma once
#include <QLoggingCategory>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QReadWriteLock>
#include <QThread>
#include <QThreadPool>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QVector>
#include <QStringList>
#include <QWaitCondition>
#include <expected>
#include <memory>
#include <atomic>
#include <optional>
#include <variant>

#include "Level2Receiver.h"
#include "BarReceiver.h"
#include "PositionsReceiver.h"
#include "OrdersReceiver.h"
#include "Account.h"
#include "BarCache.h"
#include "TimeFrame.h"
#include "Balance.h"
#include "BarAggregator/BarAggregator.h"
#include "Level2.h"
#include "Trade.h"
#include "LiveBarAccumulator.h"
#include "StrategyManager.h"
#include "PlaybackTypes.h"
#include "ClosePositions.h"
#include "RiskTypes.h"
#include "Misc/CONSTANTS.h"
#include "TSClient.h"       // For TSClient::AuthStateReason enum
#include "OrdersDatabase.h" // For StrategyLogEntry
#include "MarketData/Bars/StreamBars.h"
#include "MarketData/GetQuoteSnapshots/Quote.h"
#include "MarketData/StreamMarketDepthAggregate/StreamMarketDepthAggregate.h"
#include "MarketData/StreamQuote/StreamQuote.h"

Q_DECLARE_LOGGING_CATEGORY(MainAlgoLog)

class OrderEmulator;
class RiskManager;


/**
 * @brief Per-symbol passive actor — holds all data and receivers for one symbol.
 *
 * Thread-pool actor model: events are enqueued thread-safely, then drained by
 * exactly one QThreadPool thread at a time (sequential per symbol, parallel across symbols).
 *
 * Internal signal-slot connections use Qt::DirectConnection so they execute on the
 * pool thread during drain(). External connections (to MainAlgo, GUI, StrategyManager)
 * keep Qt::AutoConnection → become QueuedConnection from the pool thread.
 */
/**
 * @brief Tracks per-symbol market data activity using exponential moving averages.
 *
 * Updated from the SymbolContext drain loop (pool thread) — `m_lastTradeMs` /
 * `m_lastL2Ms` are only ever written from that thread so they need no atomics.
 * The EMA and active flag are atomics so they can be read from any thread
 * (e.g., strategy threads via StrategySDK::getTradeRate).
 */
struct ActivityTracker
{
    /// EMA smoothing factor (≈ weight of the most recent sample)
    static constexpr double kAlpha = 0.1;
    /// Combined rate (trade Hz + L2 Hz) threshold to become active
    static constexpr double kActiveThresholdHz = 0.5;
    /// Combined rate threshold to become inactive (hysteresis)
    static constexpr double kInactiveThresholdHz = 0.1;

    std::atomic<double> m_tradeRateHz{0.0};
    std::atomic<double> m_l2RateHz{0.0};
    std::atomic<bool> m_isActive{false};

    void recordTrade(qint64 p_nowMs)
    {
        updateRate(m_tradeRateHz, m_lastTradeMs, p_nowMs);
        updateActive();
    }

    void recordL2(qint64 p_nowMs)
    {
        updateRate(m_l2RateHz, m_lastL2Ms, p_nowMs);
        updateActive();
    }

    [[nodiscard]] double tradeRateHz() const
    {
        return m_tradeRateHz.load(std::memory_order_relaxed);
    }
    [[nodiscard]] double l2RateHz() const
    {
        return m_l2RateHz.load(std::memory_order_relaxed);
    }
    [[nodiscard]] bool isActive() const
    {
        return m_isActive.load(std::memory_order_relaxed);
    }

  private:
    qint64 m_lastTradeMs = 0; ///< Only written from drain thread — no atomic needed
    qint64 m_lastL2Ms = 0;

    static void updateRate(std::atomic<double>& p_ema, qint64& p_lastMs, qint64 p_nowMs)
    {
        if (p_lastMs > 0 && p_nowMs > p_lastMs)
        {
            const double deltaS = static_cast<double>(p_nowMs - p_lastMs) / 1000.0;
            const double instantHz = 1.0 / deltaS;
            const double newEma = kAlpha * instantHz + (1.0 - kAlpha) * p_ema.load(std::memory_order_relaxed);
            p_ema.store(newEma, std::memory_order_relaxed);
        }
        p_lastMs = p_nowMs;
    }

    void updateActive()
    {
        const double combinedHz =
            m_tradeRateHz.load(std::memory_order_relaxed) + m_l2RateHz.load(std::memory_order_relaxed);
        const bool currently = m_isActive.load(std::memory_order_relaxed);
        if (!currently && combinedHz >= kActiveThresholdHz)
            m_isActive.store(true, std::memory_order_relaxed);
        else if (currently && combinedHz < kInactiveThresholdHz)
            m_isActive.store(false, std::memory_order_relaxed);
    }
};

/**
 * @brief Snapshot of latest market data for GUI pull-based rendering.
 *
 * Written by the drain thread (or bar/aggregator signal handlers) under write lock.
 * Read by the GUI thread at 30 Hz under read lock. Dirty flags allow the GUI
 * to skip unchanged data. Trades are accumulated (not overwritten) so none are lost.
 */
struct DisplaySnapshot
{
    mutable QReadWriteLock lock;

    // Latest 1-minute bar (overwritten each update, GUI sees the most recent)
    std::optional<Bar> latestBar;
    bool barDirty = false;

    // Latest Level 2 depth (overwritten — GUI only needs the current snapshot)
    std::optional<Level2> latestLevel2;
    bool l2Dirty = false;

    // Accumulated trades since last GUI read (GUI drains the vector)
    QVector<Trade> pendingTrades;
    bool tradeDirty = false;

    // Stable recent trade history for control/MCP polling (not drained by the GUI)
    QVector<Trade> recentTrades;

    // Latest bar per higher timeframe (overwritten per TF)
    QMap<TimeFrame, Bar> aggregatorBars;
    bool aggregatorDirty = false;

    // Scheduler replay time pushed from DBClient. Used to wake GUI refreshes while replay runs.
    std::optional<QDateTime> replayTime;
    bool replayTimeDirty = false;

    // Latest replay timestamp actually processed by this SymbolContext on the drain thread.
    std::optional<QDateTime> processedReplayTime;
};

struct LiveStreamRetryState
{
    bool pending = false;
    bool terminal = false;
    int delayMs = StreamConstants::LIVE_RETRY_INITIAL_DELAY_MS;

    [[nodiscard]] std::optional<int> schedule(Stream::StreamError p_reason)
    {
        if (p_reason == Stream::StreamError::BadRequest || p_reason == Stream::StreamError::Forbidden)
        {
            terminal = true;
        }
        if (pending || terminal)
        {
            return std::nullopt;
        }
        pending = true;
        const int delay = delayMs;
        delayMs = qMin(delay * 2, StreamConstants::LIVE_RETRY_MAX_DELAY_MS);
        return delay;
    }
};

class SymbolContext : public QObject
{
    Q_OBJECT

  public:
    explicit SymbolContext(const QString& p_symbol, QObject* p_parent = nullptr);
    ~SymbolContext();

    /// @brief Enqueue a Level2 event for processing (thread-safe, called from any thread)
    void enqueueLevel2(const Level2& p_level2);

    /// @brief Enqueue a Trade event for processing (thread-safe, called from any thread)
    void enqueueTrade(const Trade& p_trade);

    /// @brief Returns true while this symbol still has queued or currently draining replay work.
    [[nodiscard]] bool hasReplayBacklog() const
    {
        return m_draining.load(std::memory_order_acquire) || m_pendingWorkItems.load(std::memory_order_acquire) > 0;
    }

    /// @brief Returns the latest replay timestamp actually processed for this symbol.
    [[nodiscard]] std::optional<QDateTime> getProcessedReplayTime() const
    {
        QReadLocker lock(&m_displaySnapshot.lock);
        return m_displaySnapshot.processedReplayTime;
    }

    QString symbol;
    BarCache barCache;
    BarReceiver barReceiver;
    Level2Receiver m_level2Receiver;
    QPointer<StreamBars> m_streamBars;
    QPointer<StreamMarketDepthAggregate> m_streamMarketDepthAggregate;
    QPointer<StreamQuote> m_streamQuote;
    LiveStreamRetryState m_barStreamRetry;
    LiveStreamRetryState m_depthStreamRetry;
    LiveStreamRetryState m_quoteStreamRetry;
    LiveBarAccumulator m_liveBarAccumulator;    ///< 1-minute bar accumulator (default 60s interval)
    LiveBarAccumulator m_live10sBarAccumulator; ///< 10-second bar accumulator
    BarAggregator m_barAggregator;
    bool m_liveCurrentDayHistoryPrefetchIssued = false;

    /// Reference count: incremented by display claim (+1) and each strategy subscription (+1).
    /// The SymbolContext is destroyed only when this reaches 0.
    int m_refCount = 0;

    /// Per-symbol EMA-based activity metrics, updated in the drain loop.
    ActivityTracker m_activity;

    /// Snapshot of latest data for GUI pull-based rendering (30 Hz).
    /// Written by drain thread under write lock, read by GUI under read lock.
    DisplaySnapshot m_displaySnapshot;

  signals:
    /**
     * @brief Forwarded trade event (for strategy subscriptions)
     * Thread context: Emitted from QThreadPool drain thread
     * @param p_symbol Symbol for the trade
     * @param p_trade The trade data
     */
    void receivedNewTrade(const QString& p_symbol, const Trade& p_trade);

  private:
    using WorkItem = std::variant<Level2, Trade>;

    void drain();
    void processLevel2(const Level2& p_level2);
    void processTrade(const Trade& p_trade);

    QMutex m_queueMutex;
    QQueue<WorkItem> m_queue;
    std::atomic<int> m_pendingWorkItems{0};
    std::atomic<bool> m_draining{false};
    std::atomic<bool> m_destroying{false};
    QWaitCondition m_drainDone;
};

class MainAlgo final : public QObject
{
    Q_OBJECT
  public:
    // Singleton : Instance getter  and delete copy and assignment
    static MainAlgo* getInstance();

    // Singleton : Destroy instance (for cleanup)
    static void destroyInstance();

    Q_DISABLE_COPY(MainAlgo) // Delete copy constructor and assignment operator

    void start();

    void startBalancePolling();
    void stopBalancePolling();
    [[nodiscard]] Balance getCurrentBalance() const;
    [[nodiscard]] QString getActiveAccountId() const;
    [[nodiscard]] QVector<Position> getCurrentPositionsSnapshot() const;
    [[nodiscard]] bool hasManagedBracketForAccountSymbol(const QString& p_accountID, const QString& p_symbol) const;

    /// @brief Get the currently displayed stock symbol
    [[nodiscard]] QString getDisplayedSymbol() const;

    /// @brief Get the currently displayed SymbolContext (for GUI pull-based snapshot reading).
    /// Returns nullptr if no symbol is displayed. The QPointer may auto-null if the context is destroyed.
    [[nodiscard]] QPointer<SymbolContext> getDisplayedSymbolContext() const;

    [[nodiscard]] BarCache::GetBarsResult_t requestHistoricalBarsForSymbol(const QString& p_symbol,
                                                                           QDate p_date,
                                                                           QTime p_first,
                                                                           QTime p_last,
                                                                           TimeFrame p_tf = TimeFrame::ONE_MINUTE);

    BarCache::GetBarsResult_t
    requestMissingBarsDisplayedStock(QDate date, QTime first, QTime last, TimeFrame tf = TimeFrame::ONE_MINUTE);

    /*
     * Strategy order management - called by StrategySDK
     * All methods should be called via QMetaObject::invokeMethod with Qt::QueuedConnection
     */
    /// @brief Get the StrategyManager instance for signal/slot wiring or MainAlgo-thread use.
    [[nodiscard]] StrategyManager* getStrategyManager()
    {
        return m_strategyManager.get();
    }

    /// @brief Thread-safe GUI/API entry point for loading a strategy on the MainAlgo thread.
    [[nodiscard]] std::expected<QString, QString> loadStrategy(const StrategyConfig& p_config);

    /// @brief Thread-safe GUI/API entry point for starting a strategy on the MainAlgo thread.
    [[nodiscard]] QString startStrategy(const QString& p_strategyID);
    [[nodiscard]] QString startQueuedReplayStrategies();
    [[nodiscard]] QString stopStrategy(const QString& p_strategyID);
    [[nodiscard]] QString updateStrategyConfig(const QString& p_strategyID, const StrategyConfig& p_config);
    [[nodiscard]] QString unclaimStrategySymbol(const QString& p_strategyID, const QString& p_symbol);
    [[nodiscard]] QString unclaimAndBlockStrategySymbol(const QString& p_strategyID, const QString& p_symbol);

    /// @brief Thread-safe GUI/API entry point for unloading a strategy on the MainAlgo thread.
    [[nodiscard]] QString unloadStrategy(const QString& p_strategyID);

    /// @brief Thread-safe GUI/API query for whether a strategy is currently running.
    [[nodiscard]] bool isStrategyRunning(const QString& p_strategyID) const;
    [[nodiscard]] StrategyManager::StrategyExecutionState getStrategyExecutionState(const QString& p_strategyID) const;
    [[nodiscard]] StrategyConfig getStrategyConfig(const QString& p_strategyID) const;

    /// @brief Thread-safe GUI/API query for a strategy's open positions.
    [[nodiscard]] QVector<Position> getStrategyOpenPositions(const QString& p_strategyID) const;
    [[nodiscard]] int getStrategyPositionCount(const QString& p_strategyID) const;

    /// @brief Thread-safe GUI/API query for StrategyQuickView symbol state.
    [[nodiscard]] QVector<StrategySymbolViewState> getStrategySymbolViewStates(const QString& p_strategyID) const;

    /// @brief Thread-safe GUI/API query for a strategy's current log buffer.
    [[nodiscard]] std::optional<QVector<StrategyLogMessage>> getStrategyLogMessages(const QString& p_strategyID) const;
    [[nodiscard]] int getPendingManualOrderConfirmationsForStrategy(const QString& p_strategyID) const;

    /// @brief Get next unique requestId for strategy order tracking
    /// @return Next requestId (thread-safe atomic increment)
    [[nodiscard]] uint64_t getNextRequestId();

    /// @brief Close currently open positions for an account using session-aware market/limit orders.
    /// @param p_request Account/symbol scope, extended-hours offset (in cents), and execution mode
    /// @param p_strategyID Strategy owner for resulting orders, or empty for GUI/MCP callers
    /// @return Future resolving to batch close results, or an error if the request could not be dispatched
    [[nodiscard]] QFuture<std::expected<ClosePositionsResult, QString>>
    closePositions(const ClosePositionsRequest& p_request, const QString& p_strategyID = {});

    /// @brief Thread-safe order placement entry point for GUI/MCP callers.
    [[nodiscard]] QFuture<std::expected<PlaceOrderResult, TSClient::Error>>
    placeOrder(const PlaceOrderRequest& p_orderRequest, const QString& p_strategyID = {});

    [[nodiscard]] RiskConfig getRiskConfigForAccount(const QString& p_accountID) const;
    [[nodiscard]] RiskStatusSnapshot getRiskStatusSnapshotForAccount(const QString& p_accountID) const;
    void setRiskConfigForAccount(const QString& p_accountID, const RiskConfig& p_config);
    void resetRiskDayForAccount(const QString& p_accountID);
    void unlockRiskForAccount(const QString& p_accountID);

    /// @brief Process a closePositions request on the MainAlgo thread.
    void processClosePositions(const QString& p_strategyID,
                               const ClosePositionsRequest& p_request,
                               std::shared_ptr<QPromise<std::expected<ClosePositionsResult, QString>>> p_promise);

    /// @brief Process a placeOrder request (MainAlgo thread)
    /// @param p_requestId Unique request ID from StrategySDK
    /// @param p_strategyID ID of strategy placing the order, or empty for non-strategy callers such as MCP
    /// @param p_orderRequest The order details
    /// @param p_promise Promise to resolve when order ACK is received
    void processPlaceOrder(uint64_t p_requestId,
                           const QString& p_strategyID,
                           const PlaceOrderRequest& p_orderRequest,
                           std::shared_ptr<QPromise<std::expected<PlaceOrderResult, TSClient::Error>>> p_promise);

    /// @brief Queue a strategy order for manual user confirmation before placement (MainAlgo thread)
    void processPlaceOrderWithUserConfirmation(
        uint64_t p_requestId,
        const QString& p_strategyID,
        const QString& p_strategyRequestID,
        const PlaceOrderRequest& p_orderRequest,
        const QString& p_promptText,
        StrategyManualOrderExecutionMode p_executionMode,
        std::shared_ptr<QPromise<std::expected<PlaceOrderResult, TSClient::Error>>> p_promise);

    /// @brief Called when TSClient placeOrder future resolves
    /// Routes result to a strategy when applicable and finalizes log binding
    void onOrderResolved(uint64_t p_requestId, const std::expected<PlaceOrderResult, TSClient::Error>& p_result);

    /// @brief Process a strategy cancelOrder request (MainAlgo thread)
    /// @param p_orderID ID of the order to cancel
    /// @param p_promise Promise to resolve when cancellation result is received
    void processCancelOrder(const QString& p_orderID,
                            std::shared_ptr<QPromise<std::expected<CancelOrderResult, TSClient::Error>>> p_promise);

    /// @brief Store a strategy log entry and emit strategyLogEmitted (MainAlgo thread)
    /// @param p_entry Log entry to persist and broadcast
    void processStrategyLog(const StrategyLogEntry& p_entry);
    void processStrategyStatus(const StrategyStatusEntry& p_entry);
    void processStrategyStatusClear(const QString& p_strategyID, const QString& p_symbol);
    void processStrategyChartDisplaySwitchRequest(const QString& p_strategyID,
                                                  const QString& p_symbol,
                                                  const QString& p_reason = {});
    [[nodiscard]] std::optional<StrategyStatusEntry> getLatestStrategyStatusForSymbol(const QString& p_symbol) const;

    /// @brief Upsert or replace a managed bracket for one account+symbol pair (MainAlgo thread).
    void processUpsertManagedBracket(const QString& p_sourceID,
                                     const QString& p_accountID,
                                     const QString& p_symbol,
                                     StrategyBracketSide p_side,
                                     double p_stopPrice,
                                     double p_takePrice,
                                     StrategyBracketExecutionPolicy p_executionPolicy,
                                     const std::optional<double>& p_referenceEntryPrice = std::nullopt);

    /// @brief Cancel one managed bracket for account+symbol (MainAlgo thread).
    void processCancelManagedBracket(const QString& p_sourceID,
                                     const QString& p_accountID,
                                     const QString& p_symbol,
                                     const QString& p_reason = {});

    /// @brief Adjust stop/take levels for an existing managed bracket (MainAlgo thread).
    /// If one optional level is not set, it is left unchanged.
    void processAdjustManagedBracketLevels(const QString& p_accountID,
                                           const QString& p_symbol,
                                           const std::optional<double>& p_stopPrice,
                                           const std::optional<double>& p_takePrice,
                                           const QString& p_reason = {});

    /// @brief Enable one-way stop-loss lock so stops can only move in the risk-reducing direction.
    /// Once enabled, the lock remains active until process restart.
    void activateStopLossTightenOnlyLock();

    /// @brief Subscribe a strategy to data feed for a symbol (MainAlgo thread)
    /// Creates a SymbolContext and registers routing. Data flows automatically via centralized routing.
    /// @param p_strategyID Strategy requesting the subscription
    /// @param p_symbol Symbol to subscribe to
    /// @param p_promise Resolved with true if accepted, false if rejected
    void processSubscribeToSymbol(const QString& p_strategyID,
                                  const QString& p_symbol,
                                  std::shared_ptr<QPromise<bool>> p_promise);

    /// @brief Process a symbol claim request from a strategy.
    /// Delegates to StrategyManager::processClaimSymbols().
    void processClaimSymbols(const QString& p_strategyID,
                             const QStringList& p_symbols,
                             std::shared_ptr<QPromise<QStringList>> p_promise);

    /// @brief Cancel all manual-order confirmations owned by one strategy.
    void cancelManualOrderConfirmationsForStrategy(const QString& p_strategyID,
                                                   const QString& p_reason,
                                                   bool p_resumeReplayIfNeeded = true);

    /// @brief Cancel every queued/active manual-order confirmation.
    void cancelAllManualOrderConfirmations(const QString& p_reason, bool p_resumeReplayIfNeeded = true);

    /// @brief Enable/disable global strategy manual-confirm mute mode.
    /// When enabled, active+queued confirmations are rejected and future requests are auto-rejected.
    void setManualOrderConfirmationsMuted(bool p_muted);

    /// @brief Acquire a SymbolContext for a chart window (creates if needed, increments refCount).
    /// Must be called on the MainAlgo thread (via QMetaObject::invokeMethod).
    /// @param p_symbol Symbol to acquire
    /// @return QPointer to the SymbolContext (may auto-null if destroyed)
    [[nodiscard]] QPointer<SymbolContext> acquireSymbolContext(const QString& p_symbol);

    /// @brief Release one reference on a SymbolContext. Destroys it when refCount reaches 0.
    void releaseSymbolContextRef(const QString& symbol);

    /*
     * Replay mode control - called from MainApp via QMetaObject::invokeMethod
     */
    /// @brief Enter replay mode for specified date and time
    /// @param p_date Date to replay
    /// @param p_startTime Time to start replay
    /// @param p_speed Playback speed
    void enterReplayMode(const QString& p_symbol, QDate p_date, QTime p_startTime, Playback::Speed p_speed);

    /// @brief Enter replay mode and immediately pause after first bar
    /// Used when entering replay mode to pre-populate chart
    void enterReplayModePaused(const QString& p_symbol, QDate p_date, QTime p_startTime, Playback::Speed p_speed);

    /// @brief Exit replay mode and clean up
    void exitReplayMode();

    /// @brief Pause replay playback
    void pauseReplay();

    /// @brief Resume replay playback
    void resumeReplay();

    /// @brief Set replay speed on the fly
    void setReplaySpeed(Playback::Speed p_speed);

    /// @brief Pause live streams (positions/orders) for replay mode
    void pauseLiveStreams();

    /// @brief Stop and destroy the current order/position receivers on the MainAlgo thread.
    void teardownAccountReceivers();

    /// @brief Start replay mode order/position streams with simulated account
    void startReplayOrderStreams();
    void connectReplaySignals(const QString& p_symbol);

    /// @brief Resume live streams after exiting replay mode
    void resumeLiveStreams();

    /// @brief Delete all stock instruments (for clean mode transitions)
    void deleteAllSymbolContext();

    /// @brief Stop all running strategies (for clean mode transitions)
    void stopAllStrategies();

    /// @brief Restore previously loaded strategies from StrategiesState.ini.
    /// Must be called on the MainAlgo thread after all mode transitions are complete.
    void restoreStrategiesState();

    /// @brief Create a stock instrument and set it as displayed
    /// @param p_symbol The stock symbol to create and display
    void createAndSetDisplayedSymbolContext(const QString& p_symbol);

    /// @brief Get replay playback state
    [[nodiscard]] Playback::State getReplayState() const;

    /// @brief Snapshot of per-symbol activity metrics (thread-safe read).
    struct ActivityMetrics
    {
        double tradeRateHz = 0.0; ///< EMA-smoothed trades per second
        double l2RateHz = 0.0;    ///< EMA-smoothed L2 updates per second
        bool isActive = false;    ///< Combined rate above activity threshold
    };

    /// @brief Return current activity metrics for a symbol (thread-safe).
    /// Returns a zeroed ActivityMetrics if the symbol has no SymbolContext.
    [[nodiscard]] ActivityMetrics getActivityMetrics(const QString& p_symbol) const;

    struct MarketDataSnapshot
    {
        QString symbol;
        std::optional<Level2> latestLevel2;
        QVector<Trade> recentTrades;
        std::optional<QDateTime> replayTime;
        ActivityMetrics activity;
    };

    /// @brief Return a market-data snapshot for a symbol, leasing a SymbolContext if needed.
    /// If p_symbol is empty, the currently displayed symbol is used.
    [[nodiscard]] std::expected<MarketDataSnapshot, QString> getMarketDataSnapshot(const QString& p_symbol,
                                                                                   int p_maxTrades);

  signals:
    /**
     * @brief Signal emitted when a new position is received
     * Thread context: Emitted from MainAlgo worker thread
     */
    void receivedNewPosition(QString account, Position position);

    /**
     * @brief Signal emitted when a position is deleted
     * Thread context: Emitted from MainAlgo worker thread
     */
    void positionDeleted(QString account, QString positionID);

    /**
     * @brief Signal emitted when a new order is received
     * Thread context: Emitted from MainAlgo worker thread
     */
    void receivedNewOrder(QString account, Order order);

    /**
     * @brief Signal emitted when TradeStation accounts are received
     * Thread context: Emitted from MainAlgo worker thread
     */
    void tradeStationAccountsReceived(QVector<Account> accounts);

    /**
     * @brief Signal emitted when account balance is updated
     * Thread context: Emitted from MainAlgo worker thread
     */
    void balanceUpdated(Balance balance);

    /**
     * @brief Signal emitted when risk runtime state/config changed for one account.
     * Thread context: Emitted from MainAlgo worker thread, received on Main/GUI thread
     */
    void riskStatusChanged(QString accountID);

    /**
     * @brief Emitted when a strategy calls logToChart() — carries the log entry to the chart.
     * Thread context: Emitted from MainAlgo worker thread
     */
    void strategyLogEmitted(StrategyLogEntry entry);

    /**
     * @brief Emitted when managed bracket state changes for a symbol.
     * Thread context: Emitted from MainAlgo worker thread
     */
    void managedBracketOverlayEmitted(StrategyBracketOverlayEntry entry);

    /**
     * @brief Emitted when managed bracket protection is forcibly dropped after a native failure.
     * Thread context: Emitted from MainAlgo worker thread, received on Main/GUI thread
     */
    void managedBracketProtectionDropped(QString accountID, QString symbol, QString reason);

    /**
     * @brief Emitted when a strategy updates/clears chart status for a symbol.
     * Thread context: Emitted from MainAlgo worker thread
     */
    void strategyStatusEmitted(StrategyStatusEntry entry);

    /**
     * @brief Emitted when a strategy order requires GUI user confirmation before placement.
     * Thread context: Emitted from MainAlgo worker thread, received on Main/GUI thread
     */
    void strategyOrderConfirmationRequested(QString confirmationID,
                                            QString symbol,
                                            QString promptText,
                                            int timeoutSec,
                                            QString accountID,
                                            int quantity,
                                            bool isLongSide,
                                            double referencePrice,
                                            double stopPrice);

    /**
     * @brief Emitted when the active strategy order-confirmation prompt is resolved.
     * Thread context: Emitted from MainAlgo worker thread, received on Main/GUI thread
     */
    void strategyOrderConfirmationResolved(QString confirmationID);

    /**
     * @brief Emitted when a strategy politely requests switching the displayed chart symbol.
     * Thread context: Emitted from MainAlgo worker thread, received on Main/GUI thread
     */
    void strategyDisplaySymbolRequested(QString strategyID, QString symbol, QString reason);

    /**
     * @brief Replay control signals (forwarded from DBClient)
     * Thread context: Emitted from MainAlgo worker thread
     */
    void replayStarted();
    void replayStopped();
    void replayPaused();
    void replayResumed();
    void replayEndReached();

  public slots:
    void onTradeStationAuthStateChanged(bool isAuthenticated, TSClient::AuthStateReason reason, const QString& message);
    void onSelectDisplayedStock(const QString& symbol);
    void onStrategyOrderConfirmationDecision(const QString& p_confirmationID,
                                             bool p_accepted,
                                             double p_overrideStopPrice = 0.0);
    void onStrategyOrderConfirmationRejectAndBlockSymbol(const QString& p_confirmationID);

  private slots:
    void onThreadStarted();

    void onReceivedNewPosition(const QString& account, Position position);
    void onPositionDeleted(const QString& account, const QString& positionID);
    void onLoadedPositionsFromDatabase(const QString& account, QMap<QString, Position> positions);
    void onLoadedOrdersFromDatabase(const QString& account,
                                    QMap<QString, std::tuple<Order, std::optional<qint64>>> p_ordersById);
    void onReceivedNewOrder(const QString& account, Order order);

    void onReceivedAsyncGetAccounts(const QVector<Account>& results);

    void onBalanceReceived(const QVector<Balance>& results);
    void requestBalance();

    // Handle replay end - pause heartbeat timers
    void onReplayEndReached();
    void onManualOrderConfirmationTimeout();
    void onReplayResumedForDeferredClosePositions();

    // Forward higher-TF bar aggregator events to DisplaySnapshot
    // (Removed — snapshot writes are now handled inside SymbolContext)

    // Snapshot writers — populate DisplaySnapshot from incoming data
    // (Removed — snapshot writes are now handled inside SymbolContext)
    void onReplayTimeReceived(const QDateTime& time);

  public:
    // Direct cross-thread routing: called from DBClient thread via DirectConnection.
    // Uses a read lock on m_symbolContextsLock; SymbolContext::enqueue* are independently
    // thread-safe, so no further locking is needed inside them.
    void routeBar(const QString& p_symbol, const Bar& p_bar);
    void routeLevel2(const QString& p_symbol, const Level2& p_level2);
    void routeQuote(const QString& p_symbol, const Quote& p_quote);
    void routeTrade(const QString& p_symbol, const Trade& p_trade);

  private:
    static MainAlgo* m_instance;
    explicit MainAlgo(); // Singleton : private constructor
    ~MainAlgo();

    QThread thread;

    QMap<QString, QPointer<SymbolContext>> m_symbolContexts;
    mutable QReadWriteLock m_symbolContextsLock; ///< Guards m_symbolContexts for cross-thread reads
    QPointer<SymbolContext> m_currentDisplayedSymbolContext;

    PositionsReceiver* m_positionReceiver = nullptr; // Qt parent-child ownership (parent is 'this')
    OrdersReceiver* m_orderReceiver = nullptr;       // Qt parent-child ownership (parent is 'this')
    bool positionStreamStarted = false;
    bool orderStreamStarted = false;

    bool m_havePastSuccessfulExchanges = false;
    std::atomic<bool> m_isShuttingDown{false};
    std::atomic<uint64_t> m_accountsRequestGeneration{0};

    Account m_activeAccount;
    Balance m_currentBalance;
    QMap<QString, Position> m_currentPositions; // positionID -> latest non-historical position snapshot
    struct QuoteTradeFingerprint
    {
        QDateTime timestamp;
        double price = 0.0;
        unsigned int size = 0;
    };
    QHash<QString, QuoteTradeFingerprint> m_lastQuoteTradeBySymbol;

    std::unique_ptr<QTimer> m_balancePollingTimer;

    bool m_balancePollingStarted = false;

    // Strategy order tracking - all accessed from MainAlgo thread
    std::unique_ptr<StrategyManager> m_strategyManager;

    struct DeferredOrderUpdate
    {
        QString account;
        Order order;
    };

    std::atomic<uint64_t> m_requestIdCounter{0};
    QMap<uint64_t, std::shared_ptr<QPromise<std::expected<PlaceOrderResult, TSClient::Error>>>> m_pendingOrderPromises;
    QMap<uint64_t, QString>
        m_requestIdToStrategyId; // Temporary mapping until OrderID known; empty = non-strategy caller
    QMap<uint64_t, PlaceOrderRequest>
        m_requestIdToPlacedOrderRequest;        // Request snapshot for synthetic fallback order updates
    QMap<uint64_t, QString> m_pendingOrderLogs; // Temporary: requestId → strategyLog until OrderID known
    QMap<QString, QString> m_orderIdToLog;      // Permanent: OrderID → strategyLog (until order received via stream)
    QMap<QString, DeferredOrderUpdate>
        m_deferredOrderUpdates;              // First stream update replayed once a log binding wins the ACK race
    QMap<QString, Order> m_latestOrdersById; // Latest seen order updates keyed by order_id
    QMap<QString, QString> m_orderMappings;  // OrderID → StrategyID (strategy-owned orders only)
    QMap<QString, QSet<QString>>
        m_pendingCloseOrderIdsByPositionId; // PositionID → close-order IDs awaiting terminal stream updates
    struct PendingClosedPositionReconciliation
    {
        Position position;
        QString accountID;
        QDateTime closedTimestamp;
    };
    QMap<QString, PendingClosedPositionReconciliation>
        m_pendingClosedPositionsByPositionId; // PositionID → closed position snapshots waiting for late order updates

    struct ManagedBracket
    {
        QString sourceID;
        QString accountID;
        QString symbol;
        StrategyBracketOverlayEntry::Side side = StrategyBracketOverlayEntry::Side::Long;
        double stopPrice = 0.0;
        double takePrice = 0.0;
        double referenceEntryPrice = 0.0;
        StrategyBracketExecutionPolicy requestedExecutionPolicy = StrategyBracketExecutionPolicy::Auto;
        bool usingNativeOrders = false;
        bool triggered = false;
        QString triggerReason;
        int protectedQuantity = 0;
        bool nativeFailureDropLatched = false;
        QString nativeStopOrderID;
        QString nativeTakeOrderID;
        QString nativeOcaGroupName;
        bool virtualExitOrderPending = false;
        QString virtualExitOrderID;
        bool virtualExitReplaceInProgress = false;
        QDateTime virtualExitLastSubmitTime;
        QDateTime virtualExitLastCancelRequestTime;
        QDateTime armTimestamp;
    };

    QMap<QString, ManagedBracket> m_managedBrackets; // key=accountId + '\n' + symbol
    bool m_stopLossTightenOnlyLockActive = false;
    QMap<QString, StrategyStatusEntry> m_strategyStatusesBySymbol; // key=symbol (upper)
    std::unique_ptr<QTimer> m_managedBracketMonitorTimer;
    std::unique_ptr<RiskManager> m_riskManager;
    QMap<uint64_t, bool> m_requestIdToRiskEntryCandidate;
    QMap<QString, bool> m_orderIdToRiskEntryCandidate;
    QSet<QString> m_countedRiskEntryFillOrderIds;

    static QString managedBracketKey(const QString& p_accountID, const QString& p_symbol);
    void emitManagedBracketOverlay(const ManagedBracket& p_bracket, bool p_clear);
    void monitorManagedBrackets();
    void updateManagedBracketQuantity(ManagedBracket& p_bracket);
    bool submitManagedBracketNativeOrders(ManagedBracket& p_bracket);
    bool submitManagedBracketNativeLeg(ManagedBracket& p_bracket, bool p_takeLeg);
    void cancelManagedBracketNativeOrders(ManagedBracket& p_bracket);
    [[nodiscard]] bool shouldDropManagedBracketAfterNativeFailure(const ManagedBracket& p_bracket) const;
    void handleManagedBracketNativeFailure(const QString& p_key, ManagedBracket& p_bracket, const QString& p_reason);
    bool submitManagedBracketVirtualExitOrder(ManagedBracket& p_bracket);
    [[nodiscard]] std::optional<Level2> resolveManagedBracketPricingLevel2(const ManagedBracket& p_bracket,
                                                                           QString* p_source = nullptr) const;
    std::optional<double> managedBracketTriggerPrice(const ManagedBracket& p_bracket) const;
    void handleManagedBracketOrderUpdate(const Order& p_order);
    [[nodiscard]] int getSignedNetPositionSharesForAccountSymbol(const QString& p_accountID,
                                                                 const QString& p_symbol) const;
    [[nodiscard]] int getOpenPositionCountForAccount(const QString& p_accountID) const;
    [[nodiscard]] std::optional<double> resolveOrderRiskReferencePrice(const PlaceOrderRequest& p_orderRequest) const;
    [[nodiscard]] std::optional<double> resolveOrderRiskStopPrice(const PlaceOrderRequest& p_orderRequest) const;
    void refreshRiskStatusForAccount(const QString& p_accountID);
    void prunePendingClosedPositionReconciliation();
    bool reconcileCloseOrderToFilled(const QString& p_defaultAccount,
                                     const QString& p_positionId,
                                     const Position& p_closedPosition,
                                     const QDateTime& p_closedTimestamp,
                                     Order p_order,
                                     const QString& p_reconciliationContext);
    void emitSyntheticOrderAckIfMissing(const PlaceOrderRequest& p_orderRequest, const QString& p_orderID);
    void reconcilePendingEntryOrderFromPosition(const QString& p_account, const Position& p_position);

    struct ManualOrderConfirmationRequest
    {
        uint64_t requestId = 0;
        QString confirmationID;
        QString strategyID;
        QString strategyRequestID;
        PlaceOrderRequest orderRequest;
        QString promptText;
        StrategyManualOrderExecutionMode executionMode = StrategyManualOrderExecutionMode::Fixed;
        std::shared_ptr<QPromise<std::expected<PlaceOrderResult, TSClient::Error>>> promise;
        int timeoutSec = 0;
        double userOverrideStopPrice = 0.0;
    };

    struct DeferredClosePositionsRequest
    {
        QString strategyID;
        ClosePositionsRequest request;
        std::shared_ptr<QPromise<std::expected<ClosePositionsResult, QString>>> promise;
    };

    QQueue<DeferredClosePositionsRequest> m_deferredClosePositionsRequests;

    QQueue<ManualOrderConfirmationRequest> m_pendingManualOrderConfirmations;
    std::optional<ManualOrderConfirmationRequest> m_activeManualOrderConfirmation;
    QSet<QString> m_blockedManualConfirmationSymbols; // Uppercase symbols rejected with block shortcut (Shift+N)
    bool m_manualOrderConfirmationsMuted = false;
    std::unique_ptr<QTimer> m_manualOrderConfirmationTimer;
    int m_manualOrderConfirmationRemainingMs = 0;
    bool m_manualOrderConfirmationCountdownPaused = false;
    bool m_manualConfirmationFrontendAvailable = false;

    void activateNextManualOrderConfirmationIfIdle();
    void finalizeActiveManualOrderConfirmation(StrategyManualOrderDecision p_decision,
                                               const QString& p_reason,
                                               bool p_resumeReplayIfNeeded);
    void finishManualOrderConfirmationRequest(const ManualOrderConfirmationRequest& p_request,
                                              StrategyManualOrderDecision p_decision,
                                              const QString& p_reason,
                                              bool p_resumeReplayIfNeeded);
    void beginManualConfirmationReplaySpeedOverrideIfNeeded();
    void endManualConfirmationReplaySpeedOverrideIfIdle();
    void applyReplaySpeedInternal(Playback::Speed p_speed);
    void cancelDeferredClosePositionsRequests(const QString& p_reason);
    void notifyStrategyManualOrderDecision(const ManualOrderConfirmationRequest& p_request,
                                           StrategyManualOrderDecision p_decision,
                                           const QString& p_reason);
    [[nodiscard]] std::expected<PlaceOrderRequest, QString>
    resolveManualOrderRequestOnAccept(const ManualOrderConfirmationRequest& p_request);
    void pauseActiveManualOrderConfirmationCountdown();
    void resumeActiveManualOrderConfirmationCountdown();
    void resetActiveManualOrderConfirmationCountdown();
    static QString defaultManualOrderPrompt(const PlaceOrderRequest& p_orderRequest);

    // Replay state (set in enterReplayMode/enterReplayModePaused, used by strategy subscriptions)
    QDate m_replayDate;
    QTime m_replayStartTime;
    Playback::Speed m_replaySpeed = Playback::Speed::Normal;
    bool m_manualConfirmationReplaySpeedOverrideActive = false;
    std::optional<Playback::Speed> m_manualConfirmationReplaySpeedRestoreTarget;

    std::unique_ptr<QTimer> m_controlSymbolLeaseCleanupTimer;
    QMap<QString, QDateTime> m_controlSymbolLeaseExpirations;

    /// @brief Wire a SymbolContext's bar-close events to the OrderEmulator for PnL updates.
    /// Safe to call multiple times (uses UniqueConnection internally).
    void connectBarCloseToOrderEmulator(SymbolContext* p_sc, OrderEmulator* p_emulator);
    void subscribeLiveSymbol(SymbolContext* p_symbolContext);
    void scheduleLiveStreamRetry(SymbolContext* p_symbolContext,
                                 LiveStreamRetryState& p_state,
                                 Stream::StreamError p_reason,
                                 const QString& p_message);
    void subscribeExistingLiveSymbols();

    [[nodiscard]] std::expected<QPointer<SymbolContext>, QString> leaseControlSymbolContext(const QString& p_symbol);
    void pruneExpiredControlSymbolLeases();
};
