#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

#include "LiveStreamDB.h"
#include "SQL/LiveStreamDBQueries.h"
#include "TSClient.h"
#include "StreamQuote.h"
#include "Logging.h"
#include "Assume.h"
#include "CONSTANTS.h"

#define LOGGING_CATEGORY LiveStreamDBLog
Q_LOGGING_CATEGORY(LiveStreamDBLog, "LiveStreamDB");

LiveStreamDB::LiveStreamDB(StreamType type, const QString& dbPath, QStringList& p_stockTickers)
    : streamType(type), stockTickers(p_stockTickers)
{
    QString connectionName;
    QString tableQuery;
    QString dbTypeStr;

    switch (type)
    {
    case StreamType::Bars:
        connectionName = "LiveBarsDB";
        tableQuery = LiveStreamDBQueries::CREATE_BARS_TABLE;
        dbTypeStr = "bars";
        break;
    case StreamType::MarketDepthQuotes:
        connectionName = "LiveMarketDepthQuoteDB";
        tableQuery = LiveStreamDBQueries::CREATE_MARKET_DEPTH_QUOTES_TABLE;
        dbTypeStr = "market depth quotes";
        break;
    case StreamType::Quotes:
        connectionName = "LiveQuotesDB";
        tableQuery = LiveStreamDBQueries::CREATE_QUOTES_TABLE;
        dbTypeStr = "quotes";
        break;
    }

    setObjectName("LiveStreamDB::" + connectionName);

    db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open())
    {
        qFatal("Failed to open live %s database: %s", qPrintable(dbTypeStr), qPrintable(db.lastError().text()));
    }

    QSqlQuery query(db);
    query.exec(tableQuery);
    if (query.lastError().isValid())
    {
        WARNING << "Failed to create" << dbTypeStr << "table:" << query.lastError().text();
    }

    INFO << "Live" << dbTypeStr << "database opened at" << dbPath;

    // Setup ramp-up timer (single-shot mode, we'll restart it for each symbol)
    // Note: Quotes stream doesn't use ramp-up (single stream for all symbols)
    m_rampTimer.setSingleShot(true);
    connect(&m_rampTimer, &QTimer::timeout, this, &LiveStreamDB::openNextStream);
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

bool LiveStreamDB::storeQuoteData(const QString& stock,
                                  qint64 timestamp,
                                  const QString& objectType,
                                  const QByteArray& rawData)
{
    QSqlQuery query(db);
    query.prepare(LiveStreamDBQueries::INSERT_QUOTE);
    query.addBindValue(stock);
    query.addBindValue(timestamp);
    query.addBindValue(objectType);
    query.addBindValue(rawData);
    if (!query.exec())
    {
        WARNING << "Failed to store quote for" << stock << ":" << query.lastError().text();
        Q_UNREACHABLE();
        return false;
    }
    return true;
}

void LiveStreamDB::processQuoteRawData(const QByteArray& rawData)
{
    // Accumulate data into buffer
    m_quoteAccumulatorBuffer.append(rawData);

    // Process complete JSON objects (newline-delimited)
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();

    while (true)
    {
        int newlinePos = m_quoteAccumulatorBuffer.indexOf('\n');
        if (newlinePos == -1)
        {
            // No complete JSON object yet
            break;
        }

        // Extract complete JSON line
        QByteArray jsonLine = m_quoteAccumulatorBuffer.left(newlinePos);
        m_quoteAccumulatorBuffer.remove(0, newlinePos + 1);

        if (jsonLine.isEmpty())
        {
            continue;
        }

        // Parse JSON to determine object type and symbol
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonLine, &parseError);

        if (parseError.error != QJsonParseError::NoError)
        {
            WARNING << "Failed to parse quote JSON:" << parseError.errorString();
            continue;
        }

        QJsonObject obj = doc.object();
        QString objectType;
        QString stockTicker;

        if (obj.contains("Symbol"))
        {
            objectType = "QuoteStream";
            stockTicker = obj.value("Symbol").toString();
        }
        else if (obj.contains("Heartbeat"))
        {
            objectType = "Heartbeat";
            stockTicker = QString(); // Empty for heartbeat
        }
        else if (obj.contains("Error") || obj.contains("Message"))
        {
            objectType = "Error";
            stockTicker = QString(); // Empty for error
        }
        else
        {
            WARNING << "Unknown quote stream object type:" << jsonLine.left(100);
            objectType = "Unknown";
            stockTicker = QString();
        }

        storeQuoteData(stockTicker, timestamp, objectType, jsonLine);
    }
}

