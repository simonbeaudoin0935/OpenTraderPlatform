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

    QString barsPath = recordedDataPath + "/Bars";
    initializeBarsDatabase(barsPath);

    // TODO: Implement recording logic here
    qInfo() << "Recorder started - recording market data...";


    TSClient* tradeStationClient = TSClient::getInstancePtr();

    tradeStationClient->start();

    BarCache barCache("AAPL", true);

    return app.exec();
}