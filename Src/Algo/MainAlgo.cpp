#include <QThread>
#include <QTimer>

#include "MainAlgo.h"
#include "StrategyManager.h"
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


MainAlgo::MainAlgo()
{
    thread.setObjectName("MainAlgoThread");

    this->moveToThread(&thread);

    this->setObjectName("MainAlgo");

    connect(&thread, &QThread::started, this, &MainAlgo::onThreadStarted);

    // Create StrategyManager - owned by this MainAlgo
    m_strategyManager = std::make_unique<StrategyManager>(this);

    DEBUG << "Singleton instance created";
}

MainAlgo::~MainAlgo()
{
    DEBUG << "MainAlgo destructor - stopping thread";

    // StrategyManager will be destroyed automatically via unique_ptr
    m_strategyManager.reset();

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
    Q_UNUSED(order);

    // TODO
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
                                 const PlaceOrderRequest& p_orderRequest)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    // TODO: Add validation logic here (risk limits, portfolio constraints, etc.)

    // Store temporary mapping: requestId -> strategyID (will be replaced with OrderID -> strategyID when ACK received)
    m_requestIdToStrategyId[p_requestId] = p_strategyID;

    // Call TSClient to place the order
    QFuture<std::expected<PlaceOrderResult, TSClient::Error>> future =
        TSClient::getInstance()->placeOrder(p_orderRequest);

    // Store the future for tracking
    m_pendingOrderFutures[p_requestId] = future;

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

    // Remove from pending futures
    m_pendingOrderFutures.remove(p_requestId);

    // Look up which strategy placed this order
    auto strategyIt = m_requestIdToStrategyId.find(p_requestId);
    if (strategyIt == m_requestIdToStrategyId.end())
    {
        WARNING << "onOrderResolved: requestId not found:" << p_requestId;
        return;
    }

    QString strategyID = *strategyIt;
    m_requestIdToStrategyId.remove(p_requestId);

    if (p_result.has_value())
    {
        // Order was successfully placed
        PlaceOrderResult result = p_result.value();

        DEBUG << "Order placed successfully: strategyID=" << strategyID << "successful=" << result.isAllSuccessful();

        // TODO: Extract OrderID from result.getOrders() and create permanent mapping
        // For now, just create placeholder mapping
        // const auto& orders = result.getOrders();
        // for (const auto& order : orders)
        // {
        //     m_orderMappings[order.orderId] = strategyID;
        // }

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
