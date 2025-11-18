#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>

#include "LiveMarketDepthQuoteDB.h"
#include "SqlQueries.h"
#include "TSClient.h"

LiveMarketDepthQuoteDB::LiveMarketDepthQuoteDB(const QString& dbPath, QStringList& stockTickers) {
    db = QSqlDatabase::addDatabase("QSQLITE", "LiveMarketDepthQuoteDB");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qFatal("Failed to open live market depth quotes database: %s", qPrintable(db.lastError().text()));
    }

    QSqlQuery query(db);
    query.exec(SqlQueries::CREATE_MARKET_DEPTH_QUOTES_TABLE);
    if (query.lastError().isValid()) {
        qWarning() << "Failed to create market depth quotes table:" << query.lastError().text();
    }
    qInfo() << "Live market depth quotes database opened at" << dbPath;

    this->stockTickers = stockTickers;
}

LiveMarketDepthQuoteDB::~LiveMarketDepthQuoteDB() {
    if (db.isOpen()) {
        db.close();
    }
}

bool LiveMarketDepthQuoteDB::isOpen() const {
    return db.isOpen();
}

bool LiveMarketDepthQuoteDB::storeMarketDepthQuoteRawData(const QString& stock, qint64 timestamp, const QByteArray& rawData) {
    int stockSeq = stockSequences.value(stock, 0) + 1;
    stockSequences[stock] = stockSeq;

    QSqlQuery query(db);
    query.prepare(SqlQueries::INSERT_MARKET_DEPTH_QUOTE);
    query.addBindValue(stock);
    query.addBindValue(stockSeq);
    query.addBindValue(timestamp);
    query.addBindValue(rawData);
    if (!query.exec()) {
        qWarning() << "Failed to store market depth quote for" << stock << ":" << query.lastError().text();
        
        // Assert for now because storing should not fail, maybe handle more gracefully much later
        Q_ASSERT_X(false, "LiveMarketDepthQuoteDB::storeMarketDepthQuoteRawData", "Database insert failed");

        return false;
    }
    return true;
}

void LiveMarketDepthQuoteDB::startRecording() {

    for (const QString& symbol : this->stockTickers) {
        StreamMarketDepthQuote* streamMarketDepthQuote = TSClient::getInstance().openStreamMarketDepthQuote(symbol, 10); // depth 10
        Q_ASSERT(streamMarketDepthQuote != nullptr);

        QObject::connect(streamMarketDepthQuote, &StreamMarketDepthQuote::receivedNewRawData, this, &LiveMarketDepthQuoteDB::onReceivedNewRawDataForStock);

        this->streamMarketDepthQuotes[symbol] = streamMarketDepthQuote;
    }
}

void LiveMarketDepthQuoteDB::onReceivedNewRawDataForStock(QString symbol, const QByteArray& rawData) {

    qint64 epochMs = QDateTime::currentMSecsSinceEpoch();

    qDebug() << "Received new market depth quote raw JSON data for" << symbol << "at timestamp" << epochMs;
    
    this->storeMarketDepthQuoteRawData(symbol, epochMs, rawData);
}