void LiveStreamDB::startRecording()
{
    if (stockTickers.isEmpty())
    {
        WARNING << "No stock tickers to record";
        return;
    }

    // Quotes use a single stream for all symbols - no ramp-up needed
    if (streamType == StreamType::Quotes)
    {
        INFO << "Starting quote recording for" << stockTickers.size() << "symbols (single stream)";

        m_streamQuote = TSClient::getInstance()->openStreamQuote(stockTickers);
        OBJ_ASSUME_DIFF(m_streamQuote.data(), nullptr);

        QObject::connect(m_streamQuote,
                         &StreamQuote::receivedNewRawData,
                         this,
                         [this](const QByteArray& rawData) { processQuoteRawData(rawData); });

        QObject::connect(m_streamQuote,
                         &Stream::streamClosed,
                         this,
                         [this](Stream::StreamError reason, QString message)
                         { handleStreamError("ALL_QUOTES", reason, message); });

        INFO << "Quote stream opened for" << stockTickers.size() << "symbols";
        return;
    }

    // Bars and MarketDepthQuotes use per-symbol streams with ramp-up
    QString dataType = (streamType == StreamType::Bars) ? "bars" : "market depth quotes";
    INFO << "Starting recording for" << stockTickers.size() << dataType << "streams with graduated ramp-up delay";
    INFO << "Ramp-up strategy: 0-100=" << RecorderConstants::STREAM_RAMP_UP_DELAY_TIER1_MS << "ms, "
         << "101-200=" << RecorderConstants::STREAM_RAMP_UP_DELAY_TIER2_MS << "ms, "
         << "201+=" << RecorderConstants::STREAM_RAMP_UP_DELAY_TIER3_MS << "ms";

    // Start ramping: open first stream immediately, then schedule the rest
    m_currentRampIndex = 0;
    openNextStream(); // Open first one immediately
}

void LiveStreamDB::openNextStream()
{
    // Check if ramp-up is complete
    if (m_currentRampIndex < 0 || m_currentRampIndex >= stockTickers.size())
    {
        if (m_currentRampIndex >= stockTickers.size())
        {
            QString dataType = (streamType == StreamType::Bars) ? "bars" : "market depth quotes";
            INFO << "Ramp-up complete - all" << stockTickers.size() << dataType << "streams opened";
            m_currentRampIndex = -1; // Mark ramping as complete
        }
        return;
    }

    const QString& symbol = stockTickers[m_currentRampIndex];

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
                         [this, symbol](const QByteArray& rawData) { onReceivedNewRawDataForStock(symbol, rawData); });

        QObject::connect(stream,
                         &Stream::streamClosed,
                         this,
                         [this, symbol](Stream::StreamError reason, QString message)
                         { handleStreamError(symbol, reason, message); });

        m_streamBars[symbol] = stream;

        DEBUG << "Opened bar stream for" << symbol << "(" << (m_currentRampIndex + 1) << "/" << stockTickers.size()
              << ")";
    }
    else
    {
        // Open market depth stream - check availability first
        // For recorder, we don't queue - if limit is reached, skip this symbol
        if (!StreamMarketDepthQuote::canOpenStream())
        {
            WARNING << "Market depth stream limit reached (" << StreamMarketDepthQuote::getNumberOfMarketDepthStreams()
                    << "/" << MarketDepthConstants::MAX_CONCURRENT_STREAMS << ") - skipping" << symbol;
        }
        else
        {
            auto result = TSClient::getInstance()->openStreamMarketDepthQuote(symbol, 10); // depth 10

            if (result.has_value())
            {
                // Stream opened immediately (expected path)
                QPointer<StreamMarketDepthQuote> stream = result.value();
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

                DEBUG << "Opened market depth stream for" << symbol << "(" << (m_currentRampIndex + 1) << "/"
                      << stockTickers.size() << ")";
            }
            else
            {
                // Should not happen - we checked canOpenStream() above
                CRITICAL
                    << "Unexpected: openStreamMarketDepthQuote returned queued future despite canOpenStream() check"
                    << "- skipping" << symbol;
            }
        }
    }

    // Move to next symbol
    m_currentRampIndex++;

    // Schedule opening the next stream if more remain
    if (m_currentRampIndex < stockTickers.size())
    {
        // Calculate adaptive delay based on number of streams already opened
        int delay;
        if (m_currentRampIndex <= RecorderConstants::STREAM_RAMP_UP_TIER1_THRESHOLD)
        {
            delay = RecorderConstants::STREAM_RAMP_UP_DELAY_TIER1_MS;
        }
        else if (m_currentRampIndex <= RecorderConstants::STREAM_RAMP_UP_TIER2_THRESHOLD)
        {
            delay = RecorderConstants::STREAM_RAMP_UP_DELAY_TIER2_MS;
            // Log when transitioning to slower tier
            if (m_currentRampIndex == RecorderConstants::STREAM_RAMP_UP_TIER1_THRESHOLD + 1)
            {
                INFO << "Reached" << RecorderConstants::STREAM_RAMP_UP_TIER1_THRESHOLD << "streams - slowing down to"
                     << delay << "ms delay";
            }
        }
        else
        {
            delay = RecorderConstants::STREAM_RAMP_UP_DELAY_TIER3_MS;
            // Log when transitioning to slowest tier
            if (m_currentRampIndex == RecorderConstants::STREAM_RAMP_UP_TIER2_THRESHOLD + 1)
            {
                INFO << "Reached" << RecorderConstants::STREAM_RAMP_UP_TIER2_THRESHOLD << "streams - slowing down to"
                     << delay << "ms delay";
            }
        }

        m_rampTimer.start(delay);
    }
    else
    {
        // Ramping complete
        QString dataType = (streamType == StreamType::Bars) ? "bars" : "market depth quotes";
        INFO << "Ramp-up complete - all" << stockTickers.size() << dataType << "streams opened";
        m_currentRampIndex = -1;
    }
}

