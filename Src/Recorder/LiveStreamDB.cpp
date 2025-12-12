#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>

#include "LiveStreamDB.h"
#include "SqlQueries.h"
#include "TSClient.h"
#include "Logging.h"

#define LOGGING_CATEGORY StreamLog


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
            StreamBars* stream = TSClient::getInstance()->openStreamBars(symbol,
                                                                           1,
                                                                           Bar::BarUnit::Minute,
                                                                           2,
                                                                           Bar::BarSessionTemplate::USEQ24Hour);
            Q_ASSERT(stream != nullptr);

            QObject::connect(stream, &StreamBars::receivedNewRawData, this,
                [this, symbol](const QByteArray& rawData){
                    onReceivedNewRawDataForStock(symbol, rawData);
                });

            stream->future().then(this,
                [this, stream](){
                    CRITICAL << "StreamBars Receiver future finished for " << stream->getSymbol();
                }
            ).onFailed(this,
                [this, stream](QException ex){
                    Q_UNUSED(ex);

                    WARNING << "StreamBars Receiver failed for" << stream->getSymbol()
                            << "- Exception:" << stream->errorToString();

                    CRITICAL << "TODO : deal with this";
                }
            );



            streamBars[symbol] = stream;
        } else {
            StreamMarketDepthQuote* stream = TSClient::getInstance()->openStreamMarketDepthQuote(symbol, 10); // depth 10
            Q_ASSERT(stream != nullptr);


            QObject::connect(stream, &StreamMarketDepthQuote::receivedNewRawData, this, 
                [this, symbol = stream->getSymbol()](const QByteArray& rawData){
                    onReceivedNewRawDataForStock(symbol, rawData);
                });

            stream->future().then(this,
                [this, stream](){
                    CRITICAL << "StreamMarketDepthQuote Receiver future finished";
                }
            ).onFailed(this,
                [this, stream](QException ex){
                    Q_UNUSED(ex);

                    WARNING << "StreamMarketDepthQuote Receiver failed for" << stream->getSymbol()
                            << "- Exception:" << stream->errorToString();

                    CRITICAL << "TODO : deal with this";
                }
            );

            streamMarketDepthQuotes[symbol] = stream;
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

    // Track timeout recovery and attempt automatic recovery
    if (error == Stream::StreamError::Timeout) {
        unrecoveredTimeouts.insert(symbol);
        // Attempt automatic recovery
        attemptStreamRecovery(symbol);
    }

    qWarning() << "Stream error for" << symbol << "error:" << static_cast<int>(error) << "message:" << errorMessage;
}

void LiveStreamDB::finalizeUnrecoveredTimeouts() {
    for (const QString& symbol : unrecoveredTimeouts) {
        unrecoveredTimeoutCounts[symbol]++;
    }
    unrecoveredTimeouts.clear();
}

void LiveStreamDB::attemptStreamRecovery(const QString& symbol) {
    recoveryAttempts[symbol]++;

    qInfo() << "Attempting to recover stream for" << symbol << "(attempt #" << recoveryAttempts[symbol] << ")";

    if (streamType == StreamType::Bars) {
        // Close existing stream
        if (streamBars.contains(symbol)) {
            StreamBars* oldStream = streamBars[symbol];
            if (oldStream) {
                TSClient::getInstance()->closeStream(oldStream);
            }
            streamBars.remove(symbol);
        }

        // Open new stream
        StreamBars* stream = TSClient::getInstance()->openStreamBars(symbol,
                                                                       1,
                                                                       Bar::BarUnit::Minute,
                                                                       2,
                                                                       Bar::BarSessionTemplate::USEQ24Hour);
        if (stream) {
            QObject::connect(stream, &StreamBars::receivedNewRawData, this, 
                [this, symbol = stream->getSymbol()](const QByteArray& rawData){
                    onReceivedNewRawDataForStock(symbol, rawData);
                });


            stream->future().then(this,
                [this, stream](){
                    CRITICAL << "Bar Receiver Receiver future finished";
                }
            ).onFailed(this,
                [this, stream](QException ex){
                    Q_UNUSED(ex);

                    WARNING << "Bar Receiver Receiver failed for" << stream->getSymbol()
                            << "- Exception:" << stream->errorToString();

                    CRITICAL << "TODO : deal with this";
                }
            );

            streamBars[symbol] = stream;
            successfulRecoveries[symbol]++;
            qInfo() << "Successfully recovered bars stream for" << symbol;
        } else {
            qWarning() << "Failed to recover bars stream for" << symbol;
        }
    } else {
        // Close existing stream
        if (streamMarketDepthQuotes.contains(symbol)) {
            StreamMarketDepthQuote* oldStream = streamMarketDepthQuotes[symbol];
            if (oldStream) {
                TSClient::getInstance()->closeStream(oldStream);
            }
            streamMarketDepthQuotes.remove(symbol);
        }

        // Open new stream
        StreamMarketDepthQuote* stream = TSClient::getInstance()->openStreamMarketDepthQuote(symbol, 10);
        if (stream) {
            QObject::connect(stream, &StreamMarketDepthQuote::receivedNewRawData, this, 
                [this, symbol = stream->getSymbol()](const QByteArray& rawData){
                    onReceivedNewRawDataForStock(symbol, rawData);
                });

            stream->future().then(this,
                [this, stream](){
                    CRITICAL << "Recorder Bar receiver bar future finished";
                }
            ).onFailed(this,
                [this, stream](QException ex){
                    Q_UNUSED(ex);

                    WARNING << "Recorder Bar receiver future failed for" << stream->getSymbol()
                            << "- Exception:" << stream->errorToString();

                    CRITICAL << "TODO : deal with this";
                }
            );

            streamMarketDepthQuotes[symbol] = stream;
            successfulRecoveries[symbol]++;
            qInfo() << "Successfully recovered market depth stream for" << symbol;
        } else {
            qWarning() << "Failed to recover market depth stream for" << symbol;
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