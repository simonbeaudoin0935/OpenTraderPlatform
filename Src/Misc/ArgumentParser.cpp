#include <QCommandLineParser>
#include <QFileInfo>

#include "ArgumentParser.h"
#include "Settings.h"


void parseArguments(const QStringList &args) {

    QCommandLineParser parser;
    parser.setApplicationDescription("TradeStation Trading Algorithm");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption configCriteriaOption("criterias", "Path to the criterias file (e.g., criterias.ini)", "file", "./criterias.ini");
    parser.addOption(configCriteriaOption);

    QCommandLineOption cacheRootDirOption("cache-root-dir", "Root directory for cache files", "dir");
    parser.addOption(cacheRootDirOption);

    QCommandLineOption recordedDataDirOption("recorded-data-dir", "Directory for recorded live data (bars and market depth)", "dir");
    parser.addOption(recordedDataDirOption);

    QCommandLineOption stockCsvOption("stock-csv", "Path to CSV file containing stock tickers (first column)", "file");
    parser.addOption(stockCsvOption);

    // Process command-line arguments
    parser.process(args);



    QString criteriaFile = parser.value(configCriteriaOption);
    {
        QFileInfo fileInfo(criteriaFile);
        if (fileInfo.exists() && fileInfo.isFile()) {
            criteriaSettings = new QSettings(criteriaFile, QSettings::IniFormat);
        } else {
            // For unit tests or when criteria file is not needed, create empty settings
            criteriaSettings = new QSettings();
        }
    }

    QString cacheDir = parser.value(cacheRootDirOption);
    if (!cacheDir.isEmpty()) {
        cacheRootDir = cacheDir;
        qInfo() << "Cache root directory set to:" << cacheRootDir;
    }

    QString recordedDir = parser.value(recordedDataDirOption);
    if (!recordedDir.isEmpty()) {
        recordedDataDir = recordedDir;
        qInfo() << "Recorded data directory set to:" << recordedDataDir;
    }

    QString stockCsv = parser.value(stockCsvOption);
    if (!stockCsv.isEmpty()) {
        QFileInfo fileInfo(stockCsv);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            qFatal("Error: The specified stock CSV file does not exist or is not a file: %s", qUtf8Printable(stockCsv));
        }
        stockCsvFile = stockCsv;
    }
}
