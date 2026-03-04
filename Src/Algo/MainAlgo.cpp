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
#include "OrderEmulator.h"
#include "CONSTANTS.h"

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

    // Stop balance polling timer if it exists
    // Note: We're in the destructor, so we can't use QMetaObject::invokeMethod
    // since the thread might already be stopping. Just stop the timer directly.
    if (m_balancePollingTimer && m_balancePollingTimer->isActive())
    {
        m_balancePollingTimer->stop();
        DEBUG << "Stopped balance polling timer in destructor";
    }

    // CRITICAL: Stop thread BEFORE destroying thread-owned objects to prevent cross-thread access
    // Request thread to stop
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
}

/**
 * @brief Handles the selection of a new stock for display.
 *
 * This function is called when the user selects a different stock to display in the UI.
 * It manages the lifecycle of StockInstruments, disconnecting signals from the previous stock,
 * cleaning up resources (such as closing data streams), and setting up the new stock's
 * bar cache and market depth quote receivers with appropriate signal connections.
 *
 * If a stock was previously selected, it ensures proper cleanup by:
 * - Disconnecting signals from the old stock's BarCache and Level2Receiver
 * - Closing any active streams for the old stock
 * - Removing the old StockInstruments from the map and scheduling its deletion
 *
 * For the new stock, it either reuses an existing StockInstruments if the symbol is already
 * in the map, or creates a new one. It then connects the new stock's signals to emit
 * MainAlgo's signals for bar and market depth updates.
 *
 * @param symbol The stock symbol to select for display. Must be a valid stock symbol.
 *
 * @note This method must be called from the MainAlgo thread (QThread::currentThread() == &thread).
 * @note Assumes that if a stock is currently displayed, the new symbol is different.
 * @note Uses Qt's parent-child ownership for memory management of StockInstruments.
 */
void MainAlgo::onSelectDisplayedStock(const QString& symbol)
{
    // Make sure that this method gets Qt::InvokeMethod'ed if called from another thread
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);


    // If there is a current selected stock for display, disconnect its receivedNew* signals from the main algo emition
    if (currentDisplayedStockInstrument != nullptr)
    {
        // Selecting the same stock as currently selected. No action taken.
        OBJ_ASSUME_DIFF(currentDisplayedStockInstrument->symbol, symbol);

        disconnect(&currentDisplayedStockInstrument->barReceiver,
                   &BarReceiver::receivedNewBar,
                   this,
                   &MainAlgo::displayedStockReceivedNewBar);

        disconnect(&currentDisplayedStockInstrument->m_level2Receiver,
                   &Level2Receiver::receivedNewLevel2,
                   this,
                   &MainAlgo::displayedStockReceivedNewLevel2);

        // Disconnect trade forwarding from DBClient for old symbol
        disconnect(DBClient::getInstance(), &DBClient::newTrade, this, nullptr);

        // Clean up the previous stock instrument to free resources (streams, database connections)
        QString oldSymbol = currentDisplayedStockInstrument->symbol;
        QPointer<StockInstruments> oldInstrument = currentDisplayedStockInstrument;

        currentDisplayedStockInstrument = nullptr;

        int removed = stockInstruments.remove(oldSymbol);
        OBJ_ASSUME_EQUAL(removed, 1); // Should always remove exactly one entry

        // Schedule deletion after streams are closed
        oldInstrument->deleteLater();
        DEBUG << "Scheduled cleanup for StockInstrument:" << oldSymbol;
    }

    // Change the stock selected pointer to the new selected stock
    if (stockInstruments.contains(symbol))
    {
        currentDisplayedStockInstrument = stockInstruments[symbol];
    }
    else
    {
        currentDisplayedStockInstrument = new StockInstruments(symbol, this); // Pass 'this' as parent
        Q_CHECK_PTR(currentDisplayedStockInstrument);

        stockInstruments.insert(symbol, currentDisplayedStockInstrument);
    }

    // Redoo the plumbing we disconnected at the top of this function
    connect(&currentDisplayedStockInstrument->barReceiver,
            &BarReceiver::receivedNewBar,
            this,
            &MainAlgo::displayedStockReceivedNewBar);

    connect(&currentDisplayedStockInstrument->m_level2Receiver,
            &Level2Receiver::receivedNewLevel2,
            this,
            &MainAlgo::displayedStockReceivedNewLevel2);

    // Forward trades for displayed symbol to FrontEnd
    connect(DBClient::getInstance(),
            &DBClient::newTrade,
            this,
            [this, symbol](const QString& sym, const Trade& trade)
            {
                if (sym == symbol)
                    emit displayedStockReceivedNewTrade(sym, trade);
            });
}

