#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>

#include "LiveStreamDB.h"
#include "SQL/LiveStreamDBQueries.h"
#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"

#define LOGGING_CATEGORY LiveStreamDBLog
Q_LOGGING_CATEGORY(LiveStreamDBLog, "LiveStreamDB");

LiveStreamDB::LiveStreamDB(StreamType type, const QString& dbPath, QStringList& p_stockTickers)
    : streamType(type), stockTickers(p_stockTickers)
{
    QString connectionName = (type == StreamType::Bars) ? "LiveBarsDB" : "LiveMarketDepthQuoteDB";

    setObjectName("LiveStreamDB::" + connectionName);

    db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open())
    {
        qFatal("Failed to open live %s database: %s",
               (type == StreamType::Bars) ? "bars" : "market depth quotes",
               qPrintable(db.lastError().text()));
    }

    QSqlQuery query(db);
    QString tableQuery = (type == StreamType::Bars) ? LiveStreamDBQueries::CREATE_BARS_TABLE
                                                    : LiveStreamDBQueries::CREATE_MARKET_DEPTH_QUOTES_TABLE;
    query.exec(tableQuery);
    if (query.lastError().isValid())
    {
        WARNING << "Failed to create" << ((type == StreamType::Bars) ? "bars" : "market depth quotes")
                << "table:" << query.lastError().text();
    }

    QString dbType = (type == StreamType::Bars) ? "bars" : "market depth quotes";
    INFO << "Live" << dbType << "database opened at" << dbPath;
}

LiveStreamDB::~LiveStreamDB()
{
    if (db.isOpen())
    {
        db.close();
    }
}

bool LiveStreamDB::isOpen() const
{
    return db.isOpen();
}

bool LiveStreamDB::storeData(const QString& stock, qint64 timestamp, const QByteArray& rawData)
{
    int stockSeq = stockSequences.value(stock, 0) + 1;
    stockSequences[stock] = stockSeq;

    QSqlQuery query(db);
    QString insertQuery = (streamType == StreamType::Bars) ? LiveStreamDBQueries::INSERT_BAR
                                                           : LiveStreamDBQueries::INSERT_MARKET_DEPTH_QUOTE;
    query.prepare(insertQuery);
    query.addBindValue(stock);
    query.addBindValue(stockSeq);
    query.addBindValue(timestamp);
    query.addBindValue(rawData);
    if (!query.exec())
    {
        QString dataType = (streamType == StreamType::Bars) ? "bar" : "market depth quote";
        WARNING << "Failed to store" << dataType << "for" << stock << ":" << query.lastError().text();

        // Assert for now because storing should not fail, maybe handle more gracefully much later
        // Database insert failed
        Q_UNREACHABLE();

        return false;
    }
    return true;
}

void LiveStreamDB::startRecording()
{
    for (const QString& symbol: stockTickers)
    {
        if (streamType == StreamType::Bars)
        {
            QPointer<StreamBars> stream = TSClient::getInstance()->openStreamBars(symbol,
                                                                                  1,
                                                                                  Bar::BarUnit::Minute,
                                                                                  0,
                                                                                  Bar::BarSessionTemplate::USEQ24Hour);
            OBJ_ASSUME_TRUE(stream != nullptr);

            QObject::connect(stream,
                             &StreamBars::receivedNewRawData,
                             this,
                             [this, symbol](const QByteArray& rawData)
                             { onReceivedNewRawDataForStock(symbol, rawData); });

            QObject::connect(stream,
                             &Stream::streamClosed,
                             this,
                             [this, symbol](Stream::StreamError reason, QString message)
                             { handleStreamError(symbol, reason, message); });

            m_streamBars[symbol] = stream;
        }
        else
        {
            QPointer<StreamMarketDepthQuote> stream =
                TSClient::getInstance()->openStreamMarketDepthQuote(symbol,
                                                                    10); // depth 10
            OBJ_ASSUME_TRUE(stream != nullptr);


            QObject::connect(stream,
                             &StreamMarketDepthQuote::receivedNewRawData,
                             this,
                             [this, symbol = stream->getSymbol()](const QByteArray& rawData)
                             { onReceivedNewRawDataForStock(symbol, rawData); });

            QObject::connect(stream,
                             &Stream::streamClosed,
                             this,
                             [this, symbol](Stream::StreamError reason, QString message)
                             { handleStreamError(symbol, reason, message); });

            m_streamMarketDepthQuotes[symbol] = stream;
        }
    }
}

