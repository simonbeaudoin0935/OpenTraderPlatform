#include <QThread>
#include <QTimer>
#include <QTextStream>
#include <QDir>

#include "MainAlgo.h"

Q_LOGGING_CATEGORY(MainAlgoLog, "MainAlgo")

//TODO should be parameters
static const int newsFetchingInterval = 30;
static const int newsFetchDepthLimit = 2;

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

void MainAlgo::onThreadStarted()
{
    connect(&stockScreener, &StockScreener::finished, this, &MainAlgo::onStockScreenerFinished);
    //stockScreener.start();

    connect(&stockBarsReceiver, &StockBarsReceiver::currentHighlightedReceivedNewBar,
            this, &MainAlgo::currentHighlightedReceivedNewBar);

    stockBarsReceiver.startStream("AAPL");
}

void MainAlgo::onStockScreenerFinished()
{
    // disconnect?

    connect(&breakingNewsFetcher, &BreakingNewsFetcher::foundNewNews, this, &MainAlgo::onNewNewsFound);

    breakingNewsFetcher.start(stockScreener.getStockScreeningResult(),
                              newsFetchDepthLimit,
                              newsFetchingInterval);
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
