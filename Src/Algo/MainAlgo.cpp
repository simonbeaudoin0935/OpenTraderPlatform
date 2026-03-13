#include <QRunnable>
#include <QThread>
#include <QTimer>
#include <QSocketNotifier>
#include <QCoreApplication>
#include <unistd.h>

#include "MainAlgo.h"
#include "MainApp.h"
#include "StrategyManager.h"
#include "StrategySignalHandler.h"
#include "TSClient.h"
#include "DBClient.h"
#include "Logging.h"
#include "Assume.h"
#include "LTTng/LTTngTracepoints.h"
#include "OrderEmulator.h"
#include "CONSTANTS.h"
#include "OrdersDatabase.h"

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


MainAlgo::MainAlgo() : m_strategyManager(this)
{
    thread.setObjectName("MainAlgoThread");

    this->moveToThread(&thread);

    this->setObjectName("MainAlgo");

    connect(&thread, &QThread::started, this, &MainAlgo::onThreadStarted);

    DEBUG << "Singleton instance created";
}

MainAlgo::~MainAlgo()
{
    DEBUG << "MainAlgo destructor - stopping thread";

    // Thread affinity assertion - destructor must be called from main thread
    OBJ_ASSUME_EQUAL(QThread::currentThread(), QCoreApplication::instance()->thread());

    // CRITICAL: Destroy all thread-owned objects that have QTimer members on the MainAlgo
    // thread BEFORE calling thread.quit(). If we destroy them from the main thread after
    // the thread has stopped, Qt warns "Timers cannot be stopped from another thread".
    // The MainAlgo event loop is still running at this point, so BlockingQueuedConnection is safe.
    QMetaObject::invokeMethod(
        this,
        [this]()
        {
            // Stop GUI throttle timer on its own thread
            m_guiThrottleTimer.stop();

            // Stop and destroy balance polling timer on its own thread
            m_balancePollingTimer.reset();
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

    // Now safe to cleanup QSocketNotifier and signal handler (thread is stopped)
    m_crashNotifier.reset();
    StrategySignalHandler::cleanup();

    // StrategyManager will be destroyed automatically via composition

    DEBUG << "Destroyed singleton instance";
}

void MainAlgo::start()
{
    thread.start();
}

void MainAlgo::onThreadStarted()
{
    m_balancePollingTimer = std::make_unique<QTimer>(this);

    connect(m_balancePollingTimer.get(), &QTimer::timeout, this, &MainAlgo::requestBalance, Qt::UniqueConnection);

    // GUI throttle timer — fires periodically when AsFastAsPossible mode is active
    // to flush buffered GUI updates at a capped rate
    m_guiThrottleTimer.setInterval(ReplayConstants::GUI_THROTTLE_INTERVAL_MS);
    connect(&m_guiThrottleTimer, &QTimer::timeout, this, &MainAlgo::onGuiThrottleTimerTick, Qt::UniqueConnection);

    // Initialize signal handler system (set up crash notification pipe)
    StrategySignalHandler::initialize();

    // Set up socket notifier to monitor crash pipe
    int crashFd = StrategySignalHandler::getCrashNotificationFd();

    OBJ_ASSUME_DIFF(crashFd, -1);

    m_crashNotifier = std::make_unique<QSocketNotifier>(crashFd, QSocketNotifier::Read, this);
    auto c = connect(m_crashNotifier.get(),
                     &QSocketNotifier::activated,
                     this,
                     &MainAlgo::onStrategyCrashNotified,
                     Qt::UniqueConnection);

    OBJ_ASSUME_TRUE(c);

    DEBUG << "Installed crash notification handler";


    // Connect MainAlgo signals to StrategyManager for data broadcasting
    // Bars: route to strategies monitoring the symbol
    connect(this,
            &MainAlgo::displayedStockReceivedNewBar,
            &m_strategyManager,
            &StrategyManager::onBarReceived,
            Qt::QueuedConnection);

    // Market depth quotes: route to strategies monitoring the symbol
    connect(this,
            &MainAlgo::displayedStockReceivedNewLevel2,
            &m_strategyManager,
            &StrategyManager::onLevel2Received,
            Qt::QueuedConnection);

    // Orders: route only to strategy that placed the order
    connect(this,
            &MainAlgo::receivedNewOrder,
            &m_strategyManager,
            &StrategyManager::onMainAlgoOrderUpdated,
            Qt::QueuedConnection);

    // Positions: route only to strategy that placed the order
    connect(this,
            &MainAlgo::receivedNewPosition,
            &m_strategyManager,
            &StrategyManager::onMainAlgoPositionUpdated,
            Qt::QueuedConnection);

    // Balance: broadcast to all strategies
    connect(this,
            &MainAlgo::balanceUpdated,
            &m_strategyManager,
            &StrategyManager::onMainAlgoBalanceUpdated,
            Qt::QueuedConnection);

    // Centralized routing: all DBClient market data → MainAlgo → SymbolContext actor queues
    connect(DBClient::getInstance(), &DBClient::newLevel2, this, &MainAlgo::onNewLevel2Received);
    connect(DBClient::getInstance(), &DBClient::newTrade, this, &MainAlgo::onNewTradeReceived);

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

// ── Centralized routing slots ──────────────────────────────────────────────

void MainAlgo::onNewLevel2Received(const QString& p_symbol, const Level2& p_level2)
{
    auto sc = m_symbolContexts.value(p_symbol);
    if (!sc.isNull())
        sc->enqueueLevel2(p_level2);
}

void MainAlgo::onNewTradeReceived(const QString& p_symbol, const Trade& p_trade)
{
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


    // If there is a current selected stock for display, disconnect its receivedNew* signals from the main algo emition
    if (m_currentDisplayedSymbolContext != nullptr)
    {
        // Selecting the same stock as currently selected. No action taken.
        OBJ_ASSUME_DIFF(m_currentDisplayedSymbolContext->symbol, symbol);

        disconnect(&m_currentDisplayedSymbolContext->barReceiver,
                   &BarReceiver::receivedNewBar,
                   this,
                   &MainAlgo::onDisplayedBarReceived);

        disconnect(&m_currentDisplayedSymbolContext->m_barAggregator,
                   &BarAggregator::barUpdated,
                   this,
                   &MainAlgo::onAggregatorBarUpdated);

        disconnect(&m_currentDisplayedSymbolContext->m_barAggregator,
                   &BarAggregator::barClosed,
                   this,
                   &MainAlgo::onAggregatorBarClosed);

        disconnect(&m_currentDisplayedSymbolContext->m_live10sBarAccumulator,
                   &LiveBarAccumulator::barUpdated,
                   this,
                   nullptr);
        disconnect(&m_currentDisplayedSymbolContext->m_live10sBarAccumulator,
                   &LiveBarAccumulator::barClosed,
                   this,
                   nullptr);

        disconnect(&m_currentDisplayedSymbolContext->m_level2Receiver,
                   &Level2Receiver::receivedNewLevel2,
                   this,
                   &MainAlgo::onDisplayedLevel2Received);

        // Disconnect only the display-symbol trade forwarding lambda.
        // Do NOT use disconnect(DBClient, signal, this, nullptr) — that would also remove
        // the permanent onNewTradeReceived routing connection set up in onThreadStarted.
        QObject::disconnect(m_displayTradeConnection);
        m_displayTradeConnection = {};

        // Clean up or detach the previous SymbolContext
        QString oldSymbol = m_currentDisplayedSymbolContext->symbol;
        QPointer<SymbolContext> oldContext = m_currentDisplayedSymbolContext;
        m_currentDisplayedSymbolContext = nullptr;

        if (m_strategyManager.isSymbolClaimed(oldSymbol))
        {
            // A strategy still owns this symbol — keep the SymbolContext alive
            // in the map so switching back reuses it with live accumulator state.
            DEBUG << "Keeping SymbolContext alive for strategy-claimed symbol:" << oldSymbol;
        }
        else
        {
            // No strategy needs this symbol — free its resources
            int removed = m_symbolContexts.remove(oldSymbol);
            OBJ_ASSUME_EQUAL(removed, 1);
            oldContext->deleteLater();
            DEBUG << "Scheduled cleanup for SymbolContext:" << oldSymbol;
        }
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

        m_symbolContexts.insert(symbol, m_currentDisplayedSymbolContext);
        DEBUG << "onSelectDisplayedStock: created new SymbolContext for" << symbol;
    }
    DEBUG << "onSelectDisplayedStock: wiring display signals for" << symbol
          << "| active SymbolContexts:" << m_symbolContexts.keys();

    // Redoo the plumbing we disconnected at the top of this function
    connect(&m_currentDisplayedSymbolContext->barReceiver,
            &BarReceiver::receivedNewBar,
            this,
            &MainAlgo::onDisplayedBarReceived);

    // Forward BarAggregator higher-TF updates so the chart can show live higher-TF candles
    connect(&m_currentDisplayedSymbolContext->m_barAggregator,
            &BarAggregator::barUpdated,
            this,
            &MainAlgo::onAggregatorBarUpdated);

    connect(&m_currentDisplayedSymbolContext->m_barAggregator,
            &BarAggregator::barClosed,
            this,
            &MainAlgo::onAggregatorBarClosed);

    // Forward 10s bar updates via the aggregator signal path (reuses GUIFrontend's existing TF filter)
    connect(&m_currentDisplayedSymbolContext->m_live10sBarAccumulator,
            &LiveBarAccumulator::barUpdated,
            this,
            [this, symbol](const QString&, const Bar& bar)
            { emit displayedStockAggregatorBarUpdated(symbol, TimeFrame::TEN_SECONDS, bar); });

    connect(&m_currentDisplayedSymbolContext->m_live10sBarAccumulator,
            &LiveBarAccumulator::barClosed,
            this,
            [this, symbol](const QString&, const Bar& bar)
            { emit displayedStockAggregatorBarClosed(symbol, TimeFrame::TEN_SECONDS, bar); });

    connect(&m_currentDisplayedSymbolContext->m_level2Receiver,
            &Level2Receiver::receivedNewLevel2,
            this,
            &MainAlgo::onDisplayedLevel2Received);

    // Forward trades for displayed symbol to FrontEnd (store handle for clean targeted disconnect)
    m_displayTradeConnection = connect(DBClient::getInstance(),
                                       &DBClient::newTrade,
                                       this,
                                       [this, symbol](const QString& sym, const Trade& trade)
                                       {
                                           if (sym == symbol)
                                               onDisplayedTradeReceived(sym, trade);
                                       });
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
                          &MainAlgo::receivedNewPosition,
                          Qt::UniqueConnection);
        OBJ_ASSUME_TRUE(c1);

        auto c2 = connect(m_positionReceiver,
                          &PositionsReceiver::receivedNewPosition,
                          this,
                          &MainAlgo::onReceivedNewPosition,
                          Qt::UniqueConnection);
        OBJ_ASSUME_TRUE(c2);

        auto c3 = connect(m_positionReceiver,
                          &PositionsReceiver::positionDeleted,
                          this,
                          &MainAlgo::onPositionDeleted,
                          Qt::UniqueConnection);
        OBJ_ASSUME_TRUE(c3);

        auto c4 = connect(m_positionReceiver,
                          &PositionsReceiver::loadedPositionsFromDatabase,
                          this,
                          &MainAlgo::onLoadedPositionsFromDatabase,
                          Qt::UniqueConnection);
        OBJ_ASSUME_TRUE(c4);
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
        &m_strategyManager,
        [this, strategyID, order]() { m_strategyManager.onOrderUpdatedForStrategy(strategyID, order); },
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
    int depth = 0;
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
    int depth = 0;
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
    m_level2Receiver.onReceivedNewLevel2(p_level2);
}

void SymbolContext::processTrade(const Trade& p_trade)
{
    m_liveBarAccumulator.onNewTrade(symbol, p_trade);
    m_live10sBarAccumulator.onNewTrade(symbol, p_trade);
    emit receivedNewTrade(symbol, p_trade);
}

uint64_t MainAlgo::getNextRequestId()
{
    // Thread-safe atomic increment returns the old value, so we need pre-increment semantics
    // Actually ++operator does pre-increment by default for atomic
    return ++m_requestIdCounter;
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

        // If symbol is already loaded (displayed stock), data is already flowing via
        // the centralized routing (DBClient → MainAlgo → SymbolContext queue).
        if (m_symbolContexts.contains(p_symbol))
        {
            m_strategyManager.connectSymbolToStrategy(p_strategyID, p_symbol, nullptr);
            p_promise->addResult(true);
            p_promise->finish();
            return;
        }

        // New secondary symbol: create SymbolContext — data flows automatically via
        // DBClient::newLevel2/newTrade → MainAlgo routing → SymbolContext queue.
        // Also open its replay files in DBClient so records get emitted.
        auto* instrument = new SymbolContext(p_symbol, this);
        Q_CHECK_PTR(instrument);
        m_symbolContexts.insert(p_symbol, instrument);

        // Open replay data files for this symbol (non-blocking, same-thread call)
        if (!DBClient::getInstance()->addReplaySymbol(p_symbol))
        {
            WARNING << "addReplaySymbol failed for" << p_symbol << "- no data files found";
        }

        // Wire bar-close to OrderEmulator if replay is active
        if (OrderEmulator* emulator = TSClient::getInstance()->getOrderEmulator())
            connectBarCloseToOrderEmulator(instrument, emulator);

        m_strategyManager.connectSymbolToStrategy(p_strategyID, p_symbol, instrument);

        INFO << "Secondary symbol SymbolContext created for" << p_symbol << "(replay data routed via DBClient)";
    }
    else
    {
        // Live/sim mode: create SymbolContext if needed and subscribe
        if (!m_symbolContexts.contains(p_symbol) || m_symbolContexts[p_symbol].isNull())
        {
            auto* instrument = new SymbolContext(p_symbol, this);
            Q_CHECK_PTR(instrument);
            m_symbolContexts.insert(p_symbol, instrument);

            auto* dbClient = DBClient::getInstance();
            if (dbClient->getConnectionState() == DBClient::ConnectionState::Connected)
            {
                dbClient->subscribeLive(p_symbol);
            }
        }

        m_strategyManager.connectSymbolToStrategy(p_strategyID, p_symbol, nullptr);
    }

    p_promise->addResult(true);
    p_promise->finish();
}

void MainAlgo::processClaimSymbols(const QString& p_strategyID,
                                   const QStringList& p_symbols,
                                   std::shared_ptr<QPromise<QStringList>> p_promise)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    m_strategyManager.processClaimSymbols(p_strategyID, p_symbols, p_promise);
}

