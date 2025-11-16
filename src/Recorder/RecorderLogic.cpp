#include "RecorderLogic.h"
#include "Settings.h"

#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDate>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

QStringList loadStockTickers(const QString& csvFilePath) {
    QStringList stockTickers;
    QFile file(csvFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qFatal("Cannot open stock CSV file: %s", qUtf8Printable(csvFilePath));
    }
    QTextStream in(&file);
    QString header = in.readLine(); // Skip header line
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList fields = line.split(',');
        if (!fields.isEmpty() && !fields[0].isEmpty()) {
            stockTickers.append(fields[0]);
        }
    }
    file.close();
    return stockTickers;
}

QString createRecordingFolders(const QString& cacheLocation) {
    QDir cacheDir(cacheLocation);

    Q_ASSERT_X(cacheDir.exists(), "createRecordingFolders", "Cache directory does not exist");

    QString recordedDataPath = cacheLocation + "/RecordedLiveData";
    QDir recordedDir(recordedDataPath);
    if (!recordedDir.exists()) {
        if (!recordedDir.mkpath(".")) {
            qFatal("Cannot create RecordedLiveData directory: %s", qUtf8Printable(recordedDataPath));
        }
    }
    QString barsPath = recordedDataPath + "/Bars";
    QDir barsDir(barsPath);
    if (!barsDir.exists()) {
        if (!barsDir.mkpath(".")) {
            qFatal("Cannot create Bars directory: %s", qUtf8Printable(barsPath));
        }
    }
    QString marketDepthPath = recordedDataPath + "/MarketDepth";
    QDir mdDir(marketDepthPath);
    if (!mdDir.exists()) {
        if (!mdDir.mkpath(".")) {
            qFatal("Cannot create MarketDepth directory: %s", qUtf8Printable(marketDepthPath));
        }
    }
    return recordedDataPath;
}

LiveBarsDB* initializeBarsDatabase(const QString& barsPath) {
    QString dateStr = QDate::currentDate().toString("yyyy-MM-dd");
    QString dbPath = barsPath + "/RecordedLiveBars_" + dateStr + ".db";
    
    QStringList stockTickers = loadStockTickers(stockCsvFile);
    
    return new LiveBarsDB(dbPath, stockTickers);
}