#include <QThread>
#include <QTimer>
#include <QTextStream>
#include <QDir>

#include "MainAlgo.h"
#include "TSClient.h"
#include "Logging.h"

#define LOGGING_CATEGORY MainAlgoLog

Q_LOGGING_CATEGORY(MainAlgoLog, "MainAlgo")

// Initialize static member outside class
MainAlgo* MainAlgo::m_instance = nullptr;

MainAlgo* MainAlgo::getInstance()
{
    if (m_instance == nullptr) {
        qCDebug(MainAlgoLog) << "Singleton instance created";
        m_instance = new MainAlgo();
    }
    return m_instance;
}


MainAlgo::MainAlgo()
{
    thread.setObjectName("MainAlgoThread");

    this->moveToThread(&thread);

    auto c1 = connect(&thread, &QThread::started, this, &MainAlgo::onThreadStarted, Qt::UniqueConnection);
    Q_ASSERT(c1);

    {
        QString filePath = QDir::homePath() + "/Documents/results.txt";

        // Create QFile object
        file.setFileName(filePath);

        // Open the file in the desired mode
        QIODevice::OpenMode mode = QIODevice::Text | QIODevice::Append;

        if (!file.open(mode)) {
            qCDebug(MainAlgoLog) << "Failed to open file for writing:" << filePath
                                 << "Error:" << file.errorString();
            Q_ASSERT(0);
        }

        // Create a QTextStream attached to the file

        algoLogFile =  new QTextStream(&file);

        // Optional: Set encoding (UTF-8 is default in modern Qt)
        algoLogFile->setEncoding(QStringConverter::Utf8);
    }
}

MainAlgo::~MainAlgo()
{
    stopBalancePolling();
}

void MainAlgo::start()
{
    thread.start();
}

void MainAlgo::onThreadStarted()
{
    m_balancePollingTimer = new QTimer(this);

    connect(m_balancePollingTimer, &QTimer::timeout, this, &MainAlgo::requestBalance, Qt::UniqueConnection);
}

void MainAlgo::onSelectDisplayedStock(QString symbol)
{
    // Make sure that this method gets Qt::InvokeMethod'ed if called from another thread
    Q_ASSERT(QThread::currentThread() == &thread);

    
    // If there is a current selected stock for display, disconnect its receivedNew* signals from the main algo emition
    if (currentDisplayedStockInstrument != nullptr) {
        Q_ASSERT_X(currentDisplayedStockInstrument->symbol != symbol, "MainAlgo::onSelectDisplayedStock", "Selecting the same stock as currently selected. No action taken.");

        disconnect(&currentDisplayedStockInstrument->barCache, &BarCache::receivedNewBar,
                   this, &MainAlgo::displayedStockReceivedNewBar);

        disconnect(&currentDisplayedStockInstrument->marketDepthQuoteReceiver, &MarketDepthQuoteReceiver::receivedNewMarketDepthQuote,
                   this, &MainAlgo::displayedStockReceivedNewMarketDepthQuote);
    }

    // Change the stock selected pointer to the new selected stock
    if (stockInstruments.contains(symbol)) {
        currentDisplayedStockInstrument = stockInstruments[symbol];
    } else {
        currentDisplayedStockInstrument = new StockInstruments(symbol);
        Q_CHECK_PTR(currentDisplayedStockInstrument);

        stockInstruments.insert(symbol, currentDisplayedStockInstrument);
    }

    // Redoo the plumbing we disconnected at the top of this function
    auto c1 = connect(&currentDisplayedStockInstrument->barCache, &BarCache::receivedNewBar,
            this, &MainAlgo::displayedStockReceivedNewBar, Qt::UniqueConnection);
    Q_ASSERT(c1);

    auto c2 = connect(&currentDisplayedStockInstrument->marketDepthQuoteReceiver, &MarketDepthQuoteReceiver::receivedNewMarketDepthQuote,
            this, &MainAlgo::displayedStockReceivedNewMarketDepthQuote, Qt::UniqueConnection);
    Q_ASSERT(c2);
}

