#include <QCoreApplication>
#include <QLoggingCategory>
#include <QDateTime>

#include "ArgumentParser.h"
#include "Logging.h"
#include "Settings.h"
#include "BarCache.h"
#include "TSClient.h"
#include "RecorderLogic.h"

#include <QtGlobal>

#include <iostream>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QCoreApplication::setApplicationName("Recorder");
    QString version = QString("%1 ~ %2@%3").arg(GIT_TAG, GIT_BRANCH, GIT_HASH);
    QCoreApplication::setApplicationVersion(version);

    // Initialize logging (opens file and installs handler)
    initLogging();

    qInfo() << "Qt version:" << QT_VERSION_STR;

    qInfo() << "Version:" << version;

    parseArguments(app.arguments());

    qInfo() << "Cache root directory:" << getCacheLocation();
    
    if (stockCsvFile.isEmpty()) {
        qFatal("Stock CSV file not specified");
    } else {
        qInfo() << "Stock CSV file:" << stockCsvFile;
    }

    QStringList stockTickers = loadStockTickers(stockCsvFile);
    qInfo() << "Loaded" << stockTickers.size() << "stock tickers from CSV.";

    QString recordedDataPath = createRecordingFolders(getCacheLocation());
    qInfo() << "Recorded data folder:" << recordedDataPath;

    TSClient::getInstancePtr()->start();

    QString barsPath = recordedDataPath + "/Bars";
    LiveBarsDB* liveBarsDB = initializeBarsDatabase(barsPath);

    liveBarsDB->startRecording();

    qInfo() << "------ Recorder for Bars started - recording market data...";

    LiveMarketDepthQuoteDB* liveMarketDepthQuoteDB = initializeMarketDepthQuoteDatabase(recordedDataPath + "/MarketDepthQuotes", stockTickers);


    liveMarketDepthQuoteDB->startRecording();

    qInfo() << "------ Recorder for Market Depth Quotes started - recording market data...";

    // Write logging configuration to disk if this is the first run
    LoggingConfig::instance().writeConfigToDisk();

    return app.exec();
}