void LiveStreamDB::stopRecording()
{
    // Stop ramp-up timer if still running
    if (m_rampTimer.isActive())
    {
        m_rampTimer.stop();
        INFO << "Stopped ramp-up timer (was at index" << m_currentRampIndex << "of" << stockTickers.size() << ")";
        m_currentRampIndex = -1;
    }

    if (streamType == StreamType::Bars)
    {
        INFO << "Stopping recording for bars";
        for (auto it = m_streamBars.begin(); it != m_streamBars.end(); ++it)
        {
            const QString& symbol = it.key();
            QPointer<StreamBars> stream = it.value();
            if (stream)
            {
                DEBUG << "Closing bar stream for" << symbol;
                TSClient::getInstance()->closeStream(stream);
            }
        }
        m_streamBars.clear();
        INFO << "Stopped recording for bars";
    }
    else if (streamType == StreamType::MarketDepthQuotes)
    {
        INFO << "Stopping recording for market depth quotes";
        for (auto it = m_streamMarketDepthQuotes.begin(); it != m_streamMarketDepthQuotes.end(); ++it)
        {
            const QString& symbol = it.key();
            QPointer<StreamMarketDepthQuote> stream = it.value();
            if (stream)
            {
                DEBUG << "Closing market depth stream for" << symbol;
                TSClient::getInstance()->closeStream(stream);
            }
        }
        m_streamMarketDepthQuotes.clear();
        INFO << "Stopped recording for market depth quotes";
    }
    else if (streamType == StreamType::Quotes)
    {
        INFO << "Stopping recording for quotes";
        if (m_streamQuote)
        {
            DEBUG << "Closing quote stream for" << stockTickers.size() << "symbols";
            TSClient::getInstance()->closeStream(m_streamQuote);
            m_streamQuote = nullptr;
        }
        m_quoteAccumulatorBuffer.clear();
        INFO << "Stopped recording for quotes";
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

        // Open new stream - check availability first
        // For recorder, we don't queue - if limit is reached, we'll retry later
        if (!StreamMarketDepthQuote::canOpenStream())
        {
            WARNING << "Market depth stream limit reached during recovery for" << symbol << "("
                    << StreamMarketDepthQuote::getNumberOfMarketDepthStreams() << "/"
                    << MarketDepthConstants::MAX_CONCURRENT_STREAMS << ") - will retry later";
            // Don't mark as recovered - will retry on next handleStreamError call
            return;
        }

        auto result = TSClient::getInstance()->openStreamMarketDepthQuote(symbol, 10);

        if (result.has_value())
        {
            // Stream opened immediately (expected path)
            StreamMarketDepthQuote* stream = result.value();
            Q_CHECK_PTR(stream);

            QObject::connect(stream,
                             &StreamMarketDepthQuote::receivedNewRawData,
                             this,
                             [this, symbol](const QByteArray& rawData)
                             { onReceivedNewRawDataForStock(symbol, rawData); });

            QObject::connect(stream,
                             &Stream::streamClosed,
                             this,
                             [this, symbol](Stream::StreamError reason, QString message)
                             { handleStreamError(symbol, reason, message); });

            m_streamMarketDepthQuotes[symbol] = stream;
            successfulRecoveries[symbol]++;
            INFO << "Successfully recovered market depth stream for" << symbol;
        }
        else
        {
            // Should not happen - we checked canOpenStream() above
            CRITICAL << "Unexpected: openStreamMarketDepthQuote returned queued future despite canOpenStream() check"
                     << "- will retry recovery for" << symbol;
        }
    }
}

int LiveStreamDB::getRecordCount() const
{
    QSqlQuery query(db);
    QString tableName;
    switch (streamType)
    {
    case StreamType::Bars:
        tableName = "bars";
        break;
    case StreamType::MarketDepthQuotes:
        tableName = "market_depth_quotes";
        break;
    case StreamType::Quotes:
        tableName = "quotes";
        break;
    }
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
    else if (streamType == StreamType::MarketDepthQuotes)
    {
        for (QPointer<StreamMarketDepthQuote> stream: m_streamMarketDepthQuotes)
        {

            if (stream != nullptr)
            {
                activeCount++;
            }
        }
    }
    else if (streamType == StreamType::Quotes)
    {
        // Single stream for all symbols
        if (m_streamQuote != nullptr)
        {
            activeCount = 1;
        }
    }

    return activeCount;
}