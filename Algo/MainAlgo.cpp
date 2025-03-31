#include <QThread>

#include <QTimer>

#include "MainAlgo.h"
#include "Misc/Settings.h"
#include "Clients/FMPClient/FMPClient.h"

Q_LOGGING_CATEGORY(MainAlgoLog, "MainAlgo")

static const int newsFetchingInterval = 30;

MainAlgo::MainAlgo() :
    thread(new QThread())
{
    thread->setObjectName("MainAlgoThread");

    this->moveToThread(thread);

    connect(thread, &QThread::started, this, &MainAlgo::onThreadStarted);

    connect(FMPClient::getInstancePtr(), &FMPClient::sharesFloatReceived, this, &MainAlgo::onSharesFloatReceived);
    connect(FMPClient::getInstancePtr(), &FMPClient::stockNewsReceived, this, &MainAlgo::onStockNewsReceived);
    loadCriterias();
}

void MainAlgo::start()
{
    thread->start();
}

void MainAlgo::loadCriterias()
{
    PriceRangeLow = criteriaSettings->value("Criterias/PriceRangeLow",0.0).toDouble();
    if (PriceRangeLow <= 0.0) {
        qFatal() << "Criterias/PriceRangeLow";
    }

    PriceRangeHigh = criteriaSettings->value("Criterias/PriceRangeHigh",0.0).toDouble();
    if (PriceRangeHigh <= 0.0) {
        qFatal() << "Criterias/PriceRangeHigh";
    }

    PreferedFloat = criteriaSettings->value("Criterias/PreferedFloat",0).toULongLong();
    if (PreferedFloat == 0) {
        qFatal() << "Criterias/PreferedFloat";
    }

    MaxFloat = criteriaSettings->value("Criterias/MaxFloat",0).toULongLong();
    if (MaxFloat == 0) {
        qFatal() << "Criterias/MaxFloat";
    }

    RelativeVolume = criteriaSettings->value("Criterias/RelativeVolume",0.0).toDouble();
    if (RelativeVolume <= 0.0) {
        qFatal() << "Criterias/RelativeVolume";
    }

    GapPercentage = criteriaSettings->value("Criterias/GapPercentage",0.0).toDouble();
    if (GapPercentage <= 0.0) {
        qFatal() << "Criterias/GapPercentage";
    }
}

void MainAlgo::fetchAsyncNewsStockScreenedByFloat()
{
    FMPClient& client = FMPClient::getInstance();

    StockNewsFilter filter;

    qCDebug(MainAlgoLog) << "Fetching news ASYNC for all screened stocks";
    qCDebug(MainAlgoLog) << "Waiting for all the news to be received";

    for (QString &symbol : symbolsScreenedByFloat) {

        filter.setSymbol(symbol);
        filter.setLimit(2);

        client.fetchAsyncStockNews(filter);
    }

}

void MainAlgo::fetchSyncNewsStockScreenedByFloat()
{
    FMPClient& client = FMPClient::getInstance();


    qCDebug(MainAlgoLog) << "Fetching news SYNC for all screened stocks";
    qCDebug(MainAlgoLog) << "Waiting for all the " << symbolsScreenedByFloat.size() << "news to be received";

    for (QString &symbol : symbolsScreenedByFloat) {

        StockNewsFilter filter;
        filter.setSymbol(symbol);
        filter.setLimit(2);

        qCDebug(MainAlgoLog) << "Fetching SYNC news for : " << symbol;

        QVector<StockNewsResult> newsResults;
        bool success = client.fetchSyncStockNews(filter, newsResults);

        if (!success) {
            qCDebug(MainAlgoLog) << "Error during the fetching of the news...";
        } else {
            qCDebug(MainAlgoLog) << "Fetched " << newsResults.size();
            latestNewsPerSymbol.insert(symbol, newsResults);
        }

    }

    qCDebug(MainAlgoLog) << "Done fetching news. Fetched : " << latestNewsPerSymbol.size() << " news";
}

void MainAlgo::processReceivedNews()
{
    qCDebug(MainAlgoLog) << "Processing the news for the " << symbolsScreenedByFloat.size() << " symbols" ;

    qCDebug(MainAlgoLog) << "Launching a timer to fetch the news again in " << newsFetchingInterval << " seconds" ;

    QTimer::singleShot(1000 * newsFetchingInterval, this, [this]() {
        fetchAsyncNewsStockScreenedByFloat();
    });
}

void MainAlgo::onThreadStarted()
{
    FMPClient& client = FMPClient::getInstance();

    CompanyScreenerFilter filter;

    QLoggingCategory::setFilterRules("RESTClient.debug=false"); // *********************************

    // Set criterias
    {
        filter.setIndustry("Biotechnology");
        filter.setPriceMoreThan(PriceRangeLow);
        filter.setPriceLowerThan(PriceRangeHigh);
        filter.setExchange("NASDAQ");
        filter.setIsActivelyTrading(true);
        filter.setIsEtf(false);
        filter.setIsFund(false);
        filter.setCountry("US");
        filter.setVolumeMoreThan(10000); // TODO shiznit
    }

    // Execute the screener SYNChronously
    {
        bool success = client.fetchSyncCompanyScreener(filter, screeningResults);
        qCDebug(MainAlgoLog) << "Fetched " << screeningResults.size() << " stocks with company screener";
        Q_ASSERT(success);
    }

    qCDebug(MainAlgoLog) << "Results of the screening :";

    for (CompanyScreenerResult &result : screeningResults) {
        qCDebug(MainAlgoLog) << result.toJsonString();
    }

    qCDebug(MainAlgoLog) << "Fetching the float for all of them. Waiting for all the float values to be received ";

    for (CompanyScreenerResult &result : screeningResults) {
        client.fetchAsyncSharesFloat(result.getSymbol());
    }
}

void MainAlgo::onSharesFloatReceived(FMPClient::SharesFloatResult result)
{
    static size_t numFloatReceived = 0;

    numFloatReceived++;

    if (result.floatShares <= (signed) PreferedFloat) {
        qCDebug(MainAlgoLog) << "Company : " << result.symbol << " has a float acceptable of : " << result.floatShares;

        symbolsScreenedByFloat.append(result.symbol);
    }

    if (numFloatReceived == (unsigned) screeningResults.size()) {
        qCDebug(MainAlgoLog) << "Received the float for all " << screeningResults.size() << " companies. Proceeding";
        qCDebug(MainAlgoLog) << "Added " << symbolsScreenedByFloat.size() << " symbols to symbolsScreenedByFloat. Proceeding";

        numFloatReceived = 0; // Reset for future use

        fetchAsyncNewsStockScreenedByFloat();
    }
}

void MainAlgo::onStockNewsReceived(QVector<StockNewsResult> results)
{
    static size_t numStockNewsReceived = 0;

    numStockNewsReceived++;

    qCDebug(MainAlgoLog) << "Received " << numStockNewsReceived << " stock news";

    if (results.isEmpty()) {
        qCDebug(MainAlgoLog) << "Received an empty result. I guess this is to be expected";
    } else {
        latestNewsPerSymbol.insert(results.first().getSymbol(), results);
    }

    if (numStockNewsReceived == (unsigned) symbolsScreenedByFloat.size()) {
        qCDebug(MainAlgoLog) << "Received the news for all " << symbolsScreenedByFloat.size() << " companies. Proceeding";
        numStockNewsReceived = 0; // Reset for future use

        processReceivedNews();
    }
}
