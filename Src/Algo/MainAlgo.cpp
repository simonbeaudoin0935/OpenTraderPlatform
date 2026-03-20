#include <QRunnable>
#include <QThread>
#include <QTimer>
#include <QSocketNotifier>
#include <QCoreApplication>
#include <unistd.h>

#include "MainAlgo.h"
#include "MainApp.h"
#include "StrategyManager.h"
#include "TSClient.h"
#include "DBClient.h"
#include "Logging.h"
#include "Assume.h"
#include "LTTng/LTTngTracepoints.h"
#include "OrderEmulator.h"
#include "CONSTANTS.h"
#include "OrdersDatabase.h"
#include "ThreadNames.h"

#define LOGGING_CATEGORY MainAlgoLog

Q_LOGGING_CATEGORY(MainAlgoLog, "MainAlgo")

// Initialize static member outside class
MainAlgo* MainAlgo::m_instance = nullptr;

MainAlgo* MainAlgo::getInstance()
{
    if (m_instance == nullptr)
    {
        m_instance = new MainAlgo();
    }
    return m_instance;
}

void MainAlgo::destroyInstance()
{
    ASSUME_TRUE(m_instance != nullptr);

    delete m_instance;
    m_instance = nullptr;
}


MainAlgo::MainAlgo()
{
    thread.setObjectName("MainAlgoThread");

    this->moveToThread(&thread);

    this->setObjectName("MainAlgo");

    m_strategyManager = std::make_unique<StrategyManager>(this);
    m_strategyManager->moveToThread(&thread);

    connect(&thread, &QThread::started, this, &MainAlgo::onThreadStarted);

    DEBUG << "Singleton instance created";
}

MainAlgo::~MainAlgo()
{
    DEBUG << "MainAlgo destructor - stopping thread";

    // Thread affinity assertion - destructor must be called from main thread
    OBJ_ASSUME_EQUAL(QThread::currentThread(), QCoreApplication::instance()->thread());

    // CRITICAL: Destroy all thread-owned objects that may own timers/notifiers/processes on the
    // MainAlgo thread BEFORE calling thread.quit(). If we destroy them from the main thread after
    // the thread has stopped, Qt warns about cross-thread teardown and can leave strategy runtime
    // cleanup happening on the wrong thread. The MainAlgo event loop is still running here, so
    // BlockingQueuedConnection is safe.
    QMetaObject::invokeMethod(
        this,
        [this]()
        {
            // Stop and destroy balance polling timer on its own thread
            m_balancePollingTimer.reset();

            // Destroy StrategyManager on its own thread so process supervision, socket
            // notifiers, and unload teardown all stay on MainAlgo.
            m_strategyManager.reset();
        },
        Qt::BlockingQueuedConnection);

    // CRITICAL: Stop thread BEFORE Qt's parent-child deletion destroys thread-owned objects
    thread.quit();

    // Wait for thread to finish (with timeout)
    if (!thread.wait(5000))
    {
        CRITICAL << "MainAlgo thread did not finish within timeout, terminating";
        thread.terminate();
        thread.wait();
    }

    DEBUG << "Destroyed singleton instance";
}

void MainAlgo::start()
{
    thread.start();
}

void MainAlgo::onThreadStarted()
{
    // Set kernel thread name for visibility in trace tools (ps, top, LTTng, TraceCompass)
    ThreadNames::setCurrentThreadName("MainAlgo");

    m_balancePollingTimer = std::make_unique<QTimer>(this);

    connect(m_balancePollingTimer.get(), &QTimer::timeout, this, &MainAlgo::requestBalance, Qt::UniqueConnection);

    // Positions: broadcast to all strategies via adapter (strategy thread)
    connect(this,
            &MainAlgo::receivedNewPosition,
            m_strategyManager.get(),
            &StrategyManager::onMainAlgoPositionUpdated,
            Qt::QueuedConnection);

    // Balance: broadcast to all strategies via adapter (strategy thread)
    connect(this,
            &MainAlgo::balanceUpdated,
            m_strategyManager.get(),
            &StrategyManager::onMainAlgoBalanceUpdated,
            Qt::QueuedConnection);

    // StrategyManager now shares the MainAlgo thread, so symbol releases can
    // decrement SymbolContext ref counts synchronously during unload/shutdown.
    connect(m_strategyManager.get(),
            &StrategyManager::symbolReleased,
            this,
            &MainAlgo::releaseSymbolContextRef,
            Qt::DirectConnection);

    // Direct cross-thread routing: DBClient emits on its own thread, we handle directly
    // via routeLevel2/routeTrade which use a read lock — no event-loop bounce.
    connect(DBClient::getInstance(), &DBClient::newLevel2, this, &MainAlgo::routeLevel2, Qt::DirectConnection);
    connect(DBClient::getInstance(), &DBClient::newTrade, this, &MainAlgo::routeTrade, Qt::DirectConnection);

    // Forward DBClient replay lifecycle signals to MainAlgo signals for UI.
    // Wired once here (both singletons are stable); enterReplayMode/Paused no longer re-wires these.
    auto* dbClient = DBClient::getInstance();
    bool connected = connect(dbClient, &DBClient::replayStarted, this, &MainAlgo::replayStarted);
    ASSUME_TRUE(connected);
    connected = connect(dbClient, &DBClient::replayStopped, this, &MainAlgo::replayStopped);
    ASSUME_TRUE(connected);
    connected = connect(dbClient, &DBClient::replayPaused, this, &MainAlgo::replayPaused);
    ASSUME_TRUE(connected);
    connected = connect(dbClient, &DBClient::replayResumed, this, &MainAlgo::replayResumed);
    ASSUME_TRUE(connected);
    connected = connect(dbClient, &DBClient::replayTimeUpdated, this, &MainAlgo::onReplayTimeReceived);
    ASSUME_TRUE(connected);
    connected = connect(dbClient, &DBClient::replayEndReached, this, &MainAlgo::replayEndReached);
    ASSUME_TRUE(connected);
    connected = connect(dbClient, &DBClient::replayEndReached, this, &MainAlgo::onReplayEndReached);
    ASSUME_TRUE(connected);
}

// ── Centralized routing (called directly from DBClient thread) ─────────────

void MainAlgo::routeLevel2(const QString& p_symbol, const Level2& p_level2)
{
    QReadLocker lock(&m_symbolContextsLock);
    auto sc = m_symbolContexts.value(p_symbol);
    if (!sc.isNull())
        sc->enqueueLevel2(p_level2);
}

void MainAlgo::routeTrade(const QString& p_symbol, const Trade& p_trade)
{
    QReadLocker lock(&m_symbolContextsLock);
    auto sc = m_symbolContexts.value(p_symbol);
    if (!sc.isNull())
        sc->enqueueTrade(p_trade);
}

/**
 * @brief Handles the selection of a new stock for display.
 *
 * This function is called when the user selects a different stock to display in the UI.
 * It manages the lifecycle of SymbolContext, disconnecting signals from the previous stock,
 * cleaning up resources (such as closing data streams), and setting up the new stock's
 * bar cache and market depth quote receivers with appropriate signal connections.
 *
 * If a stock was previously selected, it ensures proper cleanup by:
 * - Disconnecting signals from the old stock's BarCache and Level2Receiver
 * - Closing any active streams for the old stock
 * - Removing the old SymbolContext from the map and scheduling its deletion
 *
 * For the new stock, it either reuses an existing SymbolContext if the symbol is already
 * in the map, or creates a new one. It then connects the new stock's signals to emit
 * MainAlgo's signals for bar and market depth updates.
 *
 * @param symbol The stock symbol to select for display. Must be a valid stock symbol.
 *
 * @note This method must be called from the MainAlgo thread (QThread::currentThread() == &thread).
 * @note Assumes that if a stock is currently displayed, the new symbol is different.
 * @note Uses Qt's parent-child ownership for memory management of SymbolContext.
 */