void MainAlgo::onStrategyCrashNotified()
{
    // Read crash notification from pipe
    // The pipe was set up by StrategySignalHandler::initialize()
    // and monitored by QSocketNotifier on this MainAlgo thread

    OBJ_ASSUME_TRUE(m_crashNotifier != nullptr);

    // Read from the pipe - keep reading until it's empty
    const int fd = m_crashNotifier->socket();

    OBJ_ASSUME_DIFF(fd, -1);

    struct CrashNotification
    {
        char strategyID[256];
        char errorMsg[256];
        int signal;
    };

    CrashNotification notif;
    ssize_t result = read(fd, &notif, sizeof(notif));

    if (result != static_cast<ssize_t>(sizeof(notif)))
    {
        CRITICAL << "Failed to read crash notification from pipe:" << strerror(errno);
        return;
    }

    QString strategyID = QString::fromStdString(std::string(notif.strategyID));
    QString errorMsg = QString::fromStdString(std::string(notif.errorMsg));

    CRITICAL << "Strategy thread crashed with signal:" << strategyID << "-" << errorMsg;

    // Now safely call StrategyManager::markStrategyFailed on the same thread
    m_strategyManager.markStrategyFailed(strategyID, errorMsg);
}

void MainAlgo::onAggregatorBarUpdated(TimeFrame tf, const Bar& bar)
{
    if (m_currentDisplayedSymbolContext == nullptr)
        return;

    if (m_guiThrottleActive)
    {
        m_pendingAggregatorBarUpdate = {tf, bar};
        m_pendingAggregatorSymbol = m_currentDisplayedSymbolContext->symbol;
        return;
    }
    emit displayedStockAggregatorBarUpdated(m_currentDisplayedSymbolContext->symbol, tf, bar);
}

