#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>

#include "LiveStreamDB.h"
#include "SqlQueries.h"
#include "TSClient.h"

LiveStreamDB::LiveStreamDB(StreamType type, const QString& dbPath, QStringList& stockTickers)
    : streamType(type), stockTickers(stockTickers)
{
    QString connectionName = (type == StreamType::Bars) ? "LiveBarsDB" : "LiveMarketDepthQuoteDB";
    db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qFatal("Failed to open live %s database: %s",
               (type == StreamType::Bars) ? "bars" : "market depth quotes",
               qPrintable(db.lastError().text()));
    }

    QSqlQuery query(db);
    QString tableQuery = (type == StreamType::Bars) ? SqlQueries::CREATE_BARS_TABLE : SqlQueries::CREATE_MARKET_DEPTH_QUOTES_TABLE;
    query.exec(tableQuery);
    if (query.lastError().isValid()) {
        qWarning() << "Failed to create" << ((type == StreamType::Bars) ? "bars" : "market depth quotes") << "table:" << query.lastError().text();
    }

    QString dbType = (type == StreamType::Bars) ? "bars" : "market depth quotes";
    qInfo() << "Live" << dbType << "database opened at" << dbPath;
}

LiveStreamDB::~LiveStreamDB() {
    if (db.isOpen()) {
        db.close();
    }
}

bool LiveStreamDB::isOpen() const {
    return db.isOpen();
}

bool LiveStreamDB::storeData(const QString& stock, qint64 timestamp, const QByteArray& rawData) {
    int stockSeq = stockSequences.value(stock, 0) + 1;
    stockSequences[stock] = stockSeq;

    QSqlQuery query(db);
    QString insertQuery = (streamType == StreamType::Bars) ? SqlQueries::INSERT_BAR : SqlQueries::INSERT_MARKET_DEPTH_QUOTE;
    query.prepare(insertQuery);
    query.addBindValue(stock);
    query.addBindValue(stockSeq);
    query.addBindValue(timestamp);
    query.addBindValue(rawData);
    if (!query.exec()) {
        QString dataType = (streamType == StreamType::Bars) ? "bar" : "market depth quote";
        qWarning() << "Failed to store" << dataType << "for" << stock << ":" << query.lastError().text();

        // Assert for now because storing should not fail, maybe handle more gracefully much later
        Q_ASSERT_X(false, "LiveStreamDB::storeData", "Database insert failed");

        return false;
    }
    return true;
}

void LiveStreamDB::startRecording() {
    for (const QString& symbol : stockTickers) {
        if (streamType == StreamType::Bars) {
            StreamBars* streamBar = TSClient::getInstance().openStreamBars(symbol,
                                                                           1,
                                                                           Bar::BarUnit::Minute,
                                                                           2,
                                                                           Bar::BarSessionTemplate::USEQ24Hour);
            Q_ASSERT(streamBar != nullptr);

            QObject::connect(streamBar, &StreamBars::receivedNewRawData, this, &LiveStreamDB::onReceivedNewRawDataForStock);
            QObject::connect(streamBar, &Stream::streamErrorOccurred, this, &LiveStreamDB::onStreamErrorOccurred);

            streamBars[symbol] = streamBar;
        } else {
            StreamMarketDepthQuote* streamMarketDepthQuote = TSClient::getInstance().openStreamMarketDepthQuote(symbol, 10); // depth 10
            Q_ASSERT(streamMarketDepthQuote != nullptr);

            QObject::connect(streamMarketDepthQuote, &StreamMarketDepthQuote::receivedNewRawData, this, &LiveStreamDB::onReceivedNewRawDataForStock);
            QObject::connect(streamMarketDepthQuote, &Stream::streamErrorOccurred, this, &LiveStreamDB::onStreamErrorOccurred);

            streamMarketDepthQuotes[symbol] = streamMarketDepthQuote;
        }
    }
}

void LiveStreamDB::onReceivedNewRawDataForStock(QString symbol, const QByteArray& rawData) {
    qint64 epochMs = QDateTime::currentMSecsSinceEpoch();

    QString dataType = (streamType == StreamType::Bars) ? "bar" : "market depth quote";
    qDebug() << "Received new" << dataType << "raw JSON data for" << symbol << "at timestamp" << epochMs;

    // Check if this symbol had an unrecovered timeout and mark it as recovered
    if (unrecoveredTimeouts.contains(symbol)) {
        unrecoveredTimeouts.remove(symbol);
        recoveredTimeouts[symbol]++;
        qInfo() << "Stream recovered from timeout for" << symbol;
    }

    storeData(symbol, epochMs, rawData);
}

void LiveStreamDB::onStreamErrorOccurred(Stream::StreamError error, QString errorMessage) {
    // Find the sender and symbol based on stream type
    QString symbol;
    if (streamType == StreamType::Bars) {
        StreamBars* senderStream = qobject_cast<StreamBars*>(sender());
        Q_ASSERT(senderStream);  // Should always be valid - catastrophic error if not

        // Find the symbol by looking up the sender in our stream map
        for (auto it = streamBars.begin(); it != streamBars.end(); ++it) {
            if (it.value() == senderStream) {
                symbol = it.key();
                break;
            }
        }
    } else {
        StreamMarketDepthQuote* senderStream = qobject_cast<StreamMarketDepthQuote*>(sender());
        Q_ASSERT(senderStream);  // Should always be valid - catastrophic error if not

        // Find the symbol by looking up the sender in our stream map
        for (auto it = streamMarketDepthQuotes.begin(); it != streamMarketDepthQuotes.end(); ++it) {
            if (it.value() == senderStream) {
                symbol = it.key();
                break;
            }
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

void LiveStreamDB::finalizeUnrecoveredTimeouts() {
    for (const QString& symbol : unrecoveredTimeouts) {
        unrecoveredTimeoutCounts[symbol]++;
    }
    unrecoveredTimeouts.clear();
}