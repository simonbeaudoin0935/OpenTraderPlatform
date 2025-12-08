#include <QThread>
#include <QTimer>
#include <QTextStream>
#include <QDir>

#include "MainAlgo.h"
#include "TSClient.h"

Q_LOGGING_CATEGORY(MainAlgoLog, "MainAlgo")

// Initialize static member outside class
MainAlgo* MainAlgo::m_instance = nullptr;
    
MainAlgo& MainAlgo::getInstance()
{
    if (m_instance == nullptr) {
        qCDebug(TSClientLog) << "Singleton instance created";
        m_instance = new MainAlgo();
    }
    return *m_instance;
}

MainAlgo* MainAlgo::getInstancePtr()
{
    if (m_instance == nullptr) {
        qCDebug(TSClientLog) << "Singleton instance created";
        m_instance = new MainAlgo();
    }
    return m_instance;
}

MainAlgo::MainAlgo()
{
    thread.setObjectName("MainAlgoThread");

    this->moveToThread(&thread);

    connect(&thread, &QThread::started, this, &MainAlgo::onThreadStarted);

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

    m_savedGetBalancesRequestID = 0;
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

        disconnect(&currentDisplayedStockInstrument->barCache, &BarCache::receivedAsyncGetBars,
                   this, &MainAlgo::requestedMissingBarsDisplayedStockReceived);

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
    connect(&currentDisplayedStockInstrument->barCache, &BarCache::receivedNewBar,
            this, &MainAlgo::displayedStockReceivedNewBar);

    connect(&currentDisplayedStockInstrument->barCache, &BarCache::receivedAsyncGetBars,
            this, &MainAlgo::requestedMissingBarsDisplayedStockReceived);

    connect(&currentDisplayedStockInstrument->marketDepthQuoteReceiver, &MarketDepthQuoteReceiver::receivedNewMarketDepthQuote,
            this, &MainAlgo::displayedStockReceivedNewMarketDepthQuote);

}

void MainAlgo::onRequestMissingBarsDisplayedStock(QDateTime first, QDateTime last)
{
    qCDebug(MainAlgoLog) << "Requested bars from current displayed stock cache: " << first << " to " << last;

    Q_ASSERT(first.timeZone() == QTimeZone("America/New_York"));
    Q_ASSERT(last.timeZone() == QTimeZone("America/New_York"));
    Q_ASSERT(first < last);
    Q_ASSERT_X(currentDisplayedStockInstrument != nullptr, "Currently displayed stock instrument is null", "Bug if here");

    QVector<Bar> bars = currentDisplayedStockInstrument->barCache.getBars(first, last);

    if (bars.isEmpty()) {
        qCDebug(MainAlgoLog) << "Missing bars in cache for requested range. Sent API request. The barcache will emit signal when bars are ready.";
    } else {
        qCDebug(MainAlgoLog) << "All bars found in cache for requested range. Emitting signal.";
        
        emit requestedMissingBarsDisplayedStockReceived(bars);
    }

}



void MainAlgo::onTradeStationAuthStateChanged(bool isAuthenticated, QString reason)
{

    if (!isAuthenticated && !m_havePastSuccessfulExchanges) {
        qCritical(MainAlgoLog) << "Tradestation failed to authenticate. Reason : " << reason;
        qCritical(MainAlgoLog) << "Cannot proceed without authentication. Retrying";

        // TODO relaunch a auth attempt
        return;
    } else if (!isAuthenticated && m_havePastSuccessfulExchanges) {
        qCCritical(MainAlgoLog) << "Tradestation lost authentication. Reason : " << reason;
        return;
    }

    qCDebug(MainAlgoLog) << "Tradestation authenticated successfully : " << reason;

    // TODO handle if the request times out. if happened to me when the token was not expired and went ahead to get accounts but the connection
    // was bad and the request times out after 5s. Not checking the return value is a problem because we continue otherwise and hit assert when
    // referencing accounts[1] later on.

    connect(&TSClient::getInstance(), &TSClient::receivedAsyncGetAccounts,
            this, &MainAlgo::onReceivedAsyncGetAccounts,
            Qt::UniqueConnection);

    m_savedGetAccountsRequestID = TSClient::getInstance().getAccountsAsync();
}


void MainAlgo::onReceivedAsyncGetAccounts(TSClient::AsyncRequestID_t requestID, TSClient::AsyncRequestStatus_e status, QVector<Account> results)
{
    Q_ASSERT(requestID == m_savedGetAccountsRequestID);

    m_savedGetAccountsRequestID = 0;
    
    if (status == TSClient::AsyncRequestStatus_e::ERROR) {
        qCCritical(MainAlgoLog) << "get accounts error";
        Q_ASSERT(false);
        return;
    } else if (status == TSClient::AsyncRequestStatus_e::TIMEOUT) {
        qCWarning(MainAlgoLog) << "Received get accounts timeout";

        // Retry in one second
        QTimer::singleShot(1000, this, [this]() {
            m_savedGetAccountsRequestID = TSClient::getInstance().getAccountsAsync();
        });

        return;
    }

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
        return;
    }

    positionStreamStarted = true;

    connect(&positionReceiver, &PositionsReceiver::receivedNewPosition,
            this, &MainAlgo::receivedNewPosition,
            Qt::UniqueConnection);

    connect(&positionReceiver, &PositionsReceiver::receivedNewPosition,
            this, &MainAlgo::onReceivedNewPosition,
            Qt::UniqueConnection);

    positionReceiver.startStream(m_activeAccount.getAccountId());

    emit tradeStationAccountsReceived(results);



}






void MainAlgo::onReceivedNewPosition(QString account, Position position)
{
    Q_UNUSED(account);
    Q_UNUSED(position);

    // TODO
}

void MainAlgo::startBalancePolling()
{
    Q_ASSERT(QThread::currentThread() == &thread);

    connect(&TSClient::getInstance(), &TSClient::receivedAsyncGetBalances,
            this, &MainAlgo::onBalanceReceived,
            Qt::UniqueConnection);

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


    m_savedGetBalancesRequestID = TSClient::getInstance().getBalancesAsync(QStringList(m_activeAccount.getAccountId()));

    if (m_savedGetBalancesRequestID == 0) {
        qCWarning(MainAlgoLog) << "Failed to start getBalancesAsync request";
    }
}

void MainAlgo::onBalanceReceived(TSClient::AsyncRequestID_t requestID, TSClient::AsyncRequestStatus_e status, QVector<Balance> results)
{
    Q_ASSERT(QThread::currentThread() == &thread);
    Q_ASSERT(requestID == m_savedGetBalancesRequestID);
    Q_ASSERT(results.size() == 1);

    m_savedGetBalancesRequestID = 0;

    if (status == TSClient::AsyncRequestStatus_e::SUCCESS) {
        m_currentBalance = results.at(0);
        qCDebug(MainAlgoLog) << "Received balances for" << results.size() << "accounts";
        emit balanceUpdated(m_currentBalance);
    } else if (status == TSClient::AsyncRequestStatus_e::TIMEOUT) {
        qCWarning(MainAlgoLog) << "Get balances request timed out";
    } else {
        qCWarning(MainAlgoLog) << "Get balances request failed";
    }
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