void MainAlgo::onSelectDisplayedStock(const QString& symbol)
{
    // Make sure that this method gets Qt::InvokeMethod'ed if called from another thread
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);


    // If there is a current selected stock for display, release it
    if (m_currentDisplayedSymbolContext != nullptr)
    {
        // Selecting the same stock as currently selected. No action taken.
        OBJ_ASSUME_DIFF(m_currentDisplayedSymbolContext->symbol, symbol);

        // Clean up or detach the previous SymbolContext
        QString oldSymbol = m_currentDisplayedSymbolContext->symbol;
        m_currentDisplayedSymbolContext = nullptr;

        // Release the display ref — context is destroyed only if refCount reaches 0
        releaseSymbolContextRef(oldSymbol);
    }

    // Change the stock selected pointer to the new selected stock
    if (m_symbolContexts.contains(symbol))
    {
        m_currentDisplayedSymbolContext = m_symbolContexts[symbol];
        DEBUG << "onSelectDisplayedStock: reusing existing SymbolContext for" << symbol;
    }
    else
    {
        m_currentDisplayedSymbolContext = new SymbolContext(symbol, this); // Pass 'this' as parent
        Q_CHECK_PTR(m_currentDisplayedSymbolContext);

        {
            QWriteLocker lock(&m_symbolContextsLock);
            m_symbolContexts.insert(symbol, m_currentDisplayedSymbolContext);
        }

        // Subscribe to data for the new symbol
        if (MainApp::isInReplayMode())
        {
            if (!DBClient::getInstance()->addReplaySymbol(symbol))
                WARNING << "No replay data for" << symbol << "- live bars will not flow";
        }
        else
        {
            auto* dbClient = DBClient::getInstance();
            if (dbClient->getConnectionState() == DBClient::ConnectionState::Connected)
                dbClient->subscribeLive(symbol);
        }

        DEBUG << "onSelectDisplayedStock: created new SymbolContext for" << symbol;
    }

    // Claim display reference
    ++m_currentDisplayedSymbolContext->m_refCount;
    DEBUG << "onSelectDisplayedStock: set displayed symbol to" << symbol
          << "| refCount:" << m_currentDisplayedSymbolContext->m_refCount
          << "| active SymbolContexts:" << m_symbolContexts.keys();

    // No snapshot-writing connections needed here — SymbolContext always populates
    // its own DisplaySnapshot. The GUI reads from it at 30 Hz.
}

BarCache::GetBarsResult_t MainAlgo::requestMissingBarsDisplayedStock(QDate date, QTime first, QTime last, TimeFrame tf)
{
    if (m_currentDisplayedSymbolContext == nullptr)
    {
        // Instrument not yet initialized (e.g., setSymbol fired before onSelectDisplayedStock arrived).
        // Return empty result — checkForMissingBars will retry on next scroll/zoom.
        return std::make_shared<QVector<Bar>>();
    }

    DEBUG << "Requested bars from current displayed stock cache: " << first << " to " << last;

    OBJ_ASSUME_LTE(first, last); // The Equal in less than equal is for when the program is launched at 4:02 AM

    return m_currentDisplayedSymbolContext->barCache.getBars(tf, date, first, last);
}

BarCache::GetBarsResult_t MainAlgo::requestHistoricalBarsForSymbol(const QString& p_symbol,
                                                                   QDate p_date,
                                                                   QTime p_first,
                                                                   QTime p_last,
                                                                   TimeFrame p_tf)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    OBJ_ASSUME_FALSE(p_symbol.isEmpty());

    QPointer<SymbolContext> symbolContext = acquireSymbolContext(p_symbol);
    OBJ_ASSUME_DIFF(symbolContext, nullptr);

    BarCache::GetBarsResult_t result = symbolContext->barCache.getBars(p_tf, p_date, p_first, p_last);
    if (std::holds_alternative<std::shared_ptr<QVector<Bar>>>(result))
    {
        releaseSymbolContextRef(p_symbol);
        return std::get<std::shared_ptr<QVector<Bar>>>(result);
    }

    QPromise<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>> promise;
    QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>> forwardedFuture = promise.future();
    promise.start();

    auto sharedPromise =
        std::make_shared<QPromise<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(std::move(promise));

    std::get<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(result).then(
        this,
        [this, p_symbol, sharedPromise](std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>&& p_bars) mutable
        {
            releaseSymbolContextRef(p_symbol);
            sharedPromise->addResult(std::move(p_bars));
            sharedPromise->finish();
        });

    return forwardedFuture;
}

/*
 * This is the entry point that activates the chain of events after authentication state changes
 */
void MainAlgo::onTradeStationAuthStateChanged(bool isAuthenticated,
                                              TSClient::AuthStateReason reason,
                                              const QString& message)
{
    if (!isAuthenticated)
    {
        // Ignore transient "Connecting" state during token refresh
        if (reason == TSClient::AuthStateReason::Connecting)
        {
            DEBUG << "Token refresh in progress - ignoring transient auth state";
            return;
        }

        if (!m_havePastSuccessfulExchanges)
        {
            CRITICAL << "Tradestation failed to authenticate. Reason : " << message;
            CRITICAL << "Cannot proceed without authentication. Retrying";
        }
        else
        {
            CRITICAL << "Tradestation lost authentication. Reason : " << message;
        }
        return;
    }

    DEBUG << "Tradestation authenticated successfully : " << reason;

    // Now that the TSClient notified us that we are authenticated,
    // the first thing is to request the accounts.
    QFuture<std::expected<QVector<Account>, TSClient::Error>> future = TSClient::getInstance()->getAccounts();

    future.then(this,
                [this](std::expected<QVector<Account>, TSClient::Error> results)
                {
                    if (results.has_value())
                    {
                        DEBUG << "getAccounts() succeeded with" << results.value().size() << "accounts";
                        onReceivedAsyncGetAccounts(results.value());
                        return;
                    }
                    else
                    {
                        TSClient::Error error = results.error();
                        switch (error)
                        {
                        case TSClient::Error::Timeout:
                            CRITICAL << "getAccounts() failed with Timeout error";
                            break;
                        case TSClient::Error::JSONError:
                            CRITICAL << "getAccounts() failed with JSON error";
                            break;
                        case TSClient::Error::Other:
                            CRITICAL << "getAccounts() failed with Other error";
                            break;
                        default:
                            CRITICAL << "getAccounts() failed with Unknown error";
                            break;
                        }
                        QTimer::singleShot(1000,
                                           this,
                                           [this]()
                                           {
                                               DEBUG << "Retrying getAccounts() after failure";
                                               onTradeStationAuthStateChanged(true,
                                                                              TSClient::AuthStateReason::ValidToken,
                                                                              "Re-auth after getAccounts() failure");
                                           });
                    }
                });
}


void MainAlgo::onReceivedAsyncGetAccounts(const QVector<Account>& results)
{
    m_havePastSuccessfulExchanges = true;

    // Select account based on trading mode:
    // - LIVE: first account (index 0)
    // - SIM: last account in the list
    if (MainApp::getTradingMode() == TradingMode::Live)
    {
        m_activeAccount = results.first();
    }
    else
    {
        m_activeAccount = results.last();
    }

    INFO << "Selected account:" << m_activeAccount.getAccountId()
         << "for mode:" << (MainApp::getTradingMode() == TradingMode::Sim ? "SIM" : "LIVE");

    // Start balance polling if not already started
    if (!m_balancePollingStarted)
    {
        startBalancePolling();
        m_balancePollingStarted = true;
    }

    // Only initialize position stream once
    if (positionStreamStarted)
    {
        DEBUG << "Position stream already started, skipping initialization";
    }
    else
    {
        m_positionReceiver = new PositionsReceiver(m_activeAccount.getAccountId(), this);
        Q_CHECK_PTR(m_positionReceiver);
        positionStreamStarted = true;

        auto c1 = connect(m_positionReceiver,
                          &PositionsReceiver::receivedNewPosition,
                          this,
                          &MainAlgo::onReceivedNewPosition,
                          Qt::UniqueConnection);
        OBJ_ASSUME_TRUE(c1);

        auto c2 = connect(m_positionReceiver,
                          &PositionsReceiver::positionDeleted,
                          this,
                          &MainAlgo::onPositionDeleted,
                          Qt::UniqueConnection);
        OBJ_ASSUME_TRUE(c2);

        auto c3 = connect(m_positionReceiver,
                          &PositionsReceiver::loadedPositionsFromDatabase,
                          this,
                          &MainAlgo::onLoadedPositionsFromDatabase,
                          Qt::UniqueConnection);
        OBJ_ASSUME_TRUE(c3);
    }

    // Only initialize order stream once
    if (orderStreamStarted)
    {
        DEBUG << "Order stream already started, skipping initialization";
    }
    else
    {
        m_orderReceiver = new OrdersReceiver(m_activeAccount.getAccountId(), this);
        Q_CHECK_PTR(m_orderReceiver);
        orderStreamStarted = true;

        auto c4 = connect(m_orderReceiver,
                          &OrdersReceiver::receivedNewOrder,
                          this,
                          &MainAlgo::onReceivedNewOrder,
                          Qt::UniqueConnection);
        OBJ_ASSUME_TRUE(c4);
    }

    emit tradeStationAccountsReceived(results);
}