void MainAlgo::onAggregatorBarClosed(TimeFrame tf, const Bar& bar)
{
    // Bar closes are infrequent (once per minute boundary per TF) — never throttle
    if (m_currentDisplayedSymbolContext != nullptr)
        emit displayedStockAggregatorBarClosed(m_currentDisplayedSymbolContext->symbol, tf, bar);
}

// ---------------------------------------------------------------------------
// GUI throttle gate slots
// ---------------------------------------------------------------------------

void MainAlgo::onDisplayedBarReceived(const QString& symbol, const Bar& bar)
{
    if (m_guiThrottleActive)
    {
        m_pendingBar = {symbol, bar};
        return;
    }
    emit displayedStockReceivedNewBar(symbol, bar);
}

void MainAlgo::onDisplayedLevel2Received(const QString& symbol, const Level2& level2)
{
    if (m_guiThrottleActive)
    {
        m_pendingLevel2 = {symbol, level2};
        return;
    }
    emit displayedStockReceivedNewLevel2(symbol, level2);
}

void MainAlgo::onDisplayedTradeReceived(const QString& symbol, const Trade& trade)
{
    if (m_guiThrottleActive)
    {
        m_pendingTrade = {symbol, trade};
        return;
    }
    emit displayedStockReceivedNewTrade(symbol, trade);
}

