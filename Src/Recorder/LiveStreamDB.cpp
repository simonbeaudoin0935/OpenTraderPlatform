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

bool LiveStreamDB::storeData(const Ticker& stock, qint64 timestamp, const QByteArray& rawData) {
    int stockSeq = stockSequences.value(stock, 0) + 1;
    stockSequences[stock] = stockSeq;

    QSqlQuery query(db);
    QString insertQuery = (streamType == StreamType::Bars) ? SqlQueries::INSERT_BAR : SqlQueries::INSERT_MARKET_DEPTH_QUOTE;
    query.prepare(insertQuery);
    query.addBindValue(stock.toString());
    query.addBindValue(stockSeq);
    query.addBindValue(timestamp);
    query.addBindValue(rawData);
    if (!query.exec()) {
        QString dataType = (streamType == StreamType::Bars) ? "bar" : "market depth quote";
        qWarning() << "Failed to store" << dataType << "for" << stock.toString() << ":" << query.lastError().text();

        // Assert for now because storing should not fail, maybe handle more gracefully much later
        Q_ASSERT_X(false, "LiveStreamDB::storeData", "Database insert failed");

        return false;
    }
    return true;
}

void LiveStreamDB::startRecording() {
    for (const QString& symbol : stockTickers) {
        Ticker ticker(symbol);
        if (streamType == StreamType::Bars) {
            StreamBars* streamBar = TSClient::getInstance().openStreamBars(ticker,
                                                                           1,
                                                                           Bar::BarUnit::Minute,
                                                                           2,
                                                                           Bar::BarSessionTemplate::USEQ24Hour);
            Q_ASSERT(streamBar != nullptr);

            bool connection1 = QObject::connect(streamBar, &StreamBars::receivedNewRawData, this, &LiveStreamDB::onReceivedNewRawDataForStock, Qt::UniqueConnection);
            Q_ASSERT_X(connection1, "LiveStreamDB::startStreams", "Failed to create unique connection for streamBar receivedNewRawData");
            bool connection2 = QObject::connect(streamBar, &Stream::streamErrorOccurred, this, &LiveStreamDB::onStreamErrorOccurred, Qt::UniqueConnection);
            Q_ASSERT_X(connection2, "LiveStreamDB::startStreams", "Failed to create unique connection for streamBar streamErrorOccurred");

            streamBars[ticker] = streamBar;
        } else {
            StreamMarketDepthQuote* streamMarketDepthQuote = TSClient::getInstance().openStreamMarketDepthQuote(ticker, 10); // depth 10
            Q_ASSERT(streamMarketDepthQuote != nullptr);

            bool connection3 = QObject::connect(streamMarketDepthQuote, &StreamMarketDepthQuote::receivedNewRawData, this, &LiveStreamDB::onReceivedNewRawDataForStock, Qt::UniqueConnection);
            Q_ASSERT_X(connection3, "LiveStreamDB::startStreams", "Failed to create unique connection for streamMarketDepthQuote receivedNewRawData");
            bool connection4 = QObject::connect(streamMarketDepthQuote, &Stream::streamErrorOccurred, this, &LiveStreamDB::onStreamErrorOccurred, Qt::UniqueConnection);
            Q_ASSERT_X(connection4, "LiveStreamDB::startStreams", "Failed to create unique connection for streamMarketDepthQuote streamErrorOccurred");

            streamMarketDepthQuotes[ticker] = streamMarketDepthQuote;
        }
    }
}

void LiveStreamDB::onReceivedNewRawDataForStock(Ticker symbol, const QByteArray& rawData) {
    qint64 epochMs = QDateTime::currentMSecsSinceEpoch();

    QString dataType = (streamType == StreamType::Bars) ? "bar" : "market depth quote";
    qDebug() << "Received new" << dataType << "raw JSON data for" << symbol.toString() << "at timestamp" << epochMs;

    // Check if this symbol had an unrecovered timeout and mark it as recovered
    if (unrecoveredTimeouts.contains(symbol)) {
        unrecoveredTimeouts.remove(symbol);
        recoveredTimeouts[symbol]++;
        qInfo() << "Stream recovered from timeout for" << symbol.toString();
    }

    storeData(symbol, epochMs, rawData);
}

void LiveStreamDB::onStreamErrorOccurred(Stream::StreamError error, QString errorMessage) {
    // Find the sender and symbol based on stream type
    Ticker symbol;
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

    // Track timeout recovery and attempt automatic recovery
    if (error == Stream::StreamError::Timeout) {
        unrecoveredTimeouts.insert(symbol);
        // Attempt automatic recovery
        attemptStreamRecovery(symbol);
    }

    qWarning() << "Stream error for" << symbol.toString() << "error:" << static_cast<int>(error) << "message:" << errorMessage;
}

