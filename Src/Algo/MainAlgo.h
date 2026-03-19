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
#include <QTimer>
#include <QVector>
#include <QStringList>
#include <QWaitCondition>
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
#include "TSClient.h"       // For TSClient::AuthStateReason enum
#include "OrdersDatabase.h" // For StrategyLogEntry

Q_DECLARE_LOGGING_CATEGORY(MainAlgoLog)

class OrderEmulator;


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

    // Latest bar per higher timeframe (overwritten per TF)
    QMap<TimeFrame, Bar> aggregatorBars;
    bool aggregatorDirty = false;

    // Current replay time
    std::optional<QDateTime> replayTime;
    bool replayTimeDirty = false;
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

    QString symbol;
    BarCache barCache;
    BarReceiver barReceiver;
    Level2Receiver m_level2Receiver;
    LiveBarAccumulator m_liveBarAccumulator;    ///< 1-minute bar accumulator (default 60s interval)
    LiveBarAccumulator m_live10sBarAccumulator; ///< 10-second bar accumulator
    BarAggregator m_barAggregator;

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
    /// @brief Get the StrategyManager instance
    [[nodiscard]] StrategyManager* getStrategyManager()
    {
        return &m_strategyManager;
    }

    /// @brief Get next unique requestId for strategy order tracking
    /// @return Next requestId (thread-safe atomic increment)
    [[nodiscard]] uint64_t getNextRequestId();

    /// @brief Process a strategy placeOrder request (MainAlgo thread)
    /// @param p_requestId Unique request ID from StrategySDK
    /// @param p_strategyID ID of strategy placing the order
    /// @param p_orderRequest The order details
    /// @param p_promise Promise to resolve when order ACK is received
    void processPlaceOrder(uint64_t p_requestId,
                           const QString& p_strategyID,
                           const PlaceOrderRequest& p_orderRequest,
                           std::shared_ptr<QPromise<std::expected<PlaceOrderResult, TSClient::Error>>> p_promise);

    /// @brief Called when TSClient placeOrder future resolves
    /// Routes result to strategy and emits GUI signal if displayed stock
    void onOrderResolved(uint64_t p_requestId, const std::expected<PlaceOrderResult, TSClient::Error>& p_result);

    /// @brief Process a strategy cancelOrder request (MainAlgo thread)
    /// @param p_orderID ID of the order to cancel
    /// @param p_promise Promise to resolve when cancellation result is received
    void processCancelOrder(const QString& p_orderID,
                            std::shared_ptr<QPromise<std::expected<CancelOrderResult, TSClient::Error>>> p_promise);

    /// @brief Store a strategy log entry and emit strategyLogEmitted (MainAlgo thread)
    /// @param p_entry Log entry to persist and broadcast
    void processStrategyLog(const StrategyLogEntry& p_entry);

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
     * @brief Emitted when a strategy calls logToChart() — carries the log entry to the chart.
     * Thread context: Emitted from MainAlgo worker thread
     */
    void strategyLogEmitted(StrategyLogEntry entry);

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

  private slots:
    void onThreadStarted();

    void onReceivedNewPosition(const QString& account, Position position);
    void onPositionDeleted(const QString& account, const QString& positionID);
    void onLoadedPositionsFromDatabase(const QString& account, QMap<QString, Position> positions);
    void onReceivedNewOrder(const QString& account, Order order);

    void onReceivedAsyncGetAccounts(const QVector<Account>& results);

    void onBalanceReceived(const QVector<Balance>& results);
    void requestBalance();

    // Handle replay end - pause heartbeat timers
    void onReplayEndReached();

    // Forward higher-TF bar aggregator events to DisplaySnapshot
    // (Removed — snapshot writes are now handled inside SymbolContext)

    // Snapshot writers — populate DisplaySnapshot from incoming data
    // (Removed — snapshot writes are now handled inside SymbolContext)
    void onReplayTimeReceived(const QDateTime& time);

  public:
    // Direct cross-thread routing: called from DBClient thread via DirectConnection.
    // Uses a read lock on m_symbolContextsLock; SymbolContext::enqueue* are independently
    // thread-safe, so no further locking is needed inside them.
    void routeLevel2(const QString& p_symbol, const Level2& p_level2);
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

    Account m_activeAccount;
    Balance m_currentBalance;

    std::unique_ptr<QTimer> m_balancePollingTimer;

    bool m_balancePollingStarted = false;

    // Strategy order tracking - all accessed from MainAlgo thread
    StrategyManager m_strategyManager;

    std::atomic<uint64_t> m_requestIdCounter{0};
    QMap<uint64_t, std::shared_ptr<QPromise<std::expected<PlaceOrderResult, TSClient::Error>>>> m_pendingOrderPromises;
    QMap<uint64_t, QString> m_requestIdToStrategyId; // Temporary mapping until OrderID known
    QMap<uint64_t, QString> m_pendingOrderLogs;      // Temporary: requestId → strategyLog until OrderID known
    QMap<QString, QString> m_orderIdToLog;  // Permanent: OrderID → strategyLog (until order received via stream)
    QMap<QString, QString> m_orderMappings; // OrderID → StrategyID (permanent)

    // Replay state (set in enterReplayMode/enterReplayModePaused, used by strategy subscriptions)
    QDate m_replayDate;
    QTime m_replayStartTime;
    Playback::Speed m_replaySpeed = Playback::Speed::Normal;

    /// @brief Wire a SymbolContext's bar-close events to the OrderEmulator for PnL updates.
    /// Safe to call multiple times (uses UniqueConnection internally).
    void connectBarCloseToOrderEmulator(SymbolContext* p_sc, OrderEmulator* p_emulator);
};
