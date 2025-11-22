#include <QThread>
#include <QTimer>
#include <QTextStream>
#include <QDir>

#include "MainAlgo.h"
#include "TSClient.h"

Q_LOGGING_CATEGORY(MainAlgoLog, "MainAlgo")

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
}

void MainAlgo::start()
{
    thread.start();
}

void MainAlgo::onSelectDisplayedStock(QString symbol)
{
    // Make sure that this method gets Qt::InvokeMethod'ed if called from another thread
    Q_ASSERT(QThread::currentThread() == &thread);

    // If there is a current selected stock for display, disconnect its receivedNew* signals from the main algo emition
    if (currentDisplayedStockInstrument != nullptr) {
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
    connect(&currentDisplayedStockInstrument->barCache, &BarCache::receivedNewBar,
            this, &MainAlgo::displayedStockReceivedNewBar);

    connect(&currentDisplayedStockInstrument->marketDepthQuoteReceiver, &MarketDepthQuoteReceiver::receivedNewMarketDepthQuote,
            this, &MainAlgo::displayedStockReceivedNewMarketDepthQuote);

}

void MainAlgo::onRequestMissingBarsDisplayedStock(QDateTime first, QDateTime last)
{
    qCDebug(MainAlgoLog) << "Requested bars from current displayed stock cache: " << first << " to " << last;

    QVector<Bar> bars;

    if (currentDisplayedStockInstrument != nullptr) {
        bars = currentDisplayedStockInstrument->barCache.getBars(first.toTimeZone(QTimeZone("America/New_York")), last);
    } else {
        qCWarning(MainAlgoLog) << "No current displayed stock selected";
    }

    emit requestedMissingBarsDisplayedStockReceived(bars);
}

void MainAlgo::onThreadStarted()
{
    connect(&stockScreener, &StockScreener::finished, this, &MainAlgo::onStockScreenerFinished);
    //stockScreener.start();
}

void MainAlgo::onTradeStationAuthStateChanged(bool isAuthenticated, QString reason)
{
    if (isAuthenticated) {
        TSClient::getInstance().getAccountsSync(accounts);
    } else {
        qCCritical(MainAlgoLog) << "Tradestation lost authentication. Reason : " << reason;
        return; //
    }

    // Only initialize position stream once
    if (positionStreamStarted) {
        qCDebug(MainAlgoLog) << "Position stream already started, skipping initialization";
        return;
    }

    // FIXME warning hack, better this. This is just for sim
    QString accountNumber = accounts.at(1).getAccountId();

    connect(&positionReceiver, &PositionsReceiver::receivedNewPosition,
            this, &MainAlgo::receivedNewPosition,
            Qt::UniqueConnection);

    connect(&positionReceiver, &PositionsReceiver::receivedNewPosition,
            this, &MainAlgo::onReceivedNewPosition,
            Qt::UniqueConnection);

    positionReceiver.startStream(accountNumber);
    positionStreamStarted = true;
}


void MainAlgo::onStockScreenerFinished()
{
    // disconnect?

    for (const auto &screeningResult:  stockScreener.getStockScreeningResult()) {

        QString symbol = screeningResult.getSymbol();

        // Change the stock selected pointer to the new selected stock
        if (!stockInstruments.contains(symbol)) {

            StockInstruments *stock = new StockInstruments(symbol);


            stockInstruments.insert(symbol, stock);
        }
    }

    qDebug() << "Added " << stockScreener.getStockScreeningResult().size() << " biotech stocks to the stock instruments list";

    //connect(&breakingNewsFetcher, &BreakingNewsFetcher::foundNewNews, this, &MainAlgo::onNewNewsFound);

    //breakingNewsFetcher.start(stockScreener.getStockScreeningResult(),
    //                          newsFetchDepthLimit,
    //                          newsFetchingInterval);

    //stockRunUpDetector.start(stockScreener.getStockScreeningResult());

    //stockRunUpDetector.start(stockScreener.getStockScreeningResult());


}

void MainAlgo::onNewNewsFound(StockNewsResult newNews)
{
    qDebug(BreakingNewsFetcherLog) << "  ******************** STRIKE ****************";
    qDebug(BreakingNewsFetcherLog) << "  Date     : " << newNews.getPublishedDate();
    qDebug(BreakingNewsFetcherLog) << "  Title    : " << newNews.getTitle();
    qDebug(BreakingNewsFetcherLog) << "  Url      : " << newNews.getUrl();
    qDebug(BreakingNewsFetcherLog) << "  Found at : " << QDateTime::currentDateTimeUtc();
    qDebug(BreakingNewsFetcherLog) << "  ******************** STRIKE ****************";

    *algoLogFile << "  ******************** STRIKE ****************\n";
    *algoLogFile << "  Published date : " << newNews.getPublishedDate() << "\n";
    *algoLogFile << "  Title          : " << newNews.getTitle() << "\n";
    *algoLogFile << "  Url            : " << newNews.getUrl() << "\n";
    *algoLogFile << "  Found at       : " << QDateTime::currentDateTimeUtc().toString() << "\n";
    *algoLogFile << "  ******************** STRIKE ****************\n\n";
    algoLogFile->flush();
}

void MainAlgo::onReceivedNewPosition(QString account, Position position)
{
    Q_UNUSED(account);
    Q_UNUSED(position);

    // TODO
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
