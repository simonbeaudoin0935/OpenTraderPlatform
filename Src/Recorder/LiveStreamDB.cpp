#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>

#include "LiveStreamDB.h"
#include "SqlQueries.h"
#include "TSClient.h"
#include "Logging.h"

#define LOGGING_CATEGORY LiveStreamDBLog
Q_LOGGING_CATEGORY(LiveStreamDBLog, "LiveStreamDB");

LiveStreamDB::LiveStreamDB(StreamType type, const QString& dbPath, QStringList& stockTickers)
    : streamType(type), stockTickers(stockTickers)
{
    QString connectionName = (type == StreamType::Bars) ? "LiveBarsDB" : "LiveMarketDepthQuoteDB";

    setObjectName("LiveStreamDB::" + connectionName);

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
        WARNING << "Failed to create" << ((type == StreamType::Bars) ? "bars" : "market depth quotes") << "table:" << query.lastError().text();
    }

    QString dbType = (type == StreamType::Bars) ? "bars" : "market depth quotes";
    INFO  << "Live" << dbType << "database opened at" << dbPath;
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
        WARNING << "Failed to store" << dataType << "for" << stock << ":" << query.lastError().text();

        // Assert for now because storing should not fail, maybe handle more gracefully much later
        Q_ASSERT_X(false, "LiveStreamDB::storeData", "Database insert failed");

        return false;
    }
    return true;
}

void LiveStreamDB::startRecording() {
    for (const QString& symbol : stockTickers) {
        if (streamType == StreamType::Bars) {
            QPointer<StreamBars> stream = TSClient::getInstance()->openStreamBars(symbol,
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
                [this, symbol](){
                    CRITICAL << "StreamBars Receiver future finished for " << symbol;
                }
            ).onFailed(this,
                [this, symbol](const std::exception& e){

                    CRITICAL << "StreamBars Receiver failed for" << symbol
                             << "- Exception:" << QString::fromStdString(e.what());

                    Q_ASSERT(false);

                    //TODO attempt to restart the stream

                    // if we have to know that is the source of the failure. 
                    // if its an invalid symbol, then we dont restart
                    // if its a timeout then we restart
                    
                }
            );

            m_streamBars[symbol] = stream;
        } else {
            QPointer<StreamMarketDepthQuote> stream = TSClient::getInstance()->openStreamMarketDepthQuote(symbol, 10); // depth 10
            Q_ASSERT(stream != nullptr);


            QObject::connect(stream, &StreamMarketDepthQuote::receivedNewRawData, this, 
                [this, symbol = stream->getSymbol()](const QByteArray& rawData){
                    onReceivedNewRawDataForStock(symbol, rawData);
                });

            stream->future().then(this,
                [this, symbol](){
                    CRITICAL << "StreamMarketDepthQuote Receiver future finished for " << symbol;
                }
            ).onFailed(this,
                [this, symbol](const std::exception& e){
                    CRITICAL << "StreamMarketDepthQuote Receiver failed for" << symbol
                             << "- Exception:" << QString::fromStdString(e.what());

                    Q_ASSERT(false);

                    //TODO attempt to restart the stream
                }
            );

            m_streamMarketDepthQuotes[symbol] = stream;
        }
    }
}

void LiveStreamDB::onReceivedNewRawDataForStock(QString symbol, const QByteArray& rawData) {
    qint64 epochMs = QDateTime::currentMSecsSinceEpoch();

    QString dataType = (streamType == StreamType::Bars) ? "bar" : "market depth quote";
    DEBUG << "Received new" << dataType << "raw JSON data for" << symbol << "at timestamp" << epochMs;

    // Check if this symbol had an unrecovered timeout and mark it as recovered
    if (unrecoveredTimeouts.contains(symbol)) {
        unrecoveredTimeouts.remove(symbol);
        recoveredTimeouts[symbol]++;
        qInfo() << "Stream recovered from timeout for" << symbol;
    }

    storeData(symbol, epochMs, rawData);
}

// TODO fix this shit, and plug that everywhere in this file where we have todos about acting on the stream erroring
/*
void LiveStreamDB::onStreamErrorOccurred(QString errorMessage) {
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
*/

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
        if (m_streamBars.contains(symbol)) {
            QPointer<StreamBars> oldStream = m_streamBars[symbol];
            if (oldStream) {
                TSClient::getInstance()->closeStream(oldStream);
            }
            m_streamBars.remove(symbol);
        }

        // Open new stream
        QPointer<StreamBars> stream = TSClient::getInstance()->openStreamBars(symbol,
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
            [this, symbol](){
                CRITICAL << "Bar Receiver Receiver future finished for " << symbol;
            }
        ).onFailed(this,
            [this, symbol](const std::exception& e){

                CRITICAL << "Bar Receiver Receiver failed for" << symbol
                        << "- Exception:" << QString::fromStdString(e.what());

                Q_ASSERT(false);

                //TODO attempt to restart the stream
            }
        );

        m_streamBars[symbol] = stream;
        successfulRecoveries[symbol]++;
        qInfo() << "Successfully recovered bars stream for" << symbol;
    } else {
        // Close existing stream
        if (m_streamMarketDepthQuotes.contains(symbol)) {
            StreamMarketDepthQuote* oldStream = m_streamMarketDepthQuotes[symbol];
            if (oldStream) {
                TSClient::getInstance()->closeStream(oldStream);
            }
            m_streamMarketDepthQuotes.remove(symbol);
        }

        // Open new stream
        StreamMarketDepthQuote* stream = TSClient::getInstance()->openStreamMarketDepthQuote(symbol, 10);
        Q_CHECK_PTR(stream);

        QObject::connect(stream, &StreamMarketDepthQuote::receivedNewRawData, this, 
            [this, symbol](const QByteArray& rawData){
                onReceivedNewRawDataForStock(symbol, rawData);
            });

        stream->future().then(this,
            [this, symbol](){
                CRITICAL << "Recorder Bar receiver bar future finished for " << symbol;
            }
        ).onFailed(this,
            [this, symbol](const std::exception& e){
                CRITICAL << "Recorder Bar receiver future failed for" << symbol
                         << "- Exception:" << QString::fromStdString(e.what());

                    Q_ASSERT(false);

                //TODO attempt to restart the stream
            }
        );

        m_streamMarketDepthQuotes[symbol] = stream;
        successfulRecoveries[symbol]++;
        INFO << "Successfully recovered market depth stream for" << symbol;
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

    // When an error occurs in a stream, it auto deletes itself, and thanks to QPointer we can detect that here

    if (streamType == StreamType::Bars) {
        for (QPointer<StreamBars> stream : m_streamBars) {
            if (stream != nullptr) {
                activeCount++;
            }
        }
    } else {
        for (QPointer<StreamMarketDepthQuote> stream : m_streamMarketDepthQuotes) {
    
            if (stream != nullptr) {
                activeCount++;
            }
        }
    }

    return activeCount;
}