void MainAlgo::onReceivedNewPosition(const QString& account, Position position)
{
    //DEBUG << "Received new position:" << position.toJsonString();
    emit receivedNewPosition(account, position);
}

void MainAlgo::onPositionDeleted(const QString& account, const QString& positionID)
{
    Q_UNUSED(account);
    DEBUG << "Position deleted:" << positionID;
    emit positionDeleted(account, positionID);
}

void MainAlgo::onLoadedPositionsFromDatabase(const QString& account, QMap<QString, Position> positions)
{
    INFO << "Loading" << positions.size() << "positions from database for account" << account;

    // Emit each loaded position to the frontend
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it)
    {
        const Position& position = it.value();
        DEBUG << "Emitting loaded position:" << position.getPositionID();
        emit receivedNewPosition(account, position);
    }
}

void MainAlgo::onReceivedNewOrder(const QString& account, Order order)
{
    DEBUG << "onReceivedNewOrder: orderID=" << order.getOrderID()
          << "status=" << static_cast<int>(order.getOrderStatus()) << "mappings_size=" << m_orderMappings.size();

    // Attach strategy log for the full lifetime of the order.
    // Persist to DB on first arrival; keep in memory until the order is terminal
    // so that every subsequent update (e.g. Filled) also carries the log.
    auto logIt = m_orderIdToLog.find(order.getOrderID());
    if (logIt != m_orderIdToLog.end())
    {
        order.setStrategyLog(*logIt);

        // Persist only on the first update (ACK/OPN) — idempotent but saves extra queries
        const Order::Status status = order.getOrderStatus();
        if (status == Order::Status::ACK || status == Order::Status::OPN)
        {
            OrdersDatabase::getInstance()->updateOrderStrategyLog(order.getOrderID(), *logIt);
        }

        // Remove from map only when the order is in a terminal state
        const bool isTerminal =
            (status == Order::Status::FLL || status == Order::Status::FLP || status == Order::Status::FPR ||
             status == Order::Status::CAN || status == Order::Status::UCN || status == Order::Status::TSC ||
             status == Order::Status::REJ || status == Order::Status::EXP || status == Order::Status::OUT ||
             status == Order::Status::DON);
        if (isTerminal)
        {
            m_orderIdToLog.erase(logIt);
        }
    }

    // Emit enriched order to FrontEnd (with strategy log attached if available)
    emit receivedNewOrder(account, order);

    // Route to the strategy that placed this order
    auto strategyIt = m_orderMappings.find(order.getOrderID());
    if (strategyIt == m_orderMappings.end())
    {
        WARNING << "Received order update for order ID:" << order.getOrderID() << "which has no associated strategy";
        return;
    }

    QString strategyID = *strategyIt;
    DEBUG << "Routing order update for orderID=" << order.getOrderID() << "to strategyID=" << strategyID;
    QMetaObject::invokeMethod(
        m_strategyManager.get(),
        [this, strategyID, order]() { m_strategyManager->onOrderUpdatedForStrategy(strategyID, order); },
        Qt::QueuedConnection);
}

void MainAlgo::startBalancePolling()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    m_balancePollingTimer->start(PollingConstants::BALANCE_POLLING_INTERVAL_MS);
    requestBalance(); // initial request
    DEBUG << "Started balance polling";
}

void MainAlgo::stopBalancePolling()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    m_balancePollingTimer->stop();
    DEBUG << "Stopped balance polling";
}

[[nodiscard]] Balance MainAlgo::getCurrentBalance() const
{
    return m_currentBalance;
}

[[nodiscard]] QString MainAlgo::getDisplayedSymbol() const
{
    if (m_currentDisplayedSymbolContext)
    {
        return m_currentDisplayedSymbolContext->symbol;
    }
    return QString();
}

[[nodiscard]] QPointer<SymbolContext> MainAlgo::getDisplayedSymbolContext() const
{
    return m_currentDisplayedSymbolContext;
}

void MainAlgo::requestBalance()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    // Don't request balance if not authenticated — TSClient will assert on empty API key.
    // Exception: in replay mode, the mock network manager handles requests without real credentials.
    if (!TSClient::getInstance()->isAuthenticated() && TSClient::getInstance()->getMode() != TSClient::Mode::Replay)
        return;

    OBJ_ASSUME_FALSE(m_activeAccount.getAccountId().isEmpty());


    QFuture<std::expected<QVector<Balance>, TSClient::Error>> balanceFuture =
        TSClient::getInstance()->getBalances(QStringList(m_activeAccount.getAccountId()));

    balanceFuture.then(this,
                       [this](std::expected<QVector<Balance>, TSClient::Error> results)
                       {
                           if (results.has_value())
                           {
                               // DEBUG << "getBalances() succeeded with" << results.value().size() << "balances";
                               onBalanceReceived(results.value());
                               return;
                           }
                           else
                           {
                               // TODO do something smarter with errors
                               CRITICAL << "getBalances() failed with" << QtEnum::toString(results.error());
                           }
                       });
}

void MainAlgo::onBalanceReceived(const QVector<Balance>& results)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    OBJ_ASSUME_EQUAL(results.size(), 1);

    m_currentBalance = results.at(0);

    // Emit signal for the UI or other components interested
    emit balanceUpdated(m_currentBalance);
}