BarCache::GetBarsResult_t MainAlgo::requestMissingBarsDisplayedStock(QDateTime first, QDateTime last)
{
    qCDebug(MainAlgoLog) << "Requested bars from current displayed stock cache: " << first << " to " << last;

    Q_ASSERT(first.timeZone() == QTimeZone("America/New_York"));
    Q_ASSERT(last.timeZone() == QTimeZone("America/New_York"));
    Q_ASSERT(first < last);
    Q_ASSERT_X(currentDisplayedStockInstrument != nullptr, "Currently displayed stock instrument is null", "Bug if here");

    BarCache::GetBarsResult_t result = currentDisplayedStockInstrument->barCache.getBars(first, last);
    
    return result;
}


/*
 * This is the entry point that activates the chain of events after authentication state changes
 */
void MainAlgo::onTradeStationAuthStateChanged(bool isAuthenticated, QString reason)
{
    if (!isAuthenticated) {
        if (!m_havePastSuccessfulExchanges) {
            qCritical(MainAlgoLog) << "Tradestation failed to authenticate. Reason : " << reason;
            qCritical(MainAlgoLog) << "Cannot proceed without authentication. Retrying";
            // TODO relaunch a auth attempt
            Q_ASSERT(false);
        } else {
            qCCritical(MainAlgoLog) << "Tradestation lost authentication. Reason : " << reason;
        }
        return;
    }

    qCDebug(MainAlgoLog) << "Tradestation authenticated successfully : " << reason;

    // Now that the TSClient notified us that we are authenticated,
    // the first thing is to request the accounts.
    QFuture<QVector<Account>> future = TSClient::getInstance()->getAccounts();

    future.then(this, [this](const QVector<Account>& results){
        onReceivedAsyncGetAccounts(results);
    }).onFailed(this, [this] (const QException& e){
        CRITICAL << "getAccounts() failed" << QString::fromStdString(e.what());

        QTimer::singleShot(1000, this, [this]() {
            qCDebug(MainAlgoLog) << "Retrying getAccounts() after failure";
            onTradeStationAuthStateChanged(true, "Re-auth after getAccounts() failure");
        });
    });
}


void MainAlgo::onReceivedAsyncGetAccounts(const QVector<Account>& results)
{ 
    m_havePastSuccessfulExchanges = true;

    //TODO this is only for sim, in reality it will be number 0
    m_activeAccount = results.at(1);

    // Start balance polling if not already started
    if (!m_balancePollingStarted) {
        startBalancePolling();
        m_balancePollingStarted = true;
    }
 
    // Only initialize position stream once
    if (positionStreamStarted) {
        qCDebug(MainAlgoLog) << "Position stream already started, skipping initialization";
    } else {
        m_positionReceiver = new PositionsReceiver(m_activeAccount.getAccountId());
        Q_CHECK_PTR(m_positionReceiver);
        positionStreamStarted = true;

        auto c1 = connect(m_positionReceiver, &PositionsReceiver::receivedNewPosition,
                this, &MainAlgo::receivedNewPosition,
                Qt::UniqueConnection);
        Q_ASSERT(c1);

        auto c2 = connect(m_positionReceiver, &PositionsReceiver::receivedNewPosition,
                this, &MainAlgo::onReceivedNewPosition,
                Qt::UniqueConnection);
        Q_ASSERT(c2);

        auto c3 = connect(m_positionReceiver, &PositionsReceiver::positionDeleted,
                this, &MainAlgo::onPositionDeleted,
                Qt::UniqueConnection);
        Q_ASSERT(c3);
    }

    // Only initialize order stream once
    if (orderStreamStarted) {
        qCDebug(MainAlgoLog) << "Order stream already started, skipping initialization";
    } else {
        m_orderReceiver = new OrdersReceiver(m_activeAccount.getAccountId());
        Q_CHECK_PTR(m_orderReceiver);
        orderStreamStarted = true;

        auto c3 = connect(m_orderReceiver, &OrdersReceiver::receivedNewOrder,
                this, &MainAlgo::receivedNewOrder,
                Qt::UniqueConnection);
        Q_ASSERT(c3);

        auto c4 = connect(m_orderReceiver, &OrdersReceiver::receivedNewOrder,
                this, &MainAlgo::onReceivedNewOrder,
                Qt::UniqueConnection);
        Q_ASSERT(c4);
    }

    emit tradeStationAccountsReceived(results);
}

