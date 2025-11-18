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

bool LiveBarsDB::storeBarRawData(const QString& stock, qint64 timestamp, const QByteArray& rawData) {
    int stockSeq = stockSequences.value(stock, 0) + 1;
    stockSequences[stock] = stockSeq;

    QSqlQuery query(db);
    query.prepare(SqlQueries::INSERT_BAR);
    query.addBindValue(stock);
    query.addBindValue(stockSeq);
    query.addBindValue(timestamp);
    query.addBindValue(rawData);
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

        QObject::connect(streamBar, &StreamBars::receivedNewRawData, this, &LiveBarsDB::onReceivedNewRawDataForStock);
        QObject::connect(streamBar, &Stream::streamErrorOccurred, this, &LiveBarsDB::onStreamErrorOccurred);

        this->streamBars[symbol] = streamBar;
    }
}

void LiveBarsDB::onReceivedNewRawDataForStock(QString symbol, const QByteArray& rawData) {

    qint64 epochMs = QDateTime::currentMSecsSinceEpoch();

    qDebug() << "Received new bar raw JSON data for" << symbol << "at timestamp" << epochMs;
    
    // Check if this symbol had an unrecovered timeout and mark it as recovered
    if (unrecoveredTimeouts.contains(symbol)) {
        unrecoveredTimeouts.remove(symbol);
        recoveredTimeouts[symbol]++;
        qInfo() << "Stream recovered from timeout for" << symbol;
    }
    
    this->storeBarRawData(symbol, epochMs, rawData);
}

void LiveBarsDB::finalizeUnrecoveredTimeouts() {
    for (const QString& symbol : unrecoveredTimeouts) {
        unrecoveredTimeoutCounts[symbol]++;
    }
    unrecoveredTimeouts.clear();
}

void LiveBarsDB::onStreamErrorOccurred(Stream::StreamError error, QString errorMessage) {
    StreamBars* senderStream = qobject_cast<StreamBars*>(sender());
    Q_ASSERT(senderStream);  // Should always be valid - catastrophic error if not
    
    // Find the symbol by looking up the sender in our stream map
    QString symbol;
    for (auto it = streamBars.begin(); it != streamBars.end(); ++it) {
        if (it.value() == senderStream) {
            symbol = it.key();
            break;
        }
    }
    Q_ASSERT(!symbol.isEmpty());  // Should always find the symbol - catastrophic error if not
    
    streamErrorCounters[symbol][error]++;
    
    // Track timeout recovery
    if (error == Stream::StreamError::Timeout) {
        unrecoveredTimeouts.insert(symbol);
    }
    
    qWarning() << "Stream error for" << symbol << "error:" << static_cast<int>(error) << "message:" << errorMessage;
}
