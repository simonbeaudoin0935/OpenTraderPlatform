#include "LiveBarsDB.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>

LiveBarsDB::LiveBarsDB(const QString& dbPath) {
    db = QSqlDatabase::addDatabase("QSQLITE", "LiveBarsDB");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qFatal("Failed to open live bars database: %s", qPrintable(db.lastError().text()));
    }

    QSqlQuery query(db);
    query.exec("CREATE TABLE IF NOT EXISTS bars ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "stock TEXT, "
               "timestamp INTEGER, "
               "open REAL, "
               "high REAL, "
               "low REAL, "
               "close REAL, "
               "volume INTEGER, "
               "stock_sequence INTEGER)");
    if (query.lastError().isValid()) {
        qWarning() << "Failed to create bars table:" << query.lastError().text();
    }
    qInfo() << "Live bars database opened at" << dbPath;
}

LiveBarsDB::~LiveBarsDB() {
    if (db.isOpen()) {
        db.close();
    }
}

bool LiveBarsDB::isOpen() const {
    return db.isOpen();
}

bool LiveBarsDB::storeBar(const QString& stock, qint64 timestamp, double open, double high, double low, double close, qint64 volume) {
    int stockSeq = stockSequences.value(stock, 0) + 1;
    stockSequences[stock] = stockSeq;

    QSqlQuery query(db);
    query.prepare("INSERT INTO bars (stock, timestamp, open, high, low, close, volume, stock_sequence) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(stock);
    query.addBindValue(timestamp);
    query.addBindValue(open);
    query.addBindValue(high);
    query.addBindValue(low);
    query.addBindValue(close);
    query.addBindValue(volume);
    query.addBindValue(stockSeq);

    if (!query.exec()) {
        qWarning() << "Failed to store bar for" << stock << ":" << query.lastError().text();
        
        // Assert for now becauese storing bars should not fail, maybe handle more gracefully much later
        Q_ASSERT_X(false, "LiveBarsDB::storeBar", "Database insert failed");

        return false;
    }
    return true;
}