void LiveStreamDB::finalizeUnrecoveredTimeouts() {
    for (const Ticker& symbol : unrecoveredTimeouts) {
        unrecoveredTimeoutCounts[symbol]++;
    }
    unrecoveredTimeouts.clear();
}

void LiveStreamDB::attemptStreamRecovery(const Ticker& symbol) {
    recoveryAttempts[symbol]++;

    qInfo() << "Attempting to recover stream for" << symbol.toString() << "(attempt #" << recoveryAttempts[symbol] << ")";

    if (streamType == StreamType::Bars) {
        // Close existing stream
        if (streamBars.contains(symbol)) {
            StreamBars* oldStream = streamBars[symbol];
            if (oldStream) {
                oldStream->deleteLater();
            }
            streamBars.remove(symbol);
        }

        // Open new stream
        StreamBars* newStream = TSClient::getInstance().openStreamBars(symbol,
                                                                       1,
                                                                       Bar::BarUnit::Minute,
                                                                       2,
                                                                       Bar::BarSessionTemplate::USEQ24Hour);
        if (newStream) {
            bool connection1 = QObject::connect(newStream, &StreamBars::receivedNewRawData, this, &LiveStreamDB::onReceivedNewRawDataForStock, Qt::UniqueConnection);
            Q_ASSERT_X(connection1, "LiveStreamDB::recoverStream", "Failed to create unique connection for newStream receivedNewRawData");
            bool connection2 = QObject::connect(newStream, &Stream::streamErrorOccurred, this, &LiveStreamDB::onStreamErrorOccurred, Qt::UniqueConnection);
            Q_ASSERT_X(connection2, "LiveStreamDB::recoverStream", "Failed to create unique connection for newStream streamErrorOccurred");
            streamBars[symbol] = newStream;
            successfulRecoveries[symbol]++;
            qInfo() << "Successfully recovered bars stream for" << symbol.toString();
        } else {
            qWarning() << "Failed to recover bars stream for" << symbol.toString();
        }
    } else {
        // Close existing stream
        if (streamMarketDepthQuotes.contains(symbol)) {
            StreamMarketDepthQuote* oldStream = streamMarketDepthQuotes[symbol];
            if (oldStream) {
                oldStream->deleteLater();
            }
            streamMarketDepthQuotes.remove(symbol);
        }

        // Open new stream
        StreamMarketDepthQuote* newStream = TSClient::getInstance().openStreamMarketDepthQuote(symbol, 10);
        if (newStream) {
            bool connection3 = QObject::connect(newStream, &StreamMarketDepthQuote::receivedNewRawData, this, &LiveStreamDB::onReceivedNewRawDataForStock, Qt::UniqueConnection);
            Q_ASSERT_X(connection3, "LiveStreamDB::recoverStream", "Failed to create unique connection for newStream receivedNewRawData");
            bool connection4 = QObject::connect(newStream, &Stream::streamErrorOccurred, this, &LiveStreamDB::onStreamErrorOccurred, Qt::UniqueConnection);
            Q_ASSERT_X(connection4, "LiveStreamDB::recoverStream", "Failed to create unique connection for newStream streamErrorOccurred");
            streamMarketDepthQuotes[symbol] = newStream;
            successfulRecoveries[symbol]++;
            qInfo() << "Successfully recovered market depth stream for" << symbol.toString();
        } else {
            qWarning() << "Failed to recover market depth stream for" << symbol.toString();
        }
    }
}

int LiveStreamDB::getRecordCount() const {
    QSqlQuery query(db);
    QString tableName = (streamType == StreamType::Bars) ? "bars" : "market_depth_quotes";
    query.prepare(QString("SELECT COUNT(*) FROM %1").arg(tableName));
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int LiveStreamDB::getActiveStreamCount() const {
    int activeCount = 0;
    if (streamType == StreamType::Bars) {
        for (auto it = streamBars.begin(); it != streamBars.end(); ++it) {
            if (it.value() && !it.value()->isFinished() && !it.value()->isInError()) {
                activeCount++;
            }
        }
    } else {
        for (auto it = streamMarketDepthQuotes.begin(); it != streamMarketDepthQuotes.end(); ++it) {
            if (it.value() && !it.value()->isFinished() && !it.value()->isInError()) {
                activeCount++;
            }
        }
    }
    return activeCount;
}