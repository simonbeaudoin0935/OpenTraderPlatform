#pragma once
#include <QLoggingCategory>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QQueue>
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

class QSocketNotifier;
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

  signals:
    /**
     * @brief Signal emitted when the displayed stock receives a new bar
     * Thread context: Emitted from MainAlgo worker thread
     */
    void displayedStockReceivedNewBar(QString symbol, Bar bar);

    /**
     * @brief Signals emitted when the BarAggregator produces a higher-TF bar for the displayed stock.
     * barUpdated: in-progress (open) bar tick; barClosed: completed bar.
     * Thread context: Emitted from MainAlgo worker thread
     */
    void displayedStockAggregatorBarUpdated(QString symbol, TimeFrame tf, Bar bar);
    void displayedStockAggregatorBarClosed(QString symbol, TimeFrame tf, Bar bar);

    /**
     * @brief Signal emitted when the displayed stock receives a new Level 2 book snapshot
     * Thread context: Emitted from MainAlgo worker thread
     */
    void displayedStockReceivedNewLevel2(QString symbol, Level2 level2);

    /**
     * @brief Signal emitted when the displayed stock receives a new trade
     * Thread context: Emitted from MainAlgo worker thread
     */
    void displayedStockReceivedNewTrade(QString symbol, Trade trade);

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
    void replayTimeUpdated(QDateTime currentTime);
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

    // Handle strategy crash notifications from signal handler pipe
    void onStrategyCrashNotified();

    // Handle replay end - pause heartbeat timers
    void onReplayEndReached();

    // Forward higher-TF bar aggregator events to displayed-stock signals
    void onAggregatorBarUpdated(TimeFrame tf, const Bar& bar);
    void onAggregatorBarClosed(TimeFrame tf, const Bar& bar);

    // GUI throttle gate slots — buffer data when throttle is active, emit immediately otherwise
    void onDisplayedBarReceived(const QString& symbol, const Bar& bar);
    void onDisplayedLevel2Received(const QString& symbol, const Level2& level2);
    void onDisplayedTradeReceived(const QString& symbol, const Trade& trade);
    void onReplayTimeReceived(const QDateTime& time);
    void onGuiThrottleTimerTick();

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
    QReadWriteLock m_symbolContextsLock; ///< Guards m_symbolContexts for cross-thread reads
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

    std::unique_ptr<QSocketNotifier> m_crashNotifier; // Monitor crash pipe from signal handlers
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

    // Handle for the display-symbol trade forwarding lambda so we can disconnect only it
    // (not the permanent onNewTradeReceived routing connection) when switching symbols.
    QMetaObject::Connection m_displayTradeConnection;

    // --- GUI throttle for AsFastAsPossible replay mode ---
    // When active, high-frequency GUI-bound signals are buffered and emitted
    // at a capped rate to prevent flooding the GUI thread's event queue.
    QTimer m_guiThrottleTimer;
    bool m_guiThrottleActive = false;

    void activateGuiThrottle();
    void deactivateGuiThrottle();

    /// @brief Wire a SymbolContext's bar-close events to the OrderEmulator for PnL updates.
    /// Safe to call multiple times (uses UniqueConnection internally).
    void connectBarCloseToOrderEmulator(SymbolContext* p_sc, OrderEmulator* p_emulator);

    /// @brief Release one reference on a SymbolContext. Destroys it when refCount reaches 0.
    void releaseSymbolContextRef(const QString& symbol);

    // Buffered latest state for throttled GUI emission (only latest matters)
    std::optional<std::pair<QString, Bar>> m_pendingBar;
    std::optional<std::pair<QString, Level2>> m_pendingLevel2;
    std::optional<std::pair<QString, Trade>> m_pendingTrade;
    std::optional<QDateTime> m_pendingReplayTime;
    std::optional<std::pair<TimeFrame, Bar>> m_pendingAggregatorBarUpdate;
    QString m_pendingAggregatorSymbol;
};