void MainAlgo::onReplayTimeReceived(const QDateTime& time)
{
    if (m_guiThrottleActive)
    {
        m_pendingReplayTime = time;
        return;
    }
    emit replayTimeUpdated(time);
}

void MainAlgo::onGuiThrottleTimerTick()
{
    if (m_pendingBar.has_value())
    {
        emit displayedStockReceivedNewBar(m_pendingBar->first, m_pendingBar->second);
        m_pendingBar.reset();
    }

    if (m_pendingLevel2.has_value())
    {
        emit displayedStockReceivedNewLevel2(m_pendingLevel2->first, m_pendingLevel2->second);
        m_pendingLevel2.reset();
    }

    if (m_pendingTrade.has_value())
    {
        emit displayedStockReceivedNewTrade(m_pendingTrade->first, m_pendingTrade->second);
        m_pendingTrade.reset();
    }

    if (m_pendingAggregatorBarUpdate.has_value())
    {
        emit displayedStockAggregatorBarUpdated(m_pendingAggregatorSymbol,
                                                m_pendingAggregatorBarUpdate->first,
                                                m_pendingAggregatorBarUpdate->second);
        m_pendingAggregatorBarUpdate.reset();
    }

    if (m_pendingReplayTime.has_value())
    {
        emit replayTimeUpdated(*m_pendingReplayTime);
        m_pendingReplayTime.reset();
    }
}