BarCache::GetBarsResult_t MainAlgo::requestMissingBarsDisplayedStock(QDate date, QTime first, QTime last)
{
    DEBUG << "Requested bars from current displayed stock cache: " << first << " to " << last;

    OBJ_ASSUME_LTE(first, last); // The Equal in less than equal is for when the program is launched at 4:02 AM

    if (currentDisplayedStockInstrument == nullptr)
    {
        // Instrument not yet initialized (e.g., setSymbol fired before onSelectDisplayedStock arrived).
        // Return empty result — checkForMissingBars will retry on next scroll/zoom.
        DEBUG << "No instrument ready yet, returning empty bars";
        return std::make_shared<QVector<Bar>>();
    }

    return currentDisplayedStockInstrument->barCache.getBars(date, first, last);
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

        auto c3 = connect(m_orderReceiver,
                          &OrdersReceiver::receivedNewOrder,
                          this,
                          &MainAlgo::receivedNewOrder,
                          Qt::UniqueConnection);
        OBJ_ASSUME_TRUE(c3);

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
    Q_UNUSED(account);

    DEBUG << "onReceivedNewOrder: orderID=" << order.getOrderID()
          << "status=" << static_cast<int>(order.getOrderStatus()) << "mappings_size=" << m_orderMappings.size();

    // Lookup which strategy placed this order
    auto strategyIt = m_orderMappings.find(order.getOrderID());

    if (strategyIt == m_orderMappings.end())
    {
        // This order does not belong to any strategy we know about
        WARNING << "Received order update for order ID:" << order.getOrderID() << "which has no associated strategy";
        return;
    }

    // This order belongs to a strategy - route it to that strategy only
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
    if (currentDisplayedStockInstrument)
    {
        return currentDisplayedStockInstrument->symbol;
    }
    return QString();
}

void MainAlgo::requestBalance()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

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

