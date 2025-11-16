#include "LiveBarsDB.h"
#include "SqlQueries.h"

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
    query.exec(SqlQueries::CREATE_BARS_TABLE);
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

bool LiveBarsDB::storeBarJson(const QString& stock, qint64 timestamp, const QString& jsonData) {
    int stockSeq = stockSequences.value(stock, 0) + 1;
    stockSequences[stock] = stockSeq;

    QSqlQuery query(db);
    query.prepare(SqlQueries::INSERT_BAR);
    query.addBindValue(stock);
    query.addBindValue(stockSeq);
    query.addBindValue(timestamp);
    query.addBindValue(jsonData);

    if (!query.exec()) {
        qWarning() << "Failed to store bar for" << stock << ":" << query.lastError().text();
        
        // Assert for now becauese storing bars should not fail, maybe handle more gracefully much later
        Q_ASSERT_X(false, "LiveBarsDB::storeBar", "Database insert failed");

        return false;
    }
    return true;
}