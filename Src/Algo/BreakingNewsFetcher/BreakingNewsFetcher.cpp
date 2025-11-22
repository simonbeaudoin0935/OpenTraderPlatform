#include "FMPClient.h"
#include "BreakingNewsFetcher.h"

Q_LOGGING_CATEGORY(BreakingNewsFetcherLog, "BreakingNewsFetcher")

BreakingNewsFetcher::BreakingNewsFetcher(QObject *parent)
    : QObject{parent}
{}

void BreakingNewsFetcher::start(QVector<QString> &symbolsScreenedByFloat,
                                int newsFetchDepthLimit,
                                int newsFetchingInterval)
{
    stopped = false;

    symbolsToWatch = symbolsScreenedByFloat;
    fetchingInterval = newsFetchingInterval;
    fetchDepthLimit = newsFetchDepthLimit;

    // IMPORTANT init the pointer with the first pingpong buffer
    latestNewsPerSymbolPingPongPtr = &this->latestNewsPerSymbolPingPong1;

    bool connection = connect(FMPClient::getInstancePtr(), &FMPClient::stockNewsReceived, this, &BreakingNewsFetcher::onStockNewsReceived, Qt::UniqueConnection);
    Q_ASSERT_X(connection, "BreakingNewsFetcher::start", "Failed to create unique connection for stockNewsReceived");

    fetchAsyncNewsStockScreenedByFloat();
}

void BreakingNewsFetcher::stop()
{
    stopped = true;
}


void BreakingNewsFetcher::fetchAsyncNewsStockScreenedByFloat()
{
    FMPClient& client = FMPClient::getInstance();

    StockNewsFilter filter;

    qCDebug(BreakingNewsFetcherLog) << "Fetching news ASYNC for all screened stocks";
    qCDebug(BreakingNewsFetcherLog) << "Waiting for all the news to be received";

    for (QString &symbol : symbolsToWatch) {

        filter.setSymbol(symbol);
        filter.setLimit(fetchDepthLimit);

        client.fetchAsyncStockNews(filter);
    }
}

void BreakingNewsFetcher::fetchSyncNewsStockScreenedByFloat()
{
    FMPClient& client = FMPClient::getInstance();


    qCDebug(BreakingNewsFetcherLog) << "Fetching news SYNC for all screened stocks";
    qCDebug(BreakingNewsFetcherLog) << "Waiting for all the " << symbolsToWatch.size() << "news to be received";

    for (QString &symbol : symbolsToWatch   ) {

        StockNewsFilter filter;
        filter.setSymbol(symbol);
        filter.setLimit(2);

        qCDebug(BreakingNewsFetcherLog) << "Fetching SYNC news for : " << symbol;

        QVector<StockNewsResult> newsResults;
        bool success = client.fetchSyncStockNews(filter, newsResults);

        if (!success) {
            qCDebug(BreakingNewsFetcherLog) << "Error during the fetching of the news...";
        } else {
            qCDebug(BreakingNewsFetcherLog) << "Fetched " << newsResults.size();
            latestNewsPerSymbolPingPongPtr->insert(symbol, newsResults);
        }
    }

    alternateNewsPerSymbolPingPong();
}

