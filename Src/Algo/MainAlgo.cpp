#include <QThread>
#include <QTimer>
#include <QSocketNotifier>
#include <unistd.h>

#include "MainAlgo.h"
#include "StrategyManager.h"
#include "StrategySignalHandler.h"
#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"

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

    // Cleanup signal handler system
    m_crashNotifier.reset();
    StrategySignalHandler::cleanup();

    // StrategyManager will be destroyed automatically via composition

    // Stop balance polling timer if it exists
    // Note: We're in the destructor, so we can't use QMetaObject::invokeMethod
    // since the thread might already be stopping. Just stop the timer directly.
    if (m_balancePollingTimer && m_balancePollingTimer->isActive())
    {
        m_balancePollingTimer->stop();
        DEBUG << "Stopped balance polling timer in destructor";
    }

    // Request thread to stop
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
            &MainAlgo::displayedStockReceivedNewMarketDepthQuote,
            &m_strategyManager,
            &StrategyManager::onMarketDepthReceived,
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

void MainAlgo::onSelectDisplayedStock(const QString& symbol)
{
    // Make sure that this method gets Qt::InvokeMethod'ed if called from another thread
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);


    // If there is a current selected stock for display, disconnect its receivedNew* signals from the main algo emition
    if (currentDisplayedStockInstrument != nullptr)
    {
        // Selecting the same stock as currently selected. No action taken.
        OBJ_ASSUME_DIFF(currentDisplayedStockInstrument->symbol, symbol);

        disconnect(&currentDisplayedStockInstrument->barCache,
                   &BarCache::receivedNewBar,
                   this,
                   &MainAlgo::displayedStockReceivedNewBar);

        disconnect(&currentDisplayedStockInstrument->marketDepthQuoteReceiver,
                   &MarketDepthQuoteReceiver::receivedNewMarketDepthQuote,
                   this,
                   &MainAlgo::displayedStockReceivedNewMarketDepthQuote);

        // Clean up the previous stock instrument to free resources (streams, database connections)
        QString oldSymbol = currentDisplayedStockInstrument->symbol;
        QPointer<StockInstruments> oldInstrument = currentDisplayedStockInstrument;

        // Close streams BEFORE scheduling deletion to avoid race conditions with .then() callbacks
        if (oldInstrument->barCache.getStream())
        {
            TSClient::getInstance()->closeStream(oldInstrument->barCache.getStream());
        }
        if (oldInstrument->marketDepthQuoteReceiver.getStream())
        {
            TSClient::getInstance()->closeStream(oldInstrument->marketDepthQuoteReceiver.getStream());
        }

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
    connect(&currentDisplayedStockInstrument->barCache,
            &BarCache::receivedNewBar,
            this,
            &MainAlgo::displayedStockReceivedNewBar);

    connect(&currentDisplayedStockInstrument->marketDepthQuoteReceiver,
            &MarketDepthQuoteReceiver::receivedNewMarketDepthQuote,
            this,
            &MainAlgo::displayedStockReceivedNewMarketDepthQuote);
}

BarCache::GetBarsResult_t MainAlgo::requestMissingBarsDisplayedStock(QDate date, QTime first, QTime last)
{
    DEBUG << "Requested bars from current displayed stock cache: " << first << " to " << last;

    OBJ_ASSUME_LT(first, last);
    OBJ_ASSUME_DIFF(currentDisplayedStockInstrument, nullptr);

    return currentDisplayedStockInstrument->barCache.getBars(date, first, last);
}

/*
 * This is the entry point that activates the chain of events after authentication state changes
 */
void MainAlgo::onTradeStationAuthStateChanged(bool isAuthenticated, const QString& reason)
{
    if (!isAuthenticated)
    {
        if (!m_havePastSuccessfulExchanges)
        {
            CRITICAL << "Tradestation failed to authenticate. Reason : " << reason;
            CRITICAL << "Cannot proceed without authentication. Retrying";
        }
        else
        {
            CRITICAL << "Tradestation lost authentication. Reason : " << reason;
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
                                                                              "Re-auth after getAccounts() failure");
                                           });
                    }
                });
}