StockInstruments::StockInstruments(const QString& p_symbol, QObject* p_parent)
    : QObject(p_parent)
    , symbol(p_symbol)
    , barCache(p_symbol, this)
    , barReceiver(p_symbol, this)
    , m_level2Receiver(p_symbol, this)
    , m_level1Receiver(p_symbol, this)
    , m_liveBarAccumulator(this)
{
    this->setObjectName("StockInstrument::" + p_symbol);

    // Connect BarReceiver to BarCache for storage
    bool connected = connect(&barReceiver,
                             &BarReceiver::receivedNewBar,
                             &barCache,
                             [this](const QString&, const Bar& bar) { barCache.storeBar(bar); });
    OBJ_ASSUME_TRUE(connected);

    // Wire LiveBarAccumulator::barClosed → BarReceiver::receivedNewBar
    connected =
        connect(&m_liveBarAccumulator, &LiveBarAccumulator::barClosed, &barReceiver, &BarReceiver::receivedNewBar);
    OBJ_ASSUME_TRUE(connected);

    // Wire LiveBarAccumulator::barUpdated → BarReceiver::receivedNewBar (in-progress candle)
    connected =
        connect(&m_liveBarAccumulator, &LiveBarAccumulator::barUpdated, &barReceiver, &BarReceiver::receivedNewBar);
    OBJ_ASSUME_TRUE(connected);

    // In replay mode, data comes from ReplayEngine (connected by MainAlgo::connectReplaySignals)
    // In live mode, data comes from DBClient signals
    if (!MainApp::isInReplayMode())
    {
        auto* dbClient = DBClient::getInstance();

        // Wire DBClient::newLevel2 → Level2Receiver (filtered by symbol)
        connected = connect(dbClient,
                            &DBClient::newLevel2,
                            this,
                            [this](const QString& sym, const Level2& level2)
                            {
                                if (sym == symbol)
                                    m_level2Receiver.onReceivedNewLevel2(level2);
                            });
        OBJ_ASSUME_TRUE(connected);

        // Wire DBClient::newLevel1 → Level1Receiver (filtered by symbol)
        connected = connect(dbClient,
                            &DBClient::newLevel1,
                            this,
                            [this](const QString& sym, const Level1& level1)
                            {
                                if (sym == symbol)
                                    m_level1Receiver.onReceivedNewLevel1(level1);
                            });
        OBJ_ASSUME_TRUE(connected);

        // Wire DBClient::newTrade → LiveBarAccumulator (filtered by symbol)
        connected = connect(dbClient,
                            &DBClient::newTrade,
                            &m_liveBarAccumulator,
                            [this](const QString& sym, const Trade& trade)
                            {
                                if (sym == symbol)
                                    m_liveBarAccumulator.onNewTrade(symbol, trade);
                            });
        OBJ_ASSUME_TRUE(connected);

        // Subscribe to live data if DBClient is connected
        if (dbClient->getConnectionState() == DBClient::ConnectionState::Connected)
        {
            dbClient->subscribeLive(p_symbol);
        }
    }

    DEBUG << "New instance";
}

StockInstruments::~StockInstruments()
{
    DEBUG << "Deleted instance";
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
            }
        }

        // TODO: Emit GUI signal if this order is for the displayed stock
        // TODO: Route order result to strategy via SDK
    }
    else
    {
        // Order placement failed
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

        // If symbol is already loaded (displayed stock), wire its instrument + primary engine
        if (stockInstruments.contains(p_symbol) && !m_secondaryReplayEngines.contains(p_symbol))
        {
            m_strategyManager.connectSymbolToStrategy(p_strategyID,
                                                      p_symbol,
                                                      stockInstruments[p_symbol],
                                                      m_replayEngine);
            p_promise->addResult(true);
            p_promise->finish();
            return;
        }

        // New secondary symbol: create StockInstruments + secondary ReplayEngine
        StockInstruments* instrument = nullptr;
        if (!stockInstruments.contains(p_symbol) || stockInstruments[p_symbol].isNull())
        {
            instrument = new StockInstruments(p_symbol, this);
            Q_CHECK_PTR(instrument);
            stockInstruments.insert(p_symbol, instrument);
        }
        else
        {
            instrument = stockInstruments[p_symbol];
        }

        auto* secondaryEngine = new ReplayEngine(this);
        m_secondaryReplayEngines.insert(p_symbol, secondaryEngine);

        // Wire secondary engine → StockInstruments (Level2 + Trades → bars)
        connectSecondaryReplaySignals(p_symbol, secondaryEngine, instrument);

        // Wire StockInstruments + secondary engine trade events → strategy adapter
        m_strategyManager.connectSymbolToStrategy(p_strategyID, p_symbol, instrument, secondaryEngine);

        // Start secondary replay from the same date/time/speed as the primary
        secondaryEngine->startReplay(p_symbol, m_replayDate, m_replayStartTime, m_replaySpeed);

        INFO << "Secondary replay started for" << p_symbol << "at" << m_replayDate.toString(Qt::ISODate);
    }
    else
    {
        // Live/sim mode: add symbol to monitored set
        // (data for arbitrary symbols via live DBClient streams is a future enhancement;
        //  for now the strategy must use the symbol that's already streaming)
        m_strategyManager.connectSymbolToStrategy(p_strategyID, p_symbol, nullptr, nullptr);
    }

    p_promise->addResult(true);
    p_promise->finish();
}