void MainAlgo::activateGuiThrottle()
{
    if (m_guiThrottleActive)
        return;

    INFO << "Activating GUI throttle for AsFastAsPossible replay mode (" << ReplayConstants::GUI_THROTTLE_INTERVAL_MS
         << "ms interval)";

    m_guiThrottleActive = true;
    m_guiThrottleTimer.start();
}

void MainAlgo::deactivateGuiThrottle()
{
    if (!m_guiThrottleActive)
        return;

    INFO << "Deactivating GUI throttle";

    m_guiThrottleTimer.stop();
    m_guiThrottleActive = false;

    // Flush any remaining buffered data so nothing is lost
    onGuiThrottleTimerTick();
}

void MainAlgo::onReplayEndReached()
{
    INFO << "Replay ended, pausing heartbeat timers to prevent stream timeout";

    deactivateGuiThrottle();

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

    if (p_speed == Playback::Speed::AsFastAsPossible)
        activateGuiThrottle();

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

    if (p_speed == Playback::Speed::AsFastAsPossible)
        activateGuiThrottle();

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

    // The display-symbol trade forwarding is handled by m_displayTradeConnection (set in
    // createAndSetDisplayedSymbolContext / onSelectDisplayedStock). No extra lambda here.

    INFO << "Replay signals connected for" << p_symbol;
}