void MainAlgo::onReceivedAsyncGetAccounts(const QVector<Account>& results)
{
    m_havePastSuccessfulExchanges = true;

    //TODO this is only for sim, in reality it will be number 0
    m_activeAccount = results.at(1);

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
    Q_UNUSED(account);
    DEBUG << "Received new position:" << position.toJsonString();
}

void MainAlgo::onPositionDeleted(const QString& account, const QString& positionID)
{
    Q_UNUSED(account);
    DEBUG << "Position deleted:" << positionID;
    emit positionDeleted(account, positionID);
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

    m_balancePollingTimer->start(5000); // 5 seconds
    requestBalance();                   // initial request
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
    //qCDebug(MainAlgoLog) << "Received balances for" << results.size() << "accounts";

    // Emit signal for the UI or other components interested
    emit balanceUpdated(m_currentBalance);

    // TODO save this balance figure and act on it
}

StockInstruments::StockInstruments(const QString& p_symbol, QObject* p_parent)
    : QObject(p_parent), symbol(p_symbol), barCache(p_symbol, true, this), marketDepthQuoteReceiver(p_symbol, this)
{
    this->setObjectName("StockInstrument::" + p_symbol);

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

// ============================================================================
// Replay Mode Methods
// ============================================================================

void MainAlgo::enterReplayMode(QDate p_date, QTime p_startTime, ReplayEngine::PlaybackSpeed p_speed)
{
    INFO << "MainAlgo entering replay mode for" << p_date.toString(Qt::ISODate) << "at"
         << p_startTime.toString("hh:mm:ss");

    // Create ReplayEngine on first use (lazy init, parent=this for thread affinity)
    if (m_replayEngine == nullptr)
    {
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

        DEBUG << "ReplayEngine created and connected";
    }

    m_replayEngine->startReplay(p_date, p_startTime, p_speed);
}

void MainAlgo::exitReplayMode()
{
    INFO << "MainAlgo exiting replay mode";

    if (m_replayEngine != nullptr)
    {
        m_replayEngine->stopReplay();
    }
}

void MainAlgo::pauseReplay()
{
    if (m_replayEngine != nullptr)
    {
        m_replayEngine->pauseReplay();
    }
}

void MainAlgo::resumeReplay()
{
    if (m_replayEngine != nullptr)
    {
        m_replayEngine->resumeReplay();
    }
}

void MainAlgo::pauseLiveStreams()
{
    INFO << "Pausing live streams for replay mode";

    // Stop positions stream
    if (m_positionReceiver != nullptr)
    {
        m_positionReceiver->stopStream(m_activeAccount.getAccountId());
        positionStreamStarted = false;
        DEBUG << "Positions stream stopped";
    }

    // Stop orders stream
    if (m_orderReceiver != nullptr)
    {
        m_orderReceiver->stopStream(m_activeAccount.getAccountId());
        orderStreamStarted = false;
        DEBUG << "Orders stream stopped";
    }
}

void MainAlgo::resumeLiveStreams()
{
    INFO << "Resuming live streams after replay mode";

    // Recreate positions receiver to trigger fresh snapshot
    // The constructor creates the stream automatically
    if (m_positionReceiver != nullptr)
    {
        delete m_positionReceiver;
        m_positionReceiver = nullptr;
    }
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
    positionStreamStarted = true;
    DEBUG << "Positions stream recreated and started";

    // Recreate orders receiver to trigger fresh snapshot
    // The constructor creates the stream automatically
    if (m_orderReceiver != nullptr)
    {
        delete m_orderReceiver;
        m_orderReceiver = nullptr;
    }
    m_orderReceiver = new OrdersReceiver(m_activeAccount.getAccountId(), this);
    connected = connect(m_orderReceiver,
                        &OrdersReceiver::receivedNewOrder,
                        this,
                        &MainAlgo::onReceivedNewOrder,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);
    orderStreamStarted = true;
    DEBUG << "Orders stream recreated and started";
}

ReplayEngine::PlaybackState MainAlgo::getReplayState() const
{
    if (m_replayEngine == nullptr)
    {
        return ReplayEngine::PlaybackState::Stopped;
    }
    return m_replayEngine->getState();
}