void MainAlgo::onReceivedNewPosition(QString account, Position position)
{
    Q_UNUSED(account);
    qCDebug(MainAlgoLog) << "Received new position:" << position.toJsonString();

    
}

void MainAlgo::onPositionDeleted(QString account, QString positionID)
{
    Q_UNUSED(account);
    qCDebug(MainAlgoLog) << "Position deleted:" << positionID;
    emit positionDeleted(account, positionID);
}

void MainAlgo::onReceivedNewOrder(QString account, Order order)
{
    Q_UNUSED(account);
    Q_UNUSED(order);

    // TODO
}

void MainAlgo::startBalancePolling()
{
    Q_ASSERT(QThread::currentThread() == &thread);

    m_balancePollingTimer->start(5000); // 5 seconds
    requestBalance(); // initial request
    qCDebug(MainAlgoLog) << "Started balance polling";
}

void MainAlgo::stopBalancePolling()
{
    Q_ASSERT(QThread::currentThread() == &thread);

    m_balancePollingTimer->stop();
    qCDebug(MainAlgoLog) << "Stopped balance polling";
}

[[nodiscard]] Balance MainAlgo::getCurrentBalance() const
{
    return m_currentBalance;
}

void MainAlgo::requestBalance()
{
    Q_ASSERT(QThread::currentThread() == &thread);

    Q_ASSERT(!m_activeAccount.getAccountId().isEmpty());


    QFuture<QVector<Balance>> balanceFuture = TSClient::getInstance()->getBalances(QStringList(m_activeAccount.getAccountId()));

    balanceFuture.then(this, [this](const QVector<Balance>& results){
        onBalanceReceived(results);
    }).onFailed(this, [this] (const TSClient::TimeoutException& e){
        Q_UNUSED(e);
        Q_ASSERT_X(false, "Get balances request timed out", "Get balances request timed out");
    }).onFailed(this, [this] (const TSClient::JSONErrorException& e){
        Q_UNUSED(e);
        Q_ASSERT_X(false, "Get balances request JSON error", "Get balances request JSON error");
    }).onFailed(this, [this] (const TSClient::OtherErrorException& e){
        Q_UNUSED(e);
        Q_ASSERT_X(false, "Get balances request other error", "Get balances request other error");
    });
}

void MainAlgo::onBalanceReceived(const QVector<Balance>& results)
{
    Q_ASSERT(QThread::currentThread() == &thread);
    Q_ASSERT(results.size() == 1);


    m_currentBalance = results.at(0);
    //qCDebug(MainAlgoLog) << "Received balances for" << results.size() << "accounts";

    // Emit signal for the UI or other components interested
    emit balanceUpdated(m_currentBalance);

    // TODO save this balance figure and act on it
}

StockInstruments::StockInstruments(const QString &symbol) :
    symbol(symbol),
    barCache(symbol, true, this),
    runUpDetector(&barCache, this),
    marketDepthQuoteReceiver(symbol, this)
{
    this->setObjectName("StockInstrument::" + symbol);

    qDebug() << this->objectName() << "New instance";
}

StockInstruments::~StockInstruments()
{
    qDebug() << this->objectName() << "Deleted instance";
}
