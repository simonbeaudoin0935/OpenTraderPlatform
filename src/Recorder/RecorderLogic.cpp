#include "RecorderLogic.h"

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

void initializeBarsDatabase(const QString& barsPath) {
    QString dateStr = QDate::currentDate().toString("yyyy-MM-dd");
    QString dbPath = barsPath + "/RecordedLiveBars_" + dateStr + ".db";
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "RecorderBars");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qFatal("Failed to open bars database: %s", qPrintable(db.lastError().text()));
    }
    QSqlQuery query(db);
    query.exec("CREATE TABLE IF NOT EXISTS bars ("
               "timestamp INTEGER, "
               "stock TEXT, "
               "open REAL, "
               "high REAL, "
               "low REAL, "
               "close REAL, "
               "volume INTEGER, "
               "PRIMARY KEY (timestamp, stock))");
    if (query.lastError().isValid()) {
        qWarning() << "Failed to create bars table:" << query.lastError().text();
    }
    qInfo() << "Initialized bars database at" << dbPath;
}