void LiveStreamDB::onReceivedNewRawDataForStock(QString symbol, const QByteArray& rawData)
{
    qint64 epochMs = QDateTime::currentMSecsSinceEpoch();

    QString dataType = (streamType == StreamType::Bars) ? "bar" : "market depth quote";
    DEBUG << "Received new" << dataType << "raw JSON data for" << symbol << "at timestamp" << epochMs;

    // Check if this symbol had an unrecovered timeout and mark it as recovered
    if (unrecoveredTimeouts.contains(symbol))
    {
        unrecoveredTimeouts.remove(symbol);
        recoveredTimeouts[symbol]++;
        qInfo() << "Stream recovered from timeout for" << symbol;
    }

    storeData(symbol, epochMs, rawData);
}

void LiveStreamDB::handleStreamError(const QString& symbol, Stream::StreamError reason, const QString& message)
{
    if (reason == Stream::StreamError::Closed)
    {
        return;
    }

    m_streamErrorCounters[symbol][reason]++;
    WARNING << "Stream error for" << symbol << ":" << message;

    if (reason == Stream::StreamError::Timeout)
    {
        unrecoveredTimeouts.insert(symbol);
    }

    attemptStreamRecovery(symbol);
}

void LiveStreamDB::finalizeUnrecoveredTimeouts()
{
    for (const QString& symbol: unrecoveredTimeouts)
    {
        unrecoveredTimeoutCounts[symbol]++;
    }
    unrecoveredTimeouts.clear();
}

void LiveStreamDB::attemptStreamRecovery(const QString& symbol)
{
    recoveryAttempts[symbol]++;

    qInfo() << "Attempting to recover stream for" << symbol << "(attempt #" << recoveryAttempts[symbol] << ")";

    if (streamType == StreamType::Bars)
    {
        // Close existing stream
        if (m_streamBars.contains(symbol))
        {
            QPointer<StreamBars> oldStream = m_streamBars[symbol];
            if (oldStream)
            {
                TSClient::getInstance()->closeStream(oldStream);
            }
            m_streamBars.remove(symbol);
        }

        // Open new stream
        QPointer<StreamBars> stream = TSClient::getInstance()->openStreamBars(symbol,
                                                                              1,
                                                                              Bar::BarUnit::Minute,
                                                                              0,
                                                                              Bar::BarSessionTemplate::USEQ24Hour);
        OBJ_ASSUME_TRUE(stream != nullptr);

        QObject::connect(stream,
                         &StreamBars::receivedNewRawData,
                         this,
                         [this, symbol](const QByteArray& rawData) { onReceivedNewRawDataForStock(symbol, rawData); });


        QObject::connect(stream,
                         &Stream::streamClosed,
                         this,
                         [this, symbol](Stream::StreamError reason, QString message)
                         { handleStreamError(symbol, reason, message); });

        m_streamBars[symbol] = stream;
        successfulRecoveries[symbol]++;
        qInfo() << "Successfully recovered bars stream for" << symbol;
    }
    else
    {
        // Close existing stream
        if (m_streamMarketDepthQuotes.contains(symbol))
        {
            StreamMarketDepthQuote* oldStream = m_streamMarketDepthQuotes[symbol];
            if (oldStream)
            {
                TSClient::getInstance()->closeStream(oldStream);
            }
            m_streamMarketDepthQuotes.remove(symbol);
        }

        // Open new stream
        StreamMarketDepthQuote* stream = TSClient::getInstance()->openStreamMarketDepthQuote(symbol, 10);
        Q_CHECK_PTR(stream);

        QObject::connect(stream,
                         &StreamMarketDepthQuote::receivedNewRawData,
                         this,
                         [this, symbol](const QByteArray& rawData) { onReceivedNewRawDataForStock(symbol, rawData); });

        QObject::connect(stream,
                         &Stream::streamClosed,
                         this,
                         [this, symbol](Stream::StreamError reason, QString message)
                         { handleStreamError(symbol, reason, message); });

        m_streamMarketDepthQuotes[symbol] = stream;
        successfulRecoveries[symbol]++;
        INFO << "Successfully recovered market depth stream for" << symbol;
    }
}

int LiveStreamDB::getRecordCount() const
{
    QSqlQuery query(db);
    QString tableName = (streamType == StreamType::Bars) ? "bars" : "market_depth_quotes";
    query.prepare(LiveStreamDBQueries::SELECT_COUNT_FROM_TABLE.arg(tableName));
    if (query.exec() && query.next())
    {
        return query.value(0).toInt();
    }
    return 0;
}

int LiveStreamDB::getActiveStreamCount() const
{
    int activeCount = 0;

    // When an error occurs in a stream, it auto deletes itself, and thanks to QPointer we can detect that here

    if (streamType == StreamType::Bars)
    {
        for (QPointer<StreamBars> stream: m_streamBars)
        {
            if (stream != nullptr)
            {
                activeCount++;
            }
        }
    }
    else
    {
        for (QPointer<StreamMarketDepthQuote> stream: m_streamMarketDepthQuotes)
        {

            if (stream != nullptr)
            {
                activeCount++;
            }
        }
    }

    return activeCount;
}