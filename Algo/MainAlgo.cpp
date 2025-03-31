#include <QThread>

#include <QTimer>
#include <QTextStream>
#include <QDir>

#include "MainAlgo.h"
#include "Misc/Settings.h"
#include "Clients/FMPClient/FMPClient.h"

Q_LOGGING_CATEGORY(MainAlgoLog, "MainAlgo")

//TODO should be parameters
static const int newsFetchingInterval = 20;
static const int newsFetchDepthLimit = 5;

MainAlgo::MainAlgo() :
    thread(new QThread())
{
    thread->setObjectName("MainAlgoThread");

    this->moveToThread(thread);

    connect(thread, &QThread::started, this, &MainAlgo::onThreadStarted);

    connect(FMPClient::getInstancePtr(), &FMPClient::sharesFloatReceived, this, &MainAlgo::onSharesFloatReceived);
    connect(FMPClient::getInstancePtr(), &FMPClient::stockNewsReceived, this, &MainAlgo::onStockNewsReceived);
    loadCriterias();

    // IMPORTANT init the pointer with the first pingpong buffer
    latestNewsPerSymbolPingPongPtr = &this->latestNewsPerSymbolPingPong1;


    {
        QString filePath = QDir::homePath() + "/Documents/results.txt";

        // Create QFile object
        file = new QFile(filePath);

        // Open the file in the desired mode
        QIODevice::OpenMode mode = QIODevice::Text | QIODevice::Append;

        if (!file->open(mode)) {
            qCDebug(MainAlgoLog) << "Failed to open file for writing:" << filePath
                                 << "Error:" << file->errorString();
            Q_ASSERT(0);
        }

        // Create a QTextStream attached to the file

        out =  new QTextStream(file);

        // Optional: Set encoding (UTF-8 is default in modern Qt)
        out->setEncoding(QStringConverter::Utf8);
    }
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
        filter.setLimit(newsFetchDepthLimit);

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
            latestNewsPerSymbolPingPongPtr->insert(symbol, newsResults);
        }
    }

    alternateNewsPerSymbolPingPong();
}

void MainAlgo::processReceivedNews()
{
    qCDebug(MainAlgoLog) << "Processing the news for the " << symbolsScreenedByFloat.size() << " symbols" ;

    // TODELETE : introduce a new news to test
    if (false) {
        static size_t a = 0;
        a++;
        if (a == 3) {
            a = 0;
            latestNewsPerSymbolPingPongPtr->first().first().TESTsetPublishedDateToNow();
        }
    }

    // Processing the news
    {
        processReceivedNewsPingPongBuffers();
    }

    // Processing done. Prepare the next iteration of the algorithm
    {
        alternateNewsPerSymbolPingPong();

        qCDebug(MainAlgoLog) << "Launching a timer to fetch the news again in " << newsFetchingInterval << " seconds" ;

        QTimer::singleShot(1000 * newsFetchingInterval, this, [this]() {
            fetchAsyncNewsStockScreenedByFloat();
        });
    }
}

void MainAlgo::alternateNewsPerSymbolPingPong()
{
    bool isPingPong1 = ((latestNewsPerSymbolPingPongPtr == &latestNewsPerSymbolPingPong1) ? true : false);

    qCDebug(MainAlgoLog) << "Done fetching news. Fetched : " << latestNewsPerSymbolPingPongPtr->size() << " news in pingpong " <<
        (isPingPong1 ? "1" : "2");

    qCDebug(MainAlgoLog) << "Switching pingpong ptr to " << (isPingPong1 ? "2" : "1");

    latestNewsPerSymbolPingPongPtr = (isPingPong1 ? &latestNewsPerSymbolPingPong2 : &latestNewsPerSymbolPingPong1);

    qCDebug(MainAlgoLog) << "Clearing pingpong " << (isPingPong1 ? "2" : "1");

    latestNewsPerSymbolPingPongPtr->clear();
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

        // IMPORTANT launch the fetching of the news, which will be the initial kick
        // to the loop of fetching
        fetchAsyncNewsStockScreenedByFloat();
    }
}

void MainAlgo::onStockNewsReceived(QVector<StockNewsResult> results)
{
    static size_t numStockNewsReceived = 0;

    numStockNewsReceived++;

    //qCDebug(MainAlgoLog) << "Received " << numStockNewsReceived << " stock news";

    if (results.isEmpty()) {
        //qCDebug(MainAlgoLog) << "Received an empty result. I guess this is to be expected";
    } else {
        latestNewsPerSymbolPingPongPtr->insert(results.first().getSymbol(), results);
    }

    if (numStockNewsReceived == (unsigned) symbolsScreenedByFloat.size()) {
        qCDebug(MainAlgoLog) << "Received the news for all " << symbolsScreenedByFloat.size() << " companies. Proceeding";
        numStockNewsReceived = 0; // Reset for future use

        processReceivedNews();
    }
}