void MainAlgo::exitReplayMode()
{
    INFO << "MainAlgo exiting replay mode";

    deactivateGuiThrottle();

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
    DBClient::getInstance()->setReplaySpeed(p_speed);

    // Sync speed to OrderEmulator so latency is scaled correctly
    if (OrderEmulator* emulator = TSClient::getInstance()->getOrderEmulator())
    {
        emulator->setReplaySpeed(static_cast<int>(p_speed));
    }

    // Toggle GUI throttle based on speed
    if (p_speed == Playback::Speed::AsFastAsPossible)
        activateGuiThrottle();
    else
        deactivateGuiThrottle();
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

void MainAlgo::deleteAllSymbolContext()
{
    INFO << "Deleting all stock instruments for clean mode transition";

    // Clear the displayed pointer first
    m_currentDisplayedSymbolContext = nullptr;

    // Delete instruments directly (not deleteLater) so that each BarCache destructor
    // queues closeDatabase to DatabaseThread before the next createAndSetDisplayedSymbolContext
    // queues openDatabase. deleteLater would defer destruction past the next openDatabase call,
    // causing the DB close to arrive on DatabaseThread after the new open — breaking the connection.
    for (auto it = m_symbolContexts.begin(); it != m_symbolContexts.end(); ++it)
    {
        if (SymbolContext* instrument = it.value(); instrument)
        {
            DEBUG << "Deleting stock instrument for" << instrument->symbol;
            delete instrument;
        }
    }
    m_symbolContexts.clear();

    INFO << "All stock instruments deleted";
}

void MainAlgo::stopAllStrategies()
{
    INFO << "Stopping all strategies for mode transition";
    m_strategyManager.stopAllStrategies();
    INFO << "All strategies stopped";
}

void MainAlgo::restoreStrategiesState()
{
    m_strategyManager.restoreStrategiesState();
}

void MainAlgo::createAndSetDisplayedSymbolContext(const QString& p_symbol)
{
    INFO << "Creating and setting displayed stock instrument for" << p_symbol;

    // Create new stock instrument
    auto* newInstrument = new SymbolContext(p_symbol, this);
    Q_CHECK_PTR(newInstrument);

    m_symbolContexts[p_symbol] = newInstrument;
    m_currentDisplayedSymbolContext = newInstrument;

    // Subscribe to live data if DBClient is connected (not in replay mode)
    if (!MainApp::isInReplayMode())
    {
        auto* dbClient = DBClient::getInstance();
        if (dbClient->getConnectionState() == DBClient::ConnectionState::Connected)
        {
            dbClient->subscribeLive(p_symbol);
        }
    }

    // Connect bar signals for the new displayed instrument
    bool connected = connect(&m_currentDisplayedSymbolContext->barReceiver,
                             &BarReceiver::receivedNewBar,
                             this,
                             &MainAlgo::onDisplayedBarReceived,
                             Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    // Forward higher-TF aggregator events so the chart can show live higher-TF candles
    connected = connect(&m_currentDisplayedSymbolContext->m_barAggregator,
                        &BarAggregator::barUpdated,
                        this,
                        &MainAlgo::onAggregatorBarUpdated,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected = connect(&m_currentDisplayedSymbolContext->m_barAggregator,
                        &BarAggregator::barClosed,
                        this,
                        &MainAlgo::onAggregatorBarClosed,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected = connect(&m_currentDisplayedSymbolContext->m_level2Receiver,
                        &Level2Receiver::receivedNewLevel2,
                        this,
                        &MainAlgo::onDisplayedLevel2Received,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    // Forward trades for displayed symbol to FrontEnd (store handle for clean targeted disconnect)
    m_displayTradeConnection = connect(DBClient::getInstance(),
                                       &DBClient::newTrade,
                                       this,
                                       [this, p_symbol](const QString& sym, const Trade& trade)
                                       {
                                           if (sym == p_symbol)
                                               onDisplayedTradeReceived(sym, trade);
                                       });
    ASSUME_TRUE(m_displayTradeConnection);

    // Connect to strategy manager for bar delivery
    connected = connect(&m_currentDisplayedSymbolContext->barReceiver,
                        &BarReceiver::receivedNewBar,
                        &m_strategyManager,
                        &StrategyManager::onBarReceived,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected = connect(&m_currentDisplayedSymbolContext->m_level2Receiver,
                        &Level2Receiver::receivedNewLevel2,
                        &m_strategyManager,
                        &StrategyManager::onLevel2Received,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);

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
