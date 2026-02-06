#pragma once
#include <QLoggingCategory>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QMap>
#include <QTimer>
#include <QVector>
#include <memory>
#include <atomic>

#include "MarketDepthQuoteReceiver.h"
#include "BarReceiver.h"
#include "PositionsReceiver.h"
#include "OrdersReceiver.h"
#include "Account.h"
#include "BarCache.h"
#include "Balance.h"
#include "StrategyManager.h"
#include "Core/Replay/ReplayEngine.h"

Q_DECLARE_LOGGING_CATEGORY(MainAlgoLog)

class QSocketNotifier;


class StockInstruments : public QObject
{

  public:
    explicit StockInstruments(const QString& p_symbol, QObject* p_parent = nullptr);
    ~StockInstruments();

    QString symbol;
    BarCache barCache;
    BarReceiver barReceiver;
    MarketDepthQuoteReceiver marketDepthQuoteReceiver;
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

    BarCache::GetBarsResult_t requestMissingBarsDisplayedStock(QDate date, QTime first, QTime last);

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

    /*
     * Replay mode control - called from MainApp via QMetaObject::invokeMethod
     */
    /// @brief Enter replay mode for specified date and time
    /// @param p_date Date to replay
    /// @param p_startTime Time to start replay
    /// @param p_speed Playback speed
    void enterReplayMode(QDate p_date, QTime p_startTime, ReplayEngine::PlaybackSpeed p_speed);

    /// @brief Enter replay mode and immediately pause after first bar
    /// Used when entering replay mode to pre-populate chart
    void enterReplayModePaused(QDate p_date, QTime p_startTime, ReplayEngine::PlaybackSpeed p_speed);

    /// @brief Exit replay mode and clean up
    void exitReplayMode();

    /// @brief Pause replay playback
    void pauseReplay();

    /// @brief Resume replay playback
    void resumeReplay();

    /// @brief Pause live streams (positions/orders) for replay mode
    void pauseLiveStreams();

    /// @brief Resume live streams after exiting replay mode
    void resumeLiveStreams();

    /// @brief Delete all stock instruments (for clean mode transitions)
    void deleteAllStockInstruments();

    /// @brief Stop all running strategies (for clean mode transitions)
    void stopAllStrategies();

    /// @brief Create a stock instrument and set it as displayed
    /// @param p_symbol The stock symbol to create and display
    void createAndSetDisplayedStockInstrument(const QString& p_symbol);

    /// @brief Get replay engine state
    [[nodiscard]] ReplayEngine::PlaybackState getReplayState() const;

  signals:
    void displayedStockReceivedNewBar(QString symbol, Bar bar);
    void displayedStockReceivedNewMarketDepthQuote(QString symbol,
                                                   MarketDepthQuote quote,
                                                   double bidAskImbalance,
                                                   double bidDWP,
                                                   double askDWP);

    void receivedNewPosition(QString account, Position position);
    void positionDeleted(QString account, QString positionID);
    void receivedNewOrder(QString account, Order order);
    void tradeStationAccountsReceived(QVector<Account> accounts);
    void balanceUpdated(Balance balance);

    // Replay signals (forwarded from ReplayEngine)
    void replayStarted();
    void replayStopped();
    void replayPaused();
    void replayResumed();
    void replayTimeUpdated(QDateTime currentTime);
    void replayEndReached();

  public slots:
    void onTradeStationAuthStateChanged(bool isAuthenticated, const QString& reason);
    void onSelectDisplayedStock(const QString& symbol);

  private slots:
    void onThreadStarted();

    void onReceivedNewPosition(const QString& account, Position position);
    void onPositionDeleted(const QString& account, const QString& positionID);
    void onReceivedNewOrder(const QString& account, Order order);

    void onReceivedAsyncGetAccounts(const QVector<Account>& results);

    void onBalanceReceived(const QVector<Balance>& results);
    void requestBalance();

    // Handle strategy crash notifications from signal handler pipe
    void onStrategyCrashNotified();


  private:
    static MainAlgo* m_instance;
    explicit MainAlgo(); // Singleton : private constructor
    ~MainAlgo();

    QThread thread;

    QMap<QString, QPointer<StockInstruments>> stockInstruments;
    QPointer<StockInstruments> currentDisplayedStockInstrument;

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

    // Replay engine - owned, runs in MainAlgoThread
    ReplayEngine* m_replayEngine = nullptr;

    std::unique_ptr<QSocketNotifier> m_crashNotifier; // Monitor crash pipe from signal handlers
    std::atomic<uint64_t> m_requestIdCounter{0};
    QMap<uint64_t, std::shared_ptr<QPromise<std::expected<PlaceOrderResult, TSClient::Error>>>> m_pendingOrderPromises;
    QMap<uint64_t, QString> m_requestIdToStrategyId; // Temporary mapping until OrderID known
    QMap<QString, QString> m_orderMappings;          // OrderID → StrategyID (permanent)
};