// Function to find the newest StockNewsResult
const StockNewsResult& getNewestNews(const QVector<StockNewsResult>& newsVector) {
    // Assert that the vector is not empty
    Q_ASSERT(!newsVector.isEmpty() && "News vector must not be empty");
    if (newsVector.isEmpty()) {
        qCDebug(MainAlgoLog) << "Error: Attempted to retrieve newest news from an empty vector";
        throw std::out_of_range("Cannot retrieve newest news from an empty vector");
    }

    // Initialize with the first entry
    const StockNewsResult* newest = &newsVector[0];
    QDateTime newestDate = QDateTime::fromString(newest->getPublishedDate(), Qt::ISODate);

    // Log if the initial date is invalid
    if (!newestDate.isValid()) {
        qCDebug(MainAlgoLog) << "Invalid published date for initial news:" << newest->getPublishedDate();
    }

    // Iterate through the vector to find the newest entry
    for (int i = 1; i < newsVector.size(); ++i) {
        QDateTime currentDate = QDateTime::fromString(newsVector[i].getPublishedDate(), Qt::ISODate);
        if (!currentDate.isValid()) {
            qCDebug(MainAlgoLog) << "Invalid published date for news at index" << i << ":"
                                 << newsVector[i].getPublishedDate();
            continue; // Skip invalid dates
        }

        if (currentDate > newestDate) {
            newest = &newsVector[i];
            newestDate = currentDate;
        }
    }

    // Assert that at least one valid date was found
    Q_ASSERT(newestDate.isValid() && "At least one valid published date must exist in the news vector");
    if (!newestDate.isValid()) {
        qCDebug(MainAlgoLog) << "No valid published dates found in news vector";
        throw std::runtime_error("All published dates in the vector are invalid");
    }

    return *newest;
}

void MainAlgo::processReceivedNewsPingPongBuffers()
{
    QMap<QString, QVector<StockNewsResult>> *newBuffer;
    QMap<QString, QVector<StockNewsResult>> *oldBuffer;

    newBuffer = latestNewsPerSymbolPingPongPtr;
    oldBuffer = (latestNewsPerSymbolPingPongPtr == &latestNewsPerSymbolPingPong1) ? &latestNewsPerSymbolPingPong2 : &latestNewsPerSymbolPingPong1;

    qCDebug(MainAlgoLog).noquote() << Q_FUNC_INFO << "\n\n                                ****** EXECUTING THE NEWS ALGORITHM ********\n";


    if (oldBuffer->isEmpty()) {
        qCDebug(MainAlgoLog) << "First iteration of the pingpong buffer processing. SKIP";
        return;
    }

    // Iterate through every symbol in the pingpong buffer
    QMap<QString, QVector<StockNewsResult>>::const_iterator it;
    for (it = newBuffer->constBegin(); it != newBuffer->constEnd(); ++it) {
        QString symbol = it.key();
        const QVector<StockNewsResult>& newNewsVector = it.value();

        // Find the newest new in the old news buffer
        const StockNewsResult &newestOldNews = getNewestNews((*oldBuffer)[symbol]);
        QDateTime dateNewestOldNews = QDateTime::fromString(newestOldNews.getPublishedDate());

        //qDebug(MainAlgoLog) << " Parsing news for Symbol:" << symbol;
        //qDebug(MainAlgoLog) << "  Date of newest old news :" << newestOldNews.getPublishedDate();

        for (const StockNewsResult& newNews : newNewsVector) {
            //qDebug(MainAlgoLog) << "  Date:" << newNews.getPublishedDate() << "\n  Title:" << newNews.getTitle();

            QDateTime dateNewNews = QDateTime::fromString(newNews.getPublishedDate());

            if (dateNewNews > dateNewestOldNews) {
                qDebug(MainAlgoLog) << "  ******************** STRIKE ****************";
                qDebug(MainAlgoLog) << "  Date     : " << newNews.getPublishedDate();
                qDebug(MainAlgoLog) << "  Title    : " << newNews.getTitle();
                qDebug(MainAlgoLog) << "  Url      : " << newNews.getUrl();
                qDebug(MainAlgoLog) << "  Found at : " << QDateTime::currentDateTimeUtc();
                qDebug(MainAlgoLog) << "  ******************** STRIKE ****************";

                *out << "  ******************** STRIKE ****************\n";
                *out << "  Published date : " << newNews.getPublishedDate() << "\n";
                *out << "  Title          : " << newNews.getTitle() << "\n";
                *out << "  Url            : " << newNews.getUrl() << "\n";
                *out << "  Found at       : " << QDateTime::currentDateTimeUtc().toString() << "\n";
                *out << "  ******************** STRIKE ****************\n\n";
                out->flush();
            }
        }
    }
}