void BreakingNewsFetcher::processReceivedNews()
{
    qCDebug(BreakingNewsFetcherLog) << "Processing the news for the " << symbolsToWatch.size() << " symbols" ;

    // TODELETE : introduce a new news to test
    if (false) {
        static size_t a = 0;
        a++;
        if (a == 3) {
            a = 0;
            Q_ASSERT_X(!latestNewsPerSymbolPingPongPtr->isEmpty(), "BreakingNewsFetcher::processReceivedNews", "latestNewsPerSymbolPingPongPtr should not be empty");
            Q_ASSERT_X(!latestNewsPerSymbolPingPongPtr->first().isEmpty(), "BreakingNewsFetcher::processReceivedNews", "first element should not be empty");
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

        qCDebug(BreakingNewsFetcherLog) << "Launching a timer to fetch the news again in " << fetchingInterval << " seconds" ;

        // WARNING ameliorate this. what if the timer is alread armed and we want to stop if from happening in the future
        if (!stopped) {
            QTimer::singleShot(1000 * fetchingInterval, this, [this]() {
                fetchAsyncNewsStockScreenedByFloat();
            });
        }
    }
}

void BreakingNewsFetcher::alternateNewsPerSymbolPingPong()
{
    bool isPingPong1 = ((latestNewsPerSymbolPingPongPtr == &latestNewsPerSymbolPingPong1) ? true : false);

    qCDebug(BreakingNewsFetcherLog) << "Done fetching news. Fetched : " << latestNewsPerSymbolPingPongPtr->size() << " news in pingpong " <<
        (isPingPong1 ? "1" : "2");

    qCDebug(BreakingNewsFetcherLog) << "Switching pingpong ptr to " << (isPingPong1 ? "2" : "1");

    latestNewsPerSymbolPingPongPtr = (isPingPong1 ? &latestNewsPerSymbolPingPong2 : &latestNewsPerSymbolPingPong1);

    qCDebug(BreakingNewsFetcherLog) << "Clearing pingpong " << (isPingPong1 ? "2" : "1");

    latestNewsPerSymbolPingPongPtr->clear();
}




void BreakingNewsFetcher::onStockNewsReceived(QVector<StockNewsResult> results)
{
    static size_t numStockNewsReceived = 0;

    numStockNewsReceived++;

    //qCDebug(MainAlgoLog) << "Received " << numStockNewsReceived << " stock news";

    if (results.isEmpty()) {
        //qCDebug(MainAlgoLog) << "Received an empty result. I guess this is to be expected";
    } else {
        latestNewsPerSymbolPingPongPtr->insert(results.first().getSymbol(), results);
    }

    if (numStockNewsReceived == (unsigned) symbolsToWatch.size()) {
        qCDebug(BreakingNewsFetcherLog) << "Received the news for all " << symbolsToWatch.size() << " companies. Proceeding";
        numStockNewsReceived = 0; // Reset for future use

        processReceivedNews();
    }
}

// Function to find the newest StockNewsResult
static const StockNewsResult& getNewestNews(const QVector<StockNewsResult>& newsVector) {
    // Assert that the vector is not empty
    Q_ASSERT(!newsVector.isEmpty() && "News vector must not be empty");
    if (newsVector.isEmpty()) {
        qCDebug(BreakingNewsFetcherLog) << "Error: Attempted to retrieve newest news from an empty vector";
        throw std::out_of_range("Cannot retrieve newest news from an empty vector");
    }

    // Initialize with the first entry
    const StockNewsResult* newest = &newsVector[0];
    QDateTime newestDate = QDateTime::fromString(newest->getPublishedDate(), Qt::ISODate);

    // Log if the initial date is invalid
    if (!newestDate.isValid()) {
        qCDebug(BreakingNewsFetcherLog) << "Invalid published date for initial news:" << newest->getPublishedDate();
    }

    // Iterate through the vector to find the newest entry
    for (int i = 1; i < newsVector.size(); ++i) {
        QDateTime currentDate = QDateTime::fromString(newsVector[i].getPublishedDate(), Qt::ISODate);
        if (!currentDate.isValid()) {
            qCDebug(BreakingNewsFetcherLog) << "Invalid published date for news at index" << i << ":"
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
        qCDebug(BreakingNewsFetcherLog) << "No valid published dates found in news vector";
        throw std::runtime_error("All published dates in the vector are invalid");
    }

    return *newest;
}

void BreakingNewsFetcher::processReceivedNewsPingPongBuffers()
{
    QMap<QString, QVector<StockNewsResult>> *newBuffer;
    QMap<QString, QVector<StockNewsResult>> *oldBuffer;

    newBuffer = latestNewsPerSymbolPingPongPtr;
    oldBuffer = (latestNewsPerSymbolPingPongPtr == &latestNewsPerSymbolPingPong1) ? &latestNewsPerSymbolPingPong2 : &latestNewsPerSymbolPingPong1;

    qCDebug(BreakingNewsFetcherLog).noquote() << Q_FUNC_INFO << "\n\n                                ****** EXECUTING THE NEWS ALGORITHM ********\n";


    if (oldBuffer->isEmpty()) {
        qCDebug(BreakingNewsFetcherLog) << "First iteration of the pingpong buffer processing. SKIP";
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

                emit foundNewNews(newNews);
            }
        }
    }
}