void MainAlgo::connectSecondaryReplaySignals(const QString& p_symbol,
                                             ReplayEngine* p_engine,
                                             StockInstruments* p_instrument)
{
    OBJ_ASSUME_DIFF(p_engine, nullptr);
    OBJ_ASSUME_DIFF(p_instrument, nullptr);

    // Replay Level2 → Level2Receiver
    bool connected = connect(p_engine,
                             &ReplayEngine::replayLevel2,
                             &p_instrument->m_level2Receiver,
                             [p_instrument](const QString& /*sym*/, const Level2& l2)
                             { p_instrument->m_level2Receiver.onReceivedNewLevel2(l2); });
    ASSUME_TRUE(connected);

    // Replay Trade → LiveBarAccumulator (builds bars from trades)
    connected = connect(p_engine,
                        &ReplayEngine::replayTrade,
                        &p_instrument->m_liveBarAccumulator,
                        &LiveBarAccumulator::onNewTrade);
    ASSUME_TRUE(connected);

    INFO << "Secondary replay signals connected for" << p_symbol;
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

void MainAlgo::onReplayEndReached()
{
    INFO << "Replay ended, pausing heartbeat timers to prevent stream timeout";

    // Pause heartbeat timers on ALL mock streams, not just the displayed one
    for (auto& instrument: stockInstruments)
    {
        if (instrument.isNull())
        {
            continue;
        }
        instrument->barReceiver.pauseHeartbeat();
        instrument->m_level2Receiver.pauseHeartbeat();
    }
}

void MainAlgo::enterReplayMode(const QString& p_symbol,
                               QDate p_date,
                               QTime p_startTime,
                               ReplayEngine::PlaybackSpeed p_speed)
{
    INFO << "MainAlgo entering replay mode for" << p_symbol << "on" << p_date.toString(Qt::ISODate) << "at"
         << p_startTime.toString("hh:mm:ss");

    // We assume that if we were able to click "Enter Replay Mode", then we must not already be in replay mode, so m_replayEngine should be null
    OBJ_ASSUME_TRUE(m_replayEngine == nullptr);

    // Store replay state for strategy subscription validation
    m_replayDate = p_date;
    m_replayStartTime = p_startTime;
    m_replaySpeed = p_speed;

    // Create ReplayEngine on first use (lazy init, parent=this for thread affinity)
    m_replayEngine = new ReplayEngine(this);

    // Forward signals to MainAlgo signals for UI consumption
    bool connected =
        connect(m_replayEngine, &ReplayEngine::replayStarted, this, &MainAlgo::replayStarted, Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected =
        connect(m_replayEngine, &ReplayEngine::replayStopped, this, &MainAlgo::replayStopped, Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected =
        connect(m_replayEngine, &ReplayEngine::replayPaused, this, &MainAlgo::replayPaused, Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected =
        connect(m_replayEngine, &ReplayEngine::replayResumed, this, &MainAlgo::replayResumed, Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected = connect(m_replayEngine,
                        &ReplayEngine::replayTimeUpdated,
                        this,
                        &MainAlgo::replayTimeUpdated,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected = connect(m_replayEngine,
                        &ReplayEngine::replayEndReached,
                        this,
                        &MainAlgo::replayEndReached,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    // Also handle end of replay to pause heartbeat timers
    connected = connect(m_replayEngine,
                        &ReplayEngine::replayEndReached,
                        this,
                        &MainAlgo::onReplayEndReached,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    // Connect replay data signals to StockInstruments receivers
    connectReplaySignals(p_symbol);

    DEBUG << "ReplayEngine created and connected";

    // Start replay order/position streams with simulated account
    startReplayOrderStreams();

    m_replayEngine->startReplay(p_symbol, p_date, p_startTime, p_speed);
}

void MainAlgo::enterReplayModePaused(const QString& p_symbol,
                                     QDate p_date,
                                     QTime p_startTime,
                                     ReplayEngine::PlaybackSpeed p_speed)
{
    INFO << "MainAlgo entering replay mode (paused) for" << p_symbol << "on" << p_date.toString(Qt::ISODate) << "at"
         << p_startTime.toString("hh:mm:ss");

    // Store replay state for strategy subscription validation
    m_replayDate = p_date;
    m_replayStartTime = p_startTime;
    m_replaySpeed = p_speed;

    bool isRecreatingEngine = (m_replayEngine != nullptr);

    // If ReplayEngine already exists (e.g., changing replay day), delete it first
    if (m_replayEngine != nullptr)
    {
        DEBUG << "Deleting existing ReplayEngine before creating new one";
        delete m_replayEngine;
        m_replayEngine = nullptr;
    }

    // Create ReplayEngine (same setup as enterReplayMode)
    m_replayEngine = new ReplayEngine(this);

    bool connected =
        connect(m_replayEngine, &ReplayEngine::replayStarted, this, &MainAlgo::replayStarted, Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected =
        connect(m_replayEngine, &ReplayEngine::replayStopped, this, &MainAlgo::replayStopped, Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected =
        connect(m_replayEngine, &ReplayEngine::replayPaused, this, &MainAlgo::replayPaused, Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected =
        connect(m_replayEngine, &ReplayEngine::replayResumed, this, &MainAlgo::replayResumed, Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected = connect(m_replayEngine,
                        &ReplayEngine::replayTimeUpdated,
                        this,
                        &MainAlgo::replayTimeUpdated,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected = connect(m_replayEngine,
                        &ReplayEngine::replayEndReached,
                        this,
                        &MainAlgo::replayEndReached,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    // Also handle end of replay to pause heartbeat timers
    connected = connect(m_replayEngine,
                        &ReplayEngine::replayEndReached,
                        this,
                        &MainAlgo::onReplayEndReached,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    // Connect replay data signals to StockInstruments receivers
    connectReplaySignals(p_symbol);

    DEBUG << "ReplayEngine created and connected";

    // Only setup order/position streams on first entry to replay mode
    if (!isRecreatingEngine)
    {
        startReplayOrderStreams();
    }

    // Start in paused state - emit first record then pause
    m_replayEngine->startReplayPaused(p_symbol, p_date, p_startTime, p_speed);

    // Pause heartbeat timers since we're starting in paused state
    for (auto& instrument: stockInstruments)
    {
        OBJ_ASSUME_FALSE(instrument.isNull());
        instrument->barReceiver.pauseHeartbeat();
        instrument->m_level2Receiver.pauseHeartbeat();
    }
}

void MainAlgo::connectReplaySignals(const QString& p_symbol)
{
    OBJ_ASSUME_DIFF(m_replayEngine, nullptr);

    auto it = stockInstruments.find(p_symbol);
    if (it == stockInstruments.end() || it.value().isNull())
    {
        WARNING << "No StockInstruments found for" << p_symbol << "- replay signals not connected";
        return;
    }

    StockInstruments* instrument = it.value();

    // Replay Level2 → Level2Receiver
    bool connected = connect(m_replayEngine,
                             &ReplayEngine::replayLevel2,
                             &instrument->m_level2Receiver,
                             [instrument](const QString& /*sym*/, const Level2& l2)
                             { instrument->m_level2Receiver.onReceivedNewLevel2(l2); });
    ASSUME_TRUE(connected);

    // Replay Level2 → OrderEmulator (so it has market data for order fills)
    if (OrderEmulator* emulator = TSClient::getInstance()->getOrderEmulator())
    {
        connected = connect(m_replayEngine, &ReplayEngine::replayLevel2, emulator, &OrderEmulator::updateMarketDepth);
        ASSUME_TRUE(connected);

        // Bar close price → OrderEmulator (needed by recalculatePositionPnL for P&L updates)
        // Both barUpdated (live candle) and barClosed (minute boundary) keep the price current.
        auto feedBarClose = [emulator](const QString& sym, const Bar& bar)
        { emulator->updateBarClose(sym, bar.getClose()); };

        connected = connect(&instrument->m_liveBarAccumulator, &LiveBarAccumulator::barUpdated, emulator, feedBarClose);
        ASSUME_TRUE(connected);
        connected = connect(&instrument->m_liveBarAccumulator, &LiveBarAccumulator::barClosed, emulator, feedBarClose);
        ASSUME_TRUE(connected);
    }

    // Replay Trade → LiveBarAccumulator (builds bars from trades)
    connected = connect(m_replayEngine,
                        &ReplayEngine::replayTrade,
                        &instrument->m_liveBarAccumulator,
                        &LiveBarAccumulator::onNewTrade);
    ASSUME_TRUE(connected);

    // Replay Trade → forward to FrontEnd as displayed stock trade
    connected =
        connect(m_replayEngine,
                &ReplayEngine::replayTrade,
                this,
                [this](const QString& sym, const Trade& trade) { emit displayedStockReceivedNewTrade(sym, trade); });
    ASSUME_TRUE(connected);

    INFO << "Replay signals connected for" << p_symbol;
}

void MainAlgo::exitReplayMode()
{
    INFO << "MainAlgo exiting replay mode";

    OBJ_ASSUME_DIFF(m_replayEngine, nullptr);

    m_replayEngine->stopReplay();

    // Clean up replay engine
    delete m_replayEngine;
    m_replayEngine = nullptr;
}

void MainAlgo::pauseReplay()
{
    // If we can click "Pause", replay engine must exist
    OBJ_ASSUME_DIFF(m_replayEngine, nullptr);

    m_replayEngine->pauseReplay();

    // Pause heartbeat timers on ALL mock streams to prevent timeout while paused
    for (auto& instrument: stockInstruments)
    {
        OBJ_ASSUME_FALSE(instrument.isNull());
        instrument->barReceiver.pauseHeartbeat();
        instrument->m_level2Receiver.pauseHeartbeat();
    }
}

void MainAlgo::resumeReplay()
{
    // If we can click "Resume", replay engine must exist
    OBJ_ASSUME_DIFF(m_replayEngine, nullptr);

    // Resume heartbeat timers on ALL streams before resuming replay
    for (auto& instrument: stockInstruments)
    {
        OBJ_ASSUME_FALSE(instrument.isNull());
        instrument->barReceiver.resumeHeartbeat();
        instrument->m_level2Receiver.resumeHeartbeat();
    }

    m_replayEngine->resumeReplay();
}

void MainAlgo::setReplaySpeed(ReplayEngine::PlaybackSpeed p_speed)
{
    if (m_replayEngine != nullptr)
    {
        m_replayEngine->setSpeed(p_speed);
    }
}

void MainAlgo::pauseLiveStreams()
{
    INFO << "Pausing live streams for replay mode";

    // These receivers must exist when entering replay mode from live
    OBJ_ASSUME_DIFF(m_positionReceiver, nullptr);
    OBJ_ASSUME_DIFF(m_orderReceiver, nullptr);

    // Stop streams - this closes them gracefully and disables auto-reconnect
    // The receivers remain alive but with null streams
    m_positionReceiver->stopStream(m_activeAccount.getAccountId());
    positionStreamStarted = false;
    DEBUG << "Positions stream stopped";

    m_orderReceiver->stopStream(m_activeAccount.getAccountId());
    orderStreamStarted = false;
    DEBUG << "Orders stream stopped";
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
    connected = connect(m_orderReceiver,
                        &OrdersReceiver::receivedNewOrder,
                        this,
                        &MainAlgo::receivedNewOrder,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);
    orderStreamStarted = true;
    DEBUG << "Replay orders receiver created for" << simAccountID;

    // Emit simulated account to update GUI account selector
    QVector<Account> simAccounts;
    simAccounts.append(simAccount);
    emit tradeStationAccountsReceived(simAccounts);
    DEBUG << "Emitted simulated account for replay mode";
}

void MainAlgo::resumeLiveStreams()
{
    INFO << "Resuming live streams after replay mode";

    // Receivers exist but their streams were stopped in pauseLiveStreams
    OBJ_ASSUME_DIFF(m_positionReceiver, nullptr);
    OBJ_ASSUME_DIFF(m_orderReceiver, nullptr);

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

ReplayEngine::PlaybackState MainAlgo::getReplayState() const
{
    if (m_replayEngine == nullptr)
    {
        return ReplayEngine::PlaybackState::Stopped;
    }
    return m_replayEngine->getState();
}

ReplayEngine* MainAlgo::getReplayEngine() const
{
    return m_replayEngine;
}

void MainAlgo::deleteAllStockInstruments()
{
    INFO << "Deleting all stock instruments for clean mode transition";

    // Clear the displayed pointer first
    currentDisplayedStockInstrument = nullptr;

    // Delete all stock instruments using deleteLater()
    // StockInstruments are QObject-derived with active connections (streams, receivers)
    for (auto it = stockInstruments.begin(); it != stockInstruments.end(); ++it)
    {
        if (QPointer<StockInstruments> instrument = it.value(); instrument)
        {
            DEBUG << "Scheduling deletion of stock instrument for" << instrument->symbol;
            instrument->deleteLater(); // Use deleteLater() for Qt objects with signals
        }
    }
    stockInstruments.clear();

    INFO << "All stock instruments scheduled for deletion";
}

void MainAlgo::stopAllStrategies()
{
    INFO << "Stopping all strategies for mode transition";
    m_strategyManager.stopAllStrategies();
    INFO << "All strategies stopped";
}

void MainAlgo::createAndSetDisplayedStockInstrument(const QString& p_symbol)
{
    INFO << "Creating and setting displayed stock instrument for" << p_symbol;

    // Create new stock instrument (will subscribe via DBClient if connected)
    auto* newInstrument = new StockInstruments(p_symbol, this);
    Q_CHECK_PTR(newInstrument);

    stockInstruments[p_symbol] = newInstrument;
    currentDisplayedStockInstrument = newInstrument;

    // Connect bar signals for the new displayed instrument
    bool connected = connect(&currentDisplayedStockInstrument->barReceiver,
                             &BarReceiver::receivedNewBar,
                             this,
                             &MainAlgo::displayedStockReceivedNewBar,
                             Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected = connect(&currentDisplayedStockInstrument->m_level2Receiver,
                        &Level2Receiver::receivedNewLevel2,
                        this,
                        &MainAlgo::displayedStockReceivedNewLevel2,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    // Forward trades for displayed symbol to FrontEnd
    connected = connect(DBClient::getInstance(),
                        &DBClient::newTrade,
                        this,
                        [this, p_symbol](const QString& sym, const Trade& trade)
                        {
                            if (sym == p_symbol)
                                emit displayedStockReceivedNewTrade(sym, trade);
                        });
    ASSUME_TRUE(connected);

    // Connect to strategy manager for bar delivery
    connected = connect(&currentDisplayedStockInstrument->barReceiver,
                        &BarReceiver::receivedNewBar,
                        &m_strategyManager,
                        &StrategyManager::onBarReceived,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected = connect(&currentDisplayedStockInstrument->m_level2Receiver,
                        &Level2Receiver::receivedNewLevel2,
                        &m_strategyManager,
                        &StrategyManager::onLevel2Received,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    INFO << "Stock instrument created and set as displayed for" << p_symbol;
}
