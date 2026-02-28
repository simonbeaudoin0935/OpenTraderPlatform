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

        // Clean up the previous stock instrument to free resources (streams, database connections)
        QString oldSymbol = currentDisplayedStockInstrument->symbol;
        QPointer<StockInstruments> oldInstrument = currentDisplayedStockInstrument;

        // TODO Phase 6: close DBClient stream for old instrument

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
}

BarCache::GetBarsResult_t MainAlgo::requestMissingBarsDisplayedStock(QDate date, QTime first, QTime last)
{
    DEBUG << "Requested bars from current displayed stock cache: " << first << " to " << last;

    OBJ_ASSUME_LTE(first, last); // The Equal in less than equal is for when the program is launched at 4:02 AM
    OBJ_ASSUME_DIFF(currentDisplayedStockInstrument, nullptr);

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
    DEBUG << "Received new position:" << position.toJsonString();
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

    // Lookup which strategy placed this order
    auto strategyIt = m_orderMappings.find(order.getOrderID());

    if (strategyIt == m_orderMappings.end())
    {
        // This order does not belong to any strategy we know about
        WARNING << "Received order update for order ID:" << order.getOrderID() << "which has no associated strategy";
        return;
    }

    // This order belongs to a strategy - route it to that strategy
    QString strategyID = *strategyIt;
    QMetaObject::invokeMethod(
        &m_strategyManager,
        [this, order]() { m_strategyManager.onOrderUpdated(order); },
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
{
    this->setObjectName("StockInstrument::" + p_symbol);

    // Connect BarReceiver to BarCache for storage
    bool connected = connect(&barReceiver,
                             &BarReceiver::receivedNewBar,
                             &barCache,
                             [this](const QString&, const Bar& bar) { barCache.storeBar(bar); });
    OBJ_ASSUME_TRUE(connected);

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
                DEBUG << "Created order mapping: OrderID=" << orderID << "→ strategyID=" << strategyID;
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

void MainAlgo::enterReplayMode(QDate p_date, QTime p_startTime, ReplayEngine::PlaybackSpeed p_speed)
{
    INFO << "MainAlgo entering replay mode for" << p_date.toString(Qt::ISODate) << "at"
         << p_startTime.toString("hh:mm:ss");

    // We assume that if we were able to click "Enter Replay Mode", then we must not already be in replay mode, so m_replayEngine should be null
    OBJ_ASSUME_TRUE(m_replayEngine == nullptr);

    // Create ReplayEngine on first use (lazy init, parent=this for thread affinity)
    m_replayEngine = new ReplayEngine(this, TSClient::getInstance());

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

    // TODO Phase 6: connect ReplayEngine bar data directly to BarReceiver
    // TODO Phase 6: connect ReplayEngine depth data directly to Level2Receiver
    // TODO Phase 6: connect ReplayEngine quote data directly to Level1Receiver

    DEBUG << "ReplayEngine created and connected";

    // Start replay order/position streams with simulated account
    startReplayOrderStreams();

    // TODO Phase 6: pre-roll quote state via DBClient

    m_replayEngine->startReplay(p_date, p_startTime, p_speed);
}

void MainAlgo::enterReplayModePaused(QDate p_date, QTime p_startTime, ReplayEngine::PlaybackSpeed p_speed)
{
    INFO << "MainAlgo entering replay mode (paused) for" << p_date.toString(Qt::ISODate) << "at"
         << p_startTime.toString("hh:mm:ss");

    bool isRecreatingEngine = (m_replayEngine != nullptr);

    // If ReplayEngine already exists (e.g., changing replay day), delete it first
    if (m_replayEngine != nullptr)
    {
        DEBUG << "Deleting existing ReplayEngine before creating new one";
        delete m_replayEngine;
        m_replayEngine = nullptr;
    }

    // Create ReplayEngine (same setup as enterReplayMode)
    m_replayEngine = new ReplayEngine(this, TSClient::getInstance());

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

    // TODO Phase 6: connect ReplayEngine bar data directly to BarReceiver
    // TODO Phase 6: connect ReplayEngine depth data directly to Level2Receiver
    // TODO Phase 6: connect ReplayEngine quote data directly to Level1Receiver

    DEBUG << "ReplayEngine created and connected";

    // Only setup order/position streams on first entry to replay mode
    if (!isRecreatingEngine)
    {
        startReplayOrderStreams();
    }

    // TODO Phase 6: pre-roll quote state via DBClient

    // Start in paused state - emit first bar then pause
    m_replayEngine->startReplayPaused(p_date, p_startTime, p_speed);

    // Pause heartbeat timers since we're starting in paused state
    for (auto& instrument: stockInstruments)
    {
        OBJ_ASSUME_FALSE(instrument.isNull());
        instrument->barReceiver.pauseHeartbeat();
        instrument->m_level2Receiver.pauseHeartbeat();
    }
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

    // Create new stock instrument (will open streams with current TSClient mode)
    auto* newInstrument = new StockInstruments(p_symbol, this);
    Q_CHECK_PTR(newInstrument);

    stockInstruments[p_symbol] = newInstrument;
    currentDisplayedStockInstrument = newInstrument;

    // Connect signals for the new displayed instrument
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
