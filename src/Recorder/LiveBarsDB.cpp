#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>

#include "LiveBarsDB.h"
#include "SqlQueries.h"
#include "TSClient.h"

LiveBarsDB::LiveBarsDB(const QString& dbPath, QStringList& stockTickers) {
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

    this->stockTickers = stockTickers;
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

void LiveBarsDB::startRecording() {

    for (const QString& symbol : this->stockTickers) {
        StreamBars* streamBar = TSClient::getInstance().openStreamBars(symbol,
                                                                       1,
                                                                       Bar::BarUnit::Minute,
                                                                       2,
                                                                       Bar::BarSessionTemplate::USEQ24Hour);
        Q_ASSERT(streamBar != nullptr);
        connect(streamBar, &StreamBars::receivedNewJson, this, &LiveBarsDB::onReceivedNewJson);

        this->streamBars[symbol] = streamBar;
    }
}

void LiveBarsDB::onReceivedNewJson(QString symbol, const QJsonObject& jsonObj) {
    QJsonDocument doc(jsonObj);
    QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    qint64 timestamp = jsonObj.value("timestamp").toVariant().toLongLong();

    this->storeBarJson(symbol, timestamp, jsonString);
}