SymbolContext::SymbolContext(const QString& p_symbol, QObject* p_parent)
    : QObject(p_parent)
    , symbol(p_symbol)
    , barCache(p_symbol, this)
    , barReceiver(p_symbol, this)
    , m_level2Receiver(p_symbol, this)
    , m_liveBarAccumulator(this, 60)
    , m_live10sBarAccumulator(this, 10)
    , m_barAggregator(this)
{
    this->setObjectName("SymbolContext::" + p_symbol);

    // All internal connections use Qt::DirectConnection so they execute on the
    // pool thread during drain(). This is safe because drain guarantees only one
    // pool thread accesses a symbol's internals at a time.

    // Connect BarReceiver to BarCache for 1m bar storage
    bool connected = connect(
        &barReceiver,
        &BarReceiver::receivedNewBar,
        &barCache,
        [this](const QString&, const Bar& bar) { barCache.storeBar(TimeFrame::ONE_MINUTE, bar); },
        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // Wire LiveBarAccumulator::barClosed → BarReceiver::receivedNewBar
    connected = connect(&m_liveBarAccumulator,
                        &LiveBarAccumulator::barClosed,
                        &barReceiver,
                        &BarReceiver::receivedNewBar,
                        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // Wire LiveBarAccumulator::barUpdated → BarReceiver::receivedNewBar (in-progress candle)
    connected = connect(&m_liveBarAccumulator,
                        &LiveBarAccumulator::barUpdated,
                        &barReceiver,
                        &BarReceiver::receivedNewBar,
                        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // Wire closed 1m bars → BarAggregator for higher-TF accumulation (OHLCV + period-close detection)
    connected = connect(&m_liveBarAccumulator,
                        &LiveBarAccumulator::barClosed,
                        &m_barAggregator,
                        &BarAggregator::onNewBar,
                        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // Wire in-progress 1m bar updates → BarAggregator for real-time live candle animation
    connected = connect(&m_liveBarAccumulator,
                        &LiveBarAccumulator::barUpdated,
                        &m_barAggregator,
                        &BarAggregator::onBarUpdated,
                        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // Wire BarAggregator::barClosed → BarCache for higher-TF storage
    connected =
        connect(&m_barAggregator, &BarAggregator::barClosed, &barCache, &BarCache::storeBar, Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // Wire 10s accumulator barClosed → BarCache for 10s bar storage
    connected = connect(
        &m_live10sBarAccumulator,
        &LiveBarAccumulator::barClosed,
        &barCache,
        [this](const QString&, const Bar& bar) { barCache.storeBar(TimeFrame::TEN_SECONDS, bar); },
        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // ── Always-populate DisplaySnapshot ──────────────────────────────────
    // Every SymbolContext writes to its own DisplaySnapshot regardless of whether
    // it is the "currently displayed" symbol. This allows any chart window to read
    // from any SymbolContext's snapshot at 30 Hz with zero extra wiring.

    // 1m bar → snapshot
    connected = connect(
        &barReceiver,
        &BarReceiver::receivedNewBar,
        this,
        [this](const QString&, const Bar& bar)
        {
            L2T_TP(l2trader, snapshot_write, symbol.toUtf8().constData(), "bar");
            QWriteLocker lock(&m_displaySnapshot.lock);
            m_displaySnapshot.latestBar = bar;
            m_displaySnapshot.barDirty = true;
        },
        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // L2 → snapshot
    connected = connect(
        &m_level2Receiver,
        &Level2Receiver::receivedNewLevel2,
        this,
        [this](const QString&, const Level2& level2)
        {
            L2T_TP(l2trader, snapshot_write, symbol.toUtf8().constData(), "l2");
            QWriteLocker lock(&m_displaySnapshot.lock);
            m_displaySnapshot.latestLevel2 = level2;
            m_displaySnapshot.l2Dirty = true;
        },
        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // Higher-TF aggregator → snapshot
    connected = connect(
        &m_barAggregator,
        &BarAggregator::barUpdated,
        this,
        [this](TimeFrame tf, const Bar& bar)
        {
            L2T_TP(l2trader, snapshot_write, symbol.toUtf8().constData(), "aggregator");
            QWriteLocker lock(&m_displaySnapshot.lock);
            m_displaySnapshot.aggregatorBars[tf] = bar;
            m_displaySnapshot.aggregatorDirty = true;
        },
        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    connected = connect(
        &m_barAggregator,
        &BarAggregator::barClosed,
        this,
        [this](TimeFrame tf, const Bar& bar)
        {
            L2T_TP(l2trader, snapshot_write, symbol.toUtf8().constData(), "aggregator");
            QWriteLocker lock(&m_displaySnapshot.lock);
            m_displaySnapshot.aggregatorBars[tf] = bar;
            m_displaySnapshot.aggregatorDirty = true;
        },
        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // 10s accumulator → snapshot
    connected = connect(
        &m_live10sBarAccumulator,
        &LiveBarAccumulator::barUpdated,
        this,
        [this](const QString&, const Bar& bar)
        {
            L2T_TP(l2trader, snapshot_write, symbol.toUtf8().constData(), "aggregator10s");
            QWriteLocker lock(&m_displaySnapshot.lock);
            m_displaySnapshot.aggregatorBars[TimeFrame::TEN_SECONDS] = bar;
            m_displaySnapshot.aggregatorDirty = true;
        },
        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    connected = connect(
        &m_live10sBarAccumulator,
        &LiveBarAccumulator::barClosed,
        this,
        [this](const QString&, const Bar& bar)
        {
            L2T_TP(l2trader, snapshot_write, symbol.toUtf8().constData(), "aggregator10s");
            QWriteLocker lock(&m_displaySnapshot.lock);
            m_displaySnapshot.aggregatorBars[TimeFrame::TEN_SECONDS] = bar;
            m_displaySnapshot.aggregatorDirty = true;
        },
        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // DBClient wiring is handled centrally by MainAlgo routing (onNewLevel2Received / onNewTradeReceived).
    // Live subscription is also managed by MainAlgo when creating the SymbolContext.

    DEBUG << "New instance";
}

SymbolContext::~SymbolContext()
{
    // Signal that we're destroying — drain() will exit early on pending items
    m_destroying.store(true, std::memory_order_release);

    // Wait for any running drain to complete
    L2T_TP(l2trader, symbolctx_shutdown_wait, symbol.toUtf8().constData());
    QMutexLocker lock(&m_queueMutex);
    while (m_draining.load(std::memory_order_acquire))
    {
        m_drainDone.wait(&m_queueMutex);
    }

    DEBUG << "Deleted instance";
}

// ── Actor model: enqueue / drain ───────────────────────────────────────────

void SymbolContext::enqueueLevel2(const Level2& p_level2)
{
    [[maybe_unused]] int depth = 0;
    {
        QMutexLocker lock(&m_queueMutex);
        m_queue.enqueue(WorkItem{p_level2});
        depth = m_queue.size();
    }
    L2T_TP(l2trader, symbolctx_enqueue, symbol.toUtf8().constData(), "L2", depth);
    if (!m_draining.exchange(true, std::memory_order_acq_rel))
    {
        L2T_TP(l2trader, symbolctx_pool_submit, symbol.toUtf8().constData());
        QThreadPool::globalInstance()->start(QRunnable::create([this] { drain(); }));
    }
}

void SymbolContext::enqueueTrade(const Trade& p_trade)
{
    [[maybe_unused]] int depth = 0;
    {
        QMutexLocker lock(&m_queueMutex);
        m_queue.enqueue(WorkItem{p_trade});
        depth = m_queue.size();
    }
    L2T_TP(l2trader, symbolctx_enqueue, symbol.toUtf8().constData(), "Trade", depth);
    if (!m_draining.exchange(true, std::memory_order_acq_rel))
    {
        L2T_TP(l2trader, symbolctx_pool_submit, symbol.toUtf8().constData());
        QThreadPool::globalInstance()->start(QRunnable::create([this] { drain(); }));
    }
}

void SymbolContext::drain()
{
    // Thread assertion: drain runs on a pool thread, never on the GUI or MainAlgo thread
    OBJ_ASSUME_DIFF(QThread::currentThread(), QCoreApplication::instance()->thread());

    L2T_TP(l2trader, symbolctx_drain_start, symbol.toUtf8().constData());
    int itemsProcessed = 0;

    for (;;)
    {
        WorkItem item;
        {
            QMutexLocker lock(&m_queueMutex);
            if (m_queue.isEmpty())
            {
                m_draining.store(false, std::memory_order_release);
                m_drainDone.wakeAll();
                // ABA re-check: an enqueue may have happened between isEmpty() and store(false)
                if (m_queue.isEmpty())
                {
                    L2T_TP(l2trader, symbolctx_drain_end, symbol.toUtf8().constData(), itemsProcessed);
                    return;
                }
                if (!m_draining.exchange(true, std::memory_order_acq_rel))
                {
                    L2T_TP(l2trader, symbolctx_drain_end, symbol.toUtf8().constData(), itemsProcessed);
                    return;
                }
                continue;
            }
            item = m_queue.dequeue();
        }

        if (m_destroying.load(std::memory_order_acquire))
        {
            L2T_TP(l2trader, symbolctx_drain_end, symbol.toUtf8().constData(), itemsProcessed);
            return;
        }

        std::visit(
            [this](auto&& event)
            {
                using T = std::decay_t<decltype(event)>;
                if constexpr (std::is_same_v<T, Level2>)
                {
                    L2T_TP(l2trader, symbolctx_process_level2, symbol.toUtf8().constData());
                    processLevel2(event);
                }
                else if constexpr (std::is_same_v<T, Trade>)
                {
                    L2T_TP(l2trader, symbolctx_process_trade, symbol.toUtf8().constData());
                    processTrade(event);
                }
            },
            item);
        ++itemsProcessed;
    }
}

void SymbolContext::processLevel2(const Level2& p_level2)
{
    m_activity.recordL2(QDateTime::currentMSecsSinceEpoch());
    m_level2Receiver.onReceivedNewLevel2(p_level2);
}

void SymbolContext::processTrade(const Trade& p_trade)
{
    m_activity.recordTrade(QDateTime::currentMSecsSinceEpoch());
    m_liveBarAccumulator.onNewTrade(symbol, p_trade);
    m_live10sBarAccumulator.onNewTrade(symbol, p_trade);
    emit receivedNewTrade(symbol, p_trade);

    // Write trade to DisplaySnapshot so any chart showing this symbol gets it
    L2T_TP(l2trader, snapshot_write, symbol.toUtf8().constData(), "trade");
    QWriteLocker lock(&m_displaySnapshot.lock);
    m_displaySnapshot.pendingTrades.append(p_trade);
    m_displaySnapshot.tradeDirty = true;
}

uint64_t MainAlgo::getNextRequestId()
{
    // Thread-safe atomic increment returns the old value, so we need pre-increment semantics
    // Actually ++operator does pre-increment by default for atomic
    return ++m_requestIdCounter;
}

std::expected<QString, QString> MainAlgo::loadStrategy(const StrategyConfig& p_config)
{
    if (!m_strategyManager)
    {
        return std::unexpected("Strategy manager unavailable");
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->loadStrategy(p_config);
    }

    if (!thread.isRunning())
    {
        return std::unexpected("MainAlgo thread is not running");
    }

    std::expected<QString, QString> result = std::unexpected(QString{});
    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, &result, p_config]() { result = m_strategyManager->loadStrategy(p_config); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

QString MainAlgo::startStrategy(const QString& p_strategyID)
{
    if (!m_strategyManager)
    {
        return "Strategy manager unavailable";
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->startStrategy(p_strategyID);
    }

    if (!thread.isRunning())
    {
        return "MainAlgo thread is not running";
    }

    QString result;
    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, &result, p_strategyID]() { result = m_strategyManager->startStrategy(p_strategyID); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

QString MainAlgo::unloadStrategy(const QString& p_strategyID)
{
    if (!m_strategyManager)
    {
        return "Strategy manager unavailable";
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->unloadStrategy(p_strategyID);
    }

    if (!thread.isRunning())
    {
        return "MainAlgo thread is not running";
    }

    QString result;
    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, &result, p_strategyID]() { result = m_strategyManager->unloadStrategy(p_strategyID); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

bool MainAlgo::isStrategyRunning(const QString& p_strategyID) const
{
    if (!m_strategyManager)
    {
        return false;
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->isStrategyRunning(p_strategyID);
    }

    if (!thread.isRunning())
    {
        return false;
    }

    bool result = false;
    MainAlgo* const self = const_cast<MainAlgo*>(this);
    const bool invoked = QMetaObject::invokeMethod(
        self,
        [this, &result, p_strategyID]() { result = m_strategyManager->isStrategyRunning(p_strategyID); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

QVector<Position> MainAlgo::getStrategyOpenPositions(const QString& p_strategyID) const
{
    if (!m_strategyManager)
    {
        return {};
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->getStrategyOpenPositions(p_strategyID);
    }

    if (!thread.isRunning())
    {
        return {};
    }

    QVector<Position> result;
    MainAlgo* const self = const_cast<MainAlgo*>(this);
    const bool invoked = QMetaObject::invokeMethod(
        self,
        [this, &result, p_strategyID]() { result = m_strategyManager->getStrategyOpenPositions(p_strategyID); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

std::optional<QVector<StrategyLogMessage>> MainAlgo::getStrategyLogMessages(const QString& p_strategyID) const
{
    if (!m_strategyManager)
    {
        return std::nullopt;
    }

    auto readMessages = [this, &p_strategyID]() -> std::optional<QVector<StrategyLogMessage>>
    {
        const StrategyLogger* logger = m_strategyManager->getStrategyLogger(p_strategyID);
        if (logger == nullptr)
        {
            return std::nullopt;
        }

        return logger->getMessages();
    };

    if (QThread::currentThread() == &thread)
    {
        return readMessages();
    }

    if (!thread.isRunning())
    {
        return std::nullopt;
    }

    std::optional<QVector<StrategyLogMessage>> result;
    MainAlgo* const self = const_cast<MainAlgo*>(this);
    const bool invoked = QMetaObject::invokeMethod(
        self,
        [&result, readMessages]() { result = readMessages(); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

void MainAlgo::processPlaceOrder(uint64_t p_requestId,
                                 const QString& p_strategyID,
                                 const PlaceOrderRequest& p_orderRequest,
                                 std::shared_ptr<QPromise<std::expected<PlaceOrderResult, TSClient::Error>>> p_promise)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    // Store temporary mapping: requestId -> strategyID (will be replaced with OrderID -> strategyID when ACK received)
    m_requestIdToStrategyId[p_requestId] = p_strategyID;

    // If the request carries a strategy log, hold it until we have the OrderID from ACK
    if (p_orderRequest.getStrategyLog().has_value())
    {
        m_pendingOrderLogs[p_requestId] = p_orderRequest.getStrategyLog().value();
    }

    // Store the promise for resolution when order is acknowledged
    m_pendingOrderPromises[p_requestId] = p_promise;

    // Call TSClient to place the order
    QFuture<std::expected<PlaceOrderResult, TSClient::Error>> future =
        TSClient::getInstance()->placeOrder(p_orderRequest);

    // Attach continuation to detect resolution
    // Pass 'this' as context so continuation runs on MainAlgo thread
    future.then(this,
                [this, p_requestId](std::expected<PlaceOrderResult, TSClient::Error> result)
                { onOrderResolved(p_requestId, result); });

    DEBUG << "Processing placeOrder: requestId=" << p_requestId << "strategyID=" << p_strategyID;
}

void MainAlgo::onOrderResolved(uint64_t p_requestId, const std::expected<PlaceOrderResult, TSClient::Error>& p_result)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    // Look up and remove the promise
    auto promiseIt = m_pendingOrderPromises.find(p_requestId);
    OBJ_ASSUME_FALSE(promiseIt == m_pendingOrderPromises.end());
    auto promise = *promiseIt;
    m_pendingOrderPromises.erase(promiseIt);

    // Look up which strategy placed this order
    auto strategyIt = m_requestIdToStrategyId.find(p_requestId);

    // There is something catastrophically wrong if the requestID is not in the
    // map when the QFuture associated to it gets resolved here
    OBJ_ASSUME_FALSE(strategyIt == m_requestIdToStrategyId.end());

    QString strategyID = *strategyIt;
    m_requestIdToStrategyId.remove(p_requestId);

    // Resolve the promise with the result
    promise->addResult(p_result);
    promise->finish();

    if (p_result.has_value())
    {
        // Order was successfully placed
        PlaceOrderResult result = p_result.value();

        DEBUG << "Order placed successfully: strategyID=" << strategyID << "successful=" << result.isAllSuccessful();

        // Extract OrderIDs from result and create permanent mappings
        const auto& orders = result.getOrders();
        for (const auto& orderResultItem: orders)
        {
            if (!orderResultItem.isError())
            {
                // Successful order - create permanent mapping for future updates
                QString orderID = orderResultItem.getOrderID();
                m_orderMappings[orderID] = strategyID;
                DEBUG << "Created order mapping: OrderID=" << orderID << "→ strategyID=" << strategyID
                      << "(total mappings=" << m_orderMappings.size() << ")";

                // Promote any pending strategy log from requestId → orderID scope
                if (m_pendingOrderLogs.contains(p_requestId))
                {
                    m_orderIdToLog[orderID] = m_pendingOrderLogs.take(p_requestId);
                }
            }
        }

        // TODO: Emit GUI signal if this order is for the displayed stock
        // TODO: Route order result to strategy via SDK
    }
    else
    {
        // Order placement failed - discard any pending log for this request
        m_pendingOrderLogs.remove(p_requestId);

        TSClient::Error error = p_result.error();
        WARNING << "Order placement failed: requestId=" << p_requestId << "strategyID=" << strategyID
                << "error=" << QtEnum::toString(error);

        // TODO: Route error to strategy via SDK
    }
}

void MainAlgo::processCancelOrder(
    const QString& p_orderID,
    std::shared_ptr<QPromise<std::expected<CancelOrderResult, TSClient::Error>>> p_promise)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    ASSUME_DIFF(p_promise.get(), nullptr);

    QFuture<std::expected<CancelOrderResult, TSClient::Error>> future = TSClient::getInstance()->cancelOrder(p_orderID);

    future.then(this,
                [p_promise](std::expected<CancelOrderResult, TSClient::Error> result)
                {
                    p_promise->addResult(result);
                    p_promise->finish();
                });

    DEBUG << "Processing cancelOrder: orderID=" << p_orderID;
}

void MainAlgo::processSubscribeToSymbol(const QString& p_strategyID,
                                        const QString& p_symbol,
                                        std::shared_ptr<QPromise<bool>> p_promise)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    ASSUME_DIFF(p_promise.get(), nullptr);

    if (MainApp::isInReplayMode())
    {
        // Validate data exists for the replay date
        if (!DBClient::hasReplayData(m_replayDate, p_symbol))
        {
            WARNING << "No replay data for" << p_symbol << "on" << m_replayDate.toString(Qt::ISODate)
                    << "- subscription rejected";
            p_promise->addResult(false);
            p_promise->finish();
            return;
        }

        // If symbol is already loaded (displayed stock), reuse the existing SymbolContext.
        if (m_symbolContexts.contains(p_symbol))
        {
            ++m_symbolContexts[p_symbol]->m_refCount;
            // Pass the actual SymbolContext so the strategy gets a direct connection (no MainAlgo hop)
            m_strategyManager->connectSymbolToStrategy(p_strategyID, p_symbol, m_symbolContexts[p_symbol]);
            p_promise->addResult(true);
            p_promise->finish();
            return;
        }

        // New secondary symbol: create SymbolContext — data flows automatically via
        // DBClient::newLevel2/newTrade → MainAlgo routing → SymbolContext queue.
        // Also open its replay files in DBClient so records get emitted.
        auto* instrument = new SymbolContext(p_symbol, this);
        Q_CHECK_PTR(instrument);
        instrument->m_refCount = 1; // Strategy claim
        {
            QWriteLocker lock(&m_symbolContextsLock);
            m_symbolContexts.insert(p_symbol, instrument);
        }

        // Open replay data files for this symbol (non-blocking, same-thread call)
        if (!DBClient::getInstance()->addReplaySymbol(p_symbol))
        {
            WARNING << "addReplaySymbol failed for" << p_symbol << "- no data files found";
        }

        // Wire bar-close to OrderEmulator if replay is active
        if (OrderEmulator* emulator = TSClient::getInstance()->getOrderEmulator())
            connectBarCloseToOrderEmulator(instrument, emulator);

        m_strategyManager->connectSymbolToStrategy(p_strategyID, p_symbol, instrument);

        INFO << "Secondary symbol SymbolContext created for" << p_symbol << "(replay data routed via DBClient)";
    }
    else
    {
        // Live/sim mode: create SymbolContext if needed and subscribe
        if (!m_symbolContexts.contains(p_symbol) || m_symbolContexts[p_symbol].isNull())
        {
            auto* instrument = new SymbolContext(p_symbol, this);
            Q_CHECK_PTR(instrument);
            instrument->m_refCount = 1; // Strategy claim
            {
                QWriteLocker lock(&m_symbolContextsLock);
                m_symbolContexts.insert(p_symbol, instrument);
            }

            auto* dbClient = DBClient::getInstance();
            if (dbClient->getConnectionState() == DBClient::ConnectionState::Connected)
            {
                dbClient->subscribeLive(p_symbol);
            }
        }
        else
        {
            ++m_symbolContexts[p_symbol]->m_refCount;
        }

        m_strategyManager->connectSymbolToStrategy(p_strategyID, p_symbol, m_symbolContexts[p_symbol]);
    }

    p_promise->addResult(true);
    p_promise->finish();
}

QPointer<SymbolContext> MainAlgo::acquireSymbolContext(const QString& p_symbol)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    QPointer<SymbolContext> sc;

    if (m_symbolContexts.contains(p_symbol))
    {
        sc = m_symbolContexts[p_symbol];
        DEBUG << "acquireSymbolContext: reusing existing SymbolContext for" << p_symbol;
    }
    else
    {
        sc = new SymbolContext(p_symbol, this);
        Q_CHECK_PTR(sc);

        {
            QWriteLocker lock(&m_symbolContextsLock);
            m_symbolContexts.insert(p_symbol, sc);
        }

        // Subscribe to data for the new symbol
        if (MainApp::isInReplayMode())
        {
            if (!DBClient::getInstance()->addReplaySymbol(p_symbol))
                WARNING << "No replay data for" << p_symbol << "- live bars will not flow";
        }
        else
        {
            auto* dbClient = DBClient::getInstance();
            if (dbClient->getConnectionState() == DBClient::ConnectionState::Connected)
                dbClient->subscribeLive(p_symbol);
        }

        DEBUG << "acquireSymbolContext: created new SymbolContext for" << p_symbol;
    }

    ++sc->m_refCount;
    DEBUG << "acquireSymbolContext:" << p_symbol << "refCount now" << sc->m_refCount;
    return sc;
}

void MainAlgo::releaseSymbolContextRef(const QString& symbol)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    if (!m_symbolContexts.contains(symbol))
    {
        WARNING << "releaseSymbolContextRef: no SymbolContext for" << symbol;
        return;
    }

    QPointer<SymbolContext> sc = m_symbolContexts[symbol];
    OBJ_ASSUME_DIFF(sc, nullptr);

    --sc->m_refCount;
    DEBUG << "releaseSymbolContextRef:" << symbol << "refCount now" << sc->m_refCount;

    if (sc->m_refCount <= 0)
    {
        {
            QWriteLocker lock(&m_symbolContextsLock);
            int removed = m_symbolContexts.remove(symbol);
            OBJ_ASSUME_EQUAL(removed, 1);
        }
        sc->deleteLater();
        DEBUG << "SymbolContext destroyed for" << symbol;
    }
}

void MainAlgo::processClaimSymbols(const QString& p_strategyID,
                                   const QStringList& p_symbols,
                                   std::shared_ptr<QPromise<QStringList>> p_promise)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    m_strategyManager->processClaimSymbols(p_strategyID, p_symbols, p_promise);
}

// onAggregatorBarUpdated/onAggregatorBarClosed removed — snapshot writes are now
// handled directly inside SymbolContext via DirectConnection to m_barAggregator.

// ---------------------------------------------------------------------------
// Replay time snapshot writer — replay time is a global concept, not per-SymbolContext
// ---------------------------------------------------------------------------

void MainAlgo::onReplayTimeReceived(const QDateTime& time)
{
    if (!m_currentDisplayedSymbolContext)
        return;

    L2T_TP(l2trader, snapshot_write, m_currentDisplayedSymbolContext->symbol.toUtf8().constData(), "replayTime");
    QWriteLocker lock(&m_currentDisplayedSymbolContext->m_displaySnapshot.lock);
    m_currentDisplayedSymbolContext->m_displaySnapshot.replayTime = time;
    m_currentDisplayedSymbolContext->m_displaySnapshot.replayTimeDirty = true;
}

void MainAlgo::onReplayEndReached()
{
    INFO << "Replay ended, pausing heartbeat timers to prevent stream timeout";

    // Pause heartbeat timers on ALL mock streams, not just the displayed one
    for (auto& instrument: m_symbolContexts)
    {
        if (instrument.isNull())
        {
            continue;
        }
        instrument->barReceiver.pauseHeartbeat();
        instrument->m_level2Receiver.pauseHeartbeat();
    }
}

void MainAlgo::enterReplayMode(const QString& p_symbol, QDate p_date, QTime p_startTime, Playback::Speed p_speed)
{
    INFO << "MainAlgo entering replay mode for" << p_symbol << "on" << p_date.toString(Qt::ISODate) << "at"
         << p_startTime.toString("hh:mm:ss");

    // Store replay state for strategy subscription validation
    m_replayDate = p_date;
    m_replayStartTime = p_startTime;
    m_replaySpeed = p_speed;

    auto* dbClient = DBClient::getInstance();

    // Wire OrderEmulator to DBClient market data (same signals as live)
    connectReplaySignals(p_symbol);

    startReplayOrderStreams();

    dbClient->startReplay(p_symbol, p_date, p_startTime, p_speed);
}

void MainAlgo::enterReplayModePaused(const QString& p_symbol, QDate p_date, QTime p_startTime, Playback::Speed p_speed)
{
    INFO << "MainAlgo entering replay mode (paused) for" << p_symbol << "on" << p_date.toString(Qt::ISODate) << "at"
         << p_startTime.toString("hh:mm:ss");

    m_replayDate = p_date;
    m_replayStartTime = p_startTime;
    m_replaySpeed = p_speed;

    auto* dbClient = DBClient::getInstance();

    bool isReentry = dbClient->isReplayActive();

    // If replay is already active (e.g., changing replay day), stop it first
    if (isReentry)
    {
        DEBUG << "Stopping existing replay before starting new one";
        dbClient->stopReplay();
    }

    // Forward DBClient replay lifecycle signals are wired once in onThreadStarted().

    connectReplaySignals(p_symbol);

    if (!isReentry)
    {
        startReplayOrderStreams();
    }

    dbClient->startReplayPaused(p_symbol, p_date, p_startTime, p_speed);

    // Pause heartbeat timers since we're starting in paused state
    for (auto& instrument: m_symbolContexts)
    {
        OBJ_ASSUME_FALSE(instrument.isNull());
        instrument->barReceiver.pauseHeartbeat();
        instrument->m_level2Receiver.pauseHeartbeat();
    }
}

void MainAlgo::connectBarCloseToOrderEmulator(SymbolContext* p_sc, OrderEmulator* p_emulator)
{
    OBJ_ASSUME_DIFF(p_sc, nullptr);
    OBJ_ASSUME_DIFF(p_emulator, nullptr);

    // Qt::UniqueConnection silently rejects lambda connections — use plain connection.
    // connectReplaySignals guards against re-entry at the call site.
    auto feedBarClose = [p_emulator](const QString& sym, const Bar& bar)
    { p_emulator->updateBarClose(sym, bar.getClose()); };

    connect(&p_sc->m_liveBarAccumulator, &LiveBarAccumulator::barUpdated, p_emulator, feedBarClose);
    connect(&p_sc->m_liveBarAccumulator, &LiveBarAccumulator::barClosed, p_emulator, feedBarClose);
}

void MainAlgo::connectReplaySignals(const QString& p_symbol)
{
    auto* dbClient = DBClient::getInstance();
    OrderEmulator* emulator = TSClient::getInstance()->getOrderEmulator();

    if (emulator)
    {
        // Replay Level2 → OrderEmulator (market data for order fills). UniqueConnection
        // guards against duplicate wiring on re-entry (e.g. preloadChartForReplay).
        connect(dbClient, &DBClient::newLevel2, emulator, &OrderEmulator::updateMarketDepth, Qt::UniqueConnection);

        // Bar close price → OrderEmulator for all active symbols (needed by recalculatePositionPnL).
        // Disconnect first per-SymbolContext to prevent duplicates on re-entry (preloadChart then
        // startReplay both call connectReplaySignals). UniqueConnection can't be used with lambdas.
        for (auto& sc: m_symbolContexts)
        {
            if (sc.isNull())
                continue;
            disconnect(&sc->m_liveBarAccumulator, &LiveBarAccumulator::barUpdated, emulator, nullptr);
            disconnect(&sc->m_liveBarAccumulator, &LiveBarAccumulator::barClosed, emulator, nullptr);
            connectBarCloseToOrderEmulator(sc, emulator);
        }

        emulator->setReplaySpeed(static_cast<int>(m_replaySpeed));
    }

    // Trade forwarding to DisplaySnapshot is now handled internally by SymbolContext::processTrade.

    INFO << "Replay signals connected for" << p_symbol;
}

void MainAlgo::exitReplayMode()
{
    INFO << "MainAlgo exiting replay mode";

    auto* dbClient = DBClient::getInstance();
    dbClient->stopReplay();

    // Disconnect replay-specific signals from DBClient
    disconnect(dbClient, &DBClient::replayStarted, this, nullptr);
    disconnect(dbClient, &DBClient::replayStopped, this, nullptr);
    disconnect(dbClient, &DBClient::replayPaused, this, nullptr);
    disconnect(dbClient, &DBClient::replayResumed, this, nullptr);
    disconnect(dbClient, &DBClient::replayTimeUpdated, this, nullptr);
    disconnect(dbClient, &DBClient::replayEndReached, this, nullptr);
}

void MainAlgo::pauseReplay()
{
    DBClient::getInstance()->pauseReplay();

    // Pause heartbeat timers on ALL streams to prevent timeout while paused
    for (auto& instrument: m_symbolContexts)
    {
        OBJ_ASSUME_FALSE(instrument.isNull());
        instrument->barReceiver.pauseHeartbeat();
        instrument->m_level2Receiver.pauseHeartbeat();
    }
}

void MainAlgo::resumeReplay()
{
    // Resume heartbeat timers on ALL streams before resuming replay
    for (auto& instrument: m_symbolContexts)
    {
        OBJ_ASSUME_FALSE(instrument.isNull());
        instrument->barReceiver.resumeHeartbeat();
        instrument->m_level2Receiver.resumeHeartbeat();
    }

    DBClient::getInstance()->resumeReplay();
}

void MainAlgo::setReplaySpeed(Playback::Speed p_speed)
{
    m_replaySpeed = p_speed;
    DBClient::getInstance()->setReplaySpeed(p_speed);

    // Sync speed to OrderEmulator so latency is scaled correctly
    if (OrderEmulator* emulator = TSClient::getInstance()->getOrderEmulator())
    {
        emulator->setReplaySpeed(static_cast<int>(p_speed));
    }
}

void MainAlgo::pauseLiveStreams()
{
    INFO << "Pausing live streams for replay mode";

    // Receivers may be null if the user enters replay before account setup completed
    if (m_positionReceiver)
    {
        m_positionReceiver->stopStream(m_activeAccount.getAccountId());
        positionStreamStarted = false;
        DEBUG << "Positions stream stopped";
    }
    else
    {
        DEBUG << "No position stream to stop (not yet started)";
    }

    if (m_orderReceiver)
    {
        m_orderReceiver->stopStream(m_activeAccount.getAccountId());
        orderStreamStarted = false;
        DEBUG << "Orders stream stopped";
    }
    else
    {
        DEBUG << "No order stream to stop (not yet started)";
    }
}

void MainAlgo::startReplayOrderStreams()
{
    INFO << "Starting replay order/position streams with simulated account";

    // Get the simulated account ID from TSClient
    QString simAccountID = OrderEmulator::getSimulatedAccountID();

    // Create simulated account and update m_activeAccount
    QJsonObject accountJson;
    accountJson["AccountID"] = simAccountID;
    accountJson["AccountType"] = "Margin";
    accountJson["Name"] = "Replay Simulation Account";
    accountJson["Status"] = "Active";
    Account simAccount(accountJson);

    // Update active account to the simulated account
    m_activeAccount = simAccount;
    INFO << "Set active account to simulated account:" << simAccountID;

    // Delete existing receivers and create new ones with the simulated account
    if (m_positionReceiver)
    {
        delete m_positionReceiver;
    }
    m_positionReceiver = new PositionsReceiver(simAccountID, this);
    bool connected = connect(m_positionReceiver,
                             &PositionsReceiver::receivedNewPosition,
                             this,
                             &MainAlgo::onReceivedNewPosition,
                             Qt::UniqueConnection);
    ASSUME_TRUE(connected);
    connected = connect(m_positionReceiver,
                        &PositionsReceiver::positionDeleted,
                        this,
                        &MainAlgo::onPositionDeleted,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);
    connected = connect(m_positionReceiver,
                        &PositionsReceiver::loadedPositionsFromDatabase,
                        this,
                        &MainAlgo::onLoadedPositionsFromDatabase,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);
    positionStreamStarted = true;
    DEBUG << "Replay positions receiver created for" << simAccountID;

    if (m_orderReceiver)
    {
        delete m_orderReceiver;
    }
    m_orderReceiver = new OrdersReceiver(simAccountID, this);
    connected = connect(m_orderReceiver,
                        &OrdersReceiver::receivedNewOrder,
                        this,
                        &MainAlgo::onReceivedNewOrder,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);
    orderStreamStarted = true;
    DEBUG << "Replay orders receiver created for" << simAccountID;

    // Emit simulated account to update GUI account selector
    QVector<Account> simAccounts;
    simAccounts.append(simAccount);
    emit tradeStationAccountsReceived(simAccounts);
    DEBUG << "Emitted simulated account for replay mode";

    // Start balance polling so the balances widget updates during replay
    if (!m_balancePollingStarted)
    {
        startBalancePolling();
        m_balancePollingStarted = true;
    }
}

void MainAlgo::resumeLiveStreams()
{
    INFO << "Resuming live streams after replay mode";

    // Receivers exist but their streams were stopped in pauseLiveStreams
    OBJ_ASSUME_DIFF(m_positionReceiver, nullptr);
    OBJ_ASSUME_DIFF(m_orderReceiver, nullptr);

    // Skip account fetch if not authenticated (e.g. replay-only without TS credentials)
    if (!TSClient::getInstance()->isAuthenticated())
    {
        INFO
            << "Not authenticated with TradeStation — stopping balance polling and skipping account fetch after replay mode";
        stopBalancePolling();
        m_balancePollingStarted = false;
        return;
    }

    // Fetch real accounts from API (replay mode uses fake "SIM123456")
    INFO << "Fetching real accounts from API after replay mode";
    QFuture<std::expected<QVector<Account>, TSClient::Error>> future = TSClient::getInstance()->getAccounts();

    future.then(this,
                [this](std::expected<QVector<Account>, TSClient::Error> results)
                {
                    if (!results.has_value())
                    {
                        CRITICAL << "Failed to fetch accounts after replay mode";
                        return;
                    }

                    QVector<Account> accounts = results.value();
                    if (accounts.isEmpty())
                    {
                        CRITICAL << "No accounts returned after replay mode";
                        return;
                    }

                    // Emit accounts to GUI so user can select
                    emit tradeStationAccountsReceived(accounts);

                    // Use first account as active (or find previous active if still exists)
                    m_activeAccount = accounts.first();
                    INFO << "Using account" << m_activeAccount.getAccountId() << "after replay mode";

                    // Delete and recreate receivers with real account
                    delete m_positionReceiver;
                    m_positionReceiver = new PositionsReceiver(m_activeAccount.getAccountId(), this);
                    bool connected = connect(m_positionReceiver,
                                             &PositionsReceiver::receivedNewPosition,
                                             this,
                                             &MainAlgo::onReceivedNewPosition,
                                             Qt::UniqueConnection);
                    ASSUME_TRUE(connected);
                    connected = connect(m_positionReceiver,
                                        &PositionsReceiver::positionDeleted,
                                        this,
                                        &MainAlgo::onPositionDeleted,
                                        Qt::UniqueConnection);
                    ASSUME_TRUE(connected);
                    connected = connect(m_positionReceiver,
                                        &PositionsReceiver::loadedPositionsFromDatabase,
                                        this,
                                        &MainAlgo::onLoadedPositionsFromDatabase,
                                        Qt::UniqueConnection);
                    ASSUME_TRUE(connected);
                    positionStreamStarted = true;
                    DEBUG << "Positions receiver recreated and started";

                    delete m_orderReceiver;
                    m_orderReceiver = new OrdersReceiver(m_activeAccount.getAccountId(), this);
                    connected = connect(m_orderReceiver,
                                        &OrdersReceiver::receivedNewOrder,
                                        this,
                                        &MainAlgo::onReceivedNewOrder,
                                        Qt::UniqueConnection);
                    ASSUME_TRUE(connected);
                    orderStreamStarted = true;
                    DEBUG << "Orders receiver recreated and started";
                });
}

Playback::State MainAlgo::getReplayState() const
{
    return DBClient::getInstance()->getPlaybackState();
}

MainAlgo::ActivityMetrics MainAlgo::getActivityMetrics(const QString& p_symbol) const
{
    QReadLocker lock(&m_symbolContextsLock);
    const auto sc = m_symbolContexts.value(p_symbol);
    if (sc.isNull())
        return {};
    return {sc->m_activity.tradeRateHz(), sc->m_activity.l2RateHz(), sc->m_activity.isActive()};
}

void MainAlgo::deleteAllSymbolContext()
{
    INFO << "Deleting all stock instruments for clean mode transition";

    // Clear the displayed pointer first
    m_currentDisplayedSymbolContext = nullptr;

    // Delete instruments directly (not deleteLater) so that each BarCache destructor
    // queues closeDatabase to DatabaseThread before the next createAndSetDisplayedSymbolContext
    // queues openDatabase. deleteLater would defer destruction past the next openDatabase call,
    // causing the DB close to arrive on DatabaseThread after the new open — breaking the connection.
    {
        QWriteLocker lock(&m_symbolContextsLock);
        for (auto it = m_symbolContexts.begin(); it != m_symbolContexts.end(); ++it)
        {
            if (SymbolContext* instrument = it.value(); instrument)
            {
                DEBUG << "Deleting stock instrument for" << instrument->symbol;
                delete instrument;
            }
        }
        m_symbolContexts.clear();
    }

    INFO << "All stock instruments deleted";
}

void MainAlgo::stopAllStrategies()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    INFO << "Stopping all strategies for mode transition";
    m_strategyManager->stopAllStrategies();
    INFO << "All strategies stopped";
}

void MainAlgo::restoreStrategiesState()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    m_strategyManager->restoreStrategiesState();
}

void MainAlgo::createAndSetDisplayedSymbolContext(const QString& p_symbol)
{
    INFO << "Creating and setting displayed stock instrument for" << p_symbol;

    // Create new stock instrument
    auto* newInstrument = new SymbolContext(p_symbol, this);
    Q_CHECK_PTR(newInstrument);

    m_symbolContexts[p_symbol] = newInstrument;
    m_currentDisplayedSymbolContext = newInstrument;

    // Claim display reference (matches the release in onSelectDisplayedStock)
    ++newInstrument->m_refCount;

    // Subscribe to data for the new symbol
    if (MainApp::isInReplayMode())
    {
        if (!DBClient::getInstance()->addReplaySymbol(p_symbol))
            WARNING << "No replay data for" << p_symbol << "- live bars will not flow";
    }
    else
    {
        auto* dbClient = DBClient::getInstance();
        if (dbClient->getConnectionState() == DBClient::ConnectionState::Connected)
            dbClient->subscribeLive(p_symbol);
    }

    // No snapshot-writing connections needed here — SymbolContext always populates
    // its own DisplaySnapshot. The GUI reads from it at 30 Hz.

    INFO << "Stock instrument created and set as displayed for" << p_symbol;
}

void MainAlgo::processStrategyLog(const StrategyLogEntry& p_entry)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    OBJ_ASSUME_FALSE(p_entry.symbol.isEmpty());
    OBJ_ASSUME_FALSE(p_entry.message.isEmpty());

    OrdersDatabase::getInstance()->insertStrategyLog(p_entry);
    emit strategyLogEmitted(p_entry);
}
