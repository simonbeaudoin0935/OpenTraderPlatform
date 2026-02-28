#include "ReplayDataLoader.h"
#include "CONSTANTS.h"
#include "Assume.h"
#include "Logging.h"
#include "SQL/ReplayDataQueries.h"
#include "Settings.h"

// Legacy TS recorded data index queries (will be removed when replay moves to DBClient .dbn files)
namespace LegacyReplayQueries
{
    const QString CREATE_BARS_EPOCH_INDEX = "CREATE INDEX IF NOT EXISTS idx_bars_epochMs ON bars(epochMs)";
    const QString CREATE_BARS_TICKER_INDEX = "CREATE INDEX IF NOT EXISTS idx_bars_stockTicker ON bars(stockTicker)";
    const QString CREATE_DEPTH_EPOCH_INDEX =
        "CREATE INDEX IF NOT EXISTS idx_market_depth_epochMs ON market_depth_quotes(epochMs)";
    const QString CREATE_DEPTH_TICKER_INDEX =
        "CREATE INDEX IF NOT EXISTS idx_market_depth_stockTicker ON market_depth_quotes(stockTicker)";
    const QString CREATE_QUOTES_EPOCH_INDEX = "CREATE INDEX IF NOT EXISTS idx_quotes_epochMs ON quotes(epochMs)";
    const QString CREATE_QUOTES_TICKER_INDEX =
        "CREATE INDEX IF NOT EXISTS idx_quotes_stockTicker ON quotes(stockTicker)";
} // namespace LegacyReplayQueries

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QtConcurrent>
#include <algorithm>

#define LOGGING_CATEGORY ReplayDataLoaderLog

Q_LOGGING_CATEGORY(ReplayDataLoaderLog, "ReplayDataLoader")

// Static empty data point for peekNextDataPoint when no data available
static const ReplayDataLoader::ReplayDataPoint s_emptyDataPoint{};

// Atomic counter for unique database connection names
static std::atomic<int> s_connectionCounter{0};

ReplayDataLoader::ReplayDataLoader(DataType p_dataType, QObject* p_parent) : QObject(p_parent), m_dataType(p_dataType)
{
    // Generate unique connection name for this instance
    int instanceId = s_connectionCounter.fetch_add(1);
    QString typeStr;
    switch (m_dataType)
    {
    case DataType::Bar:
        typeStr = "Bars";
        break;
    case DataType::MarketDepthQuote:
        typeStr = "Depth";
        break;
    case DataType::Quote:
        typeStr = "Quotes";
        break;
    }
    m_dbConnectionName = QString("Replay%1DB_%2").arg(typeStr).arg(instanceId);

    // Initialize buffer pointers
    m_activeBuffer = &m_bufferA;
    m_loadingBuffer = &m_bufferB;

    DEBUG << "ReplayDataLoader created for" << typeStr << "with connection:" << m_dbConnectionName;
}

ReplayDataLoader::~ReplayDataLoader()
{
    // Wait for any pending prefetch
    if (m_prefetchFuture.isRunning())
    {
        DEBUG << "Waiting for prefetch to complete...";
        m_prefetchFuture.waitForFinished();
    }

    reset();
    DEBUG << "ReplayDataLoader destroyed";
}

QString ReplayDataLoader::getDbPath(QDate p_date) const
{
    QString dateStr = p_date.toString("yyyy-MM-dd");
    switch (m_dataType)
    {
    case DataType::Bar:
        return getCacheLocation() + "/RecordedLiveData/Bars/" + dateStr + ".db";
    case DataType::MarketDepthQuote:
        return getCacheLocation() + "/RecordedLiveData/MarketDepthQuotes/" + dateStr + ".db";
    case DataType::Quote:
        return getCacheLocation() + "/RecordedLiveData/Quotes/" + dateStr + ".db";
    }
    Q_UNREACHABLE();
}

bool ReplayDataLoader::openDatabase(QDate p_date)
{
    QString dbPath = getDbPath(p_date);

    if (!QFile::exists(dbPath))
    {
        QString typeStr = (m_dataType == DataType::Bar)                ? "bars"
                          : (m_dataType == DataType::MarketDepthQuote) ? "depth"
                                                                       : "quotes";
        WARNING << "No" << typeStr << "replay database found for" << p_date.toString(Qt::ISODate);
        WARNING << "  Path:" << dbPath;
        return false;
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_dbConnectionName);
    m_db.setDatabaseName(dbPath);
    if (!m_db.open())
    {
        CRITICAL << "Failed to open replay database:" << m_db.lastError().text();
        return false;
    }
    INFO << "Opened replay database:" << dbPath;

    ensureIndexes();
    return true;
}

void ReplayDataLoader::closeDatabase()
{
    if (m_db.isOpen())
    {
        m_db.close();
    }

    // Clear the member variable before removing connection
    m_db = QSqlDatabase();

    if (QSqlDatabase::contains(m_dbConnectionName))
    {
        QSqlDatabase::removeDatabase(m_dbConnectionName);
    }
}

bool ReplayDataLoader::loadDatabase(QDate p_date, QTime p_startTime)
{
    reset();

    m_loadedDate = p_date;
    m_startTime = p_startTime;

    // Convert start time to epoch milliseconds
    QDateTime startDateTime(p_date, p_startTime, TradingHours::MARKET_TIMEZONE);
    m_startEpochMs = startDateTime.toMSecsSinceEpoch();

    QString typeStr = (m_dataType == DataType::Bar)                ? "bars"
                      : (m_dataType == DataType::MarketDepthQuote) ? "depth"
                                                                   : "quotes";
    INFO << "Loading" << typeStr << "replay database for" << p_date.toString(Qt::ISODate) << "starting at"
         << p_startTime.toString("hh:mm:ss") << "(epoch:" << m_startEpochMs << ")";

    if (!openDatabase(p_date))
    {
        return false;
    }

    // Get total count for progress tracking
    QSqlQuery countQuery(m_db);
    QString countSql;
    switch (m_dataType)
    {
    case DataType::Bar:
        countSql = ReplayDataQueries::COUNT_BARS_FROM_TIME;
        break;
    case DataType::MarketDepthQuote:
        countSql = ReplayDataQueries::COUNT_MARKET_DEPTH_FROM_TIME;
        break;
    case DataType::Quote:
        countSql = ReplayDataQueries::COUNT_QUOTES_FROM_TIME;
        break;
    }
    countQuery.prepare(countSql);
    countQuery.addBindValue(m_startEpochMs);
    if (countQuery.exec() && countQuery.next())
    {
        m_totalCount = countQuery.value(0).toLongLong();
        DEBUG << "Total" << typeStr << "from start time:" << m_totalCount;
    }

    INFO << "Total" << typeStr << "records to replay:" << m_totalCount;

    if (m_totalCount == 0)
    {
        WARNING << "No" << typeStr << "data found from" << p_startTime.toString("hh:mm:ss") << "onwards";
        closeDatabase();
        return false;
    }

    return loadInitialBuffer();
}

void ReplayDataLoader::reset()
{
    // Wait for prefetch if running
    if (m_prefetchFuture.isRunning())
    {
        m_prefetchFuture.waitForFinished();
    }

    closeDatabase();

    m_bufferA.clear();
    m_bufferB.clear();
    m_activeBuffer = &m_bufferA;
    m_loadingBuffer = &m_bufferB;
    m_activeBufferIndex = 0;

    m_nextId = 1;
    m_startEpochMs = 0;
    m_isPrefetching = false;
    m_endOfDataReached = false;
    m_loadingBufferReady = false;

    m_totalCount = 0;
    m_recordsConsumed = 0;

    m_loadedDate = QDate();
    m_startTime = QTime();

    DEBUG << "ReplayDataLoader reset complete";
}

bool ReplayDataLoader::hasMoreData() const
{
    if (m_activeBuffer == nullptr)
    {
        return false;
    }

    // Data available in active buffer
    if (m_activeBufferIndex < m_activeBuffer->size())
    {
        return true;
    }

    // Check if loading buffer is ready and has data
    if (m_loadingBufferReady && !m_loadingBuffer->isEmpty())
    {
        return true;
    }

    return false;
}

bool ReplayDataLoader::isPrefetching() const
{
    return m_isPrefetching;
}

ReplayDataLoader::ReplayDataPoint ReplayDataLoader::getNextDataPoint()
{
    // Check if we need to swap buffers
    if (m_activeBufferIndex >= m_activeBuffer->size())
    {
        if (m_loadingBufferReady && !m_loadingBuffer->isEmpty())
        {
            swapBuffers();
        }
        else
        {
            // No more data
            emit dataExhausted();
            return s_emptyDataPoint;
        }
    }

    ASSUME_TRUE(m_activeBufferIndex < m_activeBuffer->size());

    ReplayDataPoint point = (*m_activeBuffer)[m_activeBufferIndex];
    ++m_activeBufferIndex;
    ++m_recordsConsumed;

    // Check if we should trigger prefetch
    if (shouldPrefetch() && !m_isPrefetching && !m_loadingBufferReady)
    {
        triggerPrefetch();
    }

    return point;
}

const ReplayDataLoader::ReplayDataPoint& ReplayDataLoader::peekNextDataPoint() const
{
    if (m_activeBuffer == nullptr)
    {
        return s_emptyDataPoint;
    }

    if (m_activeBufferIndex < m_activeBuffer->size())
    {
        return (*m_activeBuffer)[m_activeBufferIndex];
    }

    // Active buffer exhausted but loading buffer is ready — peek into it
    if (m_loadingBufferReady && m_loadingBuffer != nullptr && !m_loadingBuffer->isEmpty())
    {
        return m_loadingBuffer->first();
    }

    return s_emptyDataPoint;
}

QDateTime ReplayDataLoader::getFirstTimestamp() const
{
    if (m_activeBuffer == nullptr || m_activeBuffer->isEmpty())
    {
        return QDateTime();
    }

    return QDateTime::fromMSecsSinceEpoch(m_activeBuffer->first().epochMs, TradingHours::MARKET_TIMEZONE);
}

QDateTime ReplayDataLoader::getLastTimestamp() const
{
    if (!m_db.isOpen())
    {
        return QDateTime();
    }

    QSqlQuery query(m_db);
    QString sql;
    switch (m_dataType)
    {
    case DataType::Bar:
        sql = ReplayDataQueries::SELECT_LAST_BAR_TIMESTAMP;
        break;
    case DataType::MarketDepthQuote:
        sql = ReplayDataQueries::SELECT_LAST_DEPTH_TIMESTAMP;
        break;
    case DataType::Quote:
        sql = ReplayDataQueries::SELECT_LAST_QUOTE_TIMESTAMP;
        break;
    }
    if (query.exec(sql) && query.next())
    {
        qint64 lastEpoch = query.value(0).toLongLong();
        if (lastEpoch > 0)
        {
            return QDateTime::fromMSecsSinceEpoch(lastEpoch, TradingHours::MARKET_TIMEZONE);
        }
    }

    return QDateTime();
}

QStringList ReplayDataLoader::getAvailableStocks() const
{
    QStringList stocks;

    if (!m_db.isOpen())
    {
        return stocks;
    }

    QSqlQuery query(m_db);
    QString sql;
    switch (m_dataType)
    {
    case DataType::Bar:
        sql = ReplayDataQueries::SELECT_AVAILABLE_STOCKS_BARS;
        break;
    case DataType::MarketDepthQuote:
        sql = ReplayDataQueries::SELECT_AVAILABLE_STOCKS_DEPTH;
        break;
    case DataType::Quote:
        sql = ReplayDataQueries::SELECT_AVAILABLE_STOCKS_QUOTES;
        break;
    }
    if (query.exec(sql))
    {
        while (query.next())
        {
            stocks.append(query.value(0).toString());
        }
    }

    return stocks;
}

qint64 ReplayDataLoader::getTotalRecordCount() const
{
    return m_totalCount;
}

qint64 ReplayDataLoader::getCurrentPosition() const
{
    return m_recordsConsumed;
}

bool ReplayDataLoader::loadInitialBuffer()
{
    loadBufferChunk(m_activeBuffer, true);

    if (m_activeBuffer->isEmpty())
    {
        WARNING << "No data loaded in initial buffer";
        return false;
    }

    INFO << "Loaded initial buffer with" << m_activeBuffer->size() << "records";
    INFO << "First record at" << getFirstTimestamp().toString("hh:mm:ss.zzz");

    return true;
}

void ReplayDataLoader::loadBufferChunk(QVector<ReplayDataPoint>* p_buffer, bool p_isInitial)
{
    p_buffer->clear();
    p_buffer->reserve(BUFFER_SIZE);

    if (!m_db.isOpen() || m_endOfDataReached)
    {
        return;
    }

    QSqlQuery query(m_db);

    if (p_isInitial)
    {
        QString sql;
        switch (m_dataType)
        {
        case DataType::Bar:
            sql = ReplayDataQueries::SELECT_BARS_FROM_TIME;
            break;
        case DataType::MarketDepthQuote:
            sql = ReplayDataQueries::SELECT_MARKET_DEPTH_FROM_TIME;
            break;
        case DataType::Quote:
            sql = ReplayDataQueries::SELECT_QUOTES_FROM_TIME;
            break;
        }
        query.prepare(sql);
        query.addBindValue(m_startEpochMs);
    }
    else
    {
        QString sql;
        switch (m_dataType)
        {
        case DataType::Bar:
            sql = ReplayDataQueries::SELECT_BARS_CHUNK;
            break;
        case DataType::MarketDepthQuote:
            sql = ReplayDataQueries::SELECT_MARKET_DEPTH_CHUNK;
            break;
        case DataType::Quote:
            sql = ReplayDataQueries::SELECT_QUOTES_CHUNK;
            break;
        }
        query.prepare(sql);
        query.addBindValue(m_nextId);
    }
    query.addBindValue(BUFFER_SIZE);

    if (query.exec())
    {
        while (query.next())
        {
            ReplayDataPoint point;
            point.id = query.value(0).toLongLong();
            point.stockTicker = query.value(1).toString();
            point.epochMs = query.value(2).toLongLong();

            // All data types now have objectType in column 3, jsonRawData in column 4
            point.objectType = query.value(3).toString();
            point.jsonRawData = query.value(4).toByteArray();
            p_buffer->append(point);

            // Track next ID for cursor
            m_nextId = point.id + 1;
        }

        if (p_buffer->size() < BUFFER_SIZE)
        {
            m_endOfDataReached = true;
            DEBUG << "End of data reached";
        }
    }
    else
    {
        WARNING << "Failed to query data:" << query.lastError().text();
    }

    DEBUG << "Loaded buffer chunk with" << p_buffer->size() << "records";
}

bool ReplayDataLoader::shouldPrefetch() const
{
    if (m_activeBuffer == nullptr || m_activeBuffer->isEmpty())
    {
        return false;
    }

    // Already have data ready in loading buffer
    if (m_loadingBufferReady)
    {
        return false;
    }

    // Already reached end of all data
    if (m_endOfDataReached)
    {
        return false;
    }

    // Trigger prefetch when we've consumed PREFETCH_THRESHOLD_PERCENT of active buffer
    int threshold = (m_activeBuffer->size() * PREFETCH_THRESHOLD_PERCENT) / 100;
    return m_activeBufferIndex >= threshold;
}

void ReplayDataLoader::triggerPrefetch()
{
    if (m_isPrefetching)
    {
        return;
    }

    m_isPrefetching = true;
    DEBUG << "Starting async prefetch...";

    // Run prefetch in thread pool
    m_prefetchFuture = QtConcurrent::run(
        [this]()
        {
            loadBufferChunk(m_loadingBuffer, false);
            onPrefetchComplete();
        });
}

void ReplayDataLoader::onPrefetchComplete()
{
    m_isPrefetching = false;
    m_loadingBufferReady = !m_loadingBuffer->isEmpty();

    DEBUG << "Prefetch complete, loading buffer ready:" << m_loadingBufferReady
          << "records:" << m_loadingBuffer->size();

    // Emit signal (will be queued to main thread)
    QMetaObject::invokeMethod(this, [this]() { emit bufferReady(); }, Qt::QueuedConnection);
}

void ReplayDataLoader::swapBuffers()
{
    QMutexLocker locker(&m_bufferMutex);

    std::swap(m_activeBuffer, m_loadingBuffer);
    m_activeBufferIndex = 0;
    m_loadingBufferReady = false;
    m_loadingBuffer->clear();

    DEBUG << "Buffers swapped, new active buffer size:" << m_activeBuffer->size();
}

void ReplayDataLoader::ensureIndexes()
{
    QSqlQuery query(m_db);

    switch (m_dataType)
    {
    case DataType::Bar:
        if (!query.exec(LegacyReplayQueries::CREATE_BARS_EPOCH_INDEX))
        {
            WARNING << "Failed to create bars epoch index:" << query.lastError().text();
        }
        if (!query.exec(LegacyReplayQueries::CREATE_BARS_TICKER_INDEX))
        {
            WARNING << "Failed to create bars ticker index:" << query.lastError().text();
        }
        DEBUG << "Ensured bars database indexes exist";
        break;

    case DataType::MarketDepthQuote:
        if (!query.exec(LegacyReplayQueries::CREATE_DEPTH_EPOCH_INDEX))
        {
            WARNING << "Failed to create depth epoch index:" << query.lastError().text();
        }
        if (!query.exec(LegacyReplayQueries::CREATE_DEPTH_TICKER_INDEX))
        {
            WARNING << "Failed to create depth ticker index:" << query.lastError().text();
        }
        DEBUG << "Ensured depth database indexes exist";
        break;

    case DataType::Quote:
        if (!query.exec(LegacyReplayQueries::CREATE_QUOTES_EPOCH_INDEX))
        {
            WARNING << "Failed to create quotes epoch index:" << query.lastError().text();
        }
        if (!query.exec(LegacyReplayQueries::CREATE_QUOTES_TICKER_INDEX))
        {
            WARNING << "Failed to create quotes ticker index:" << query.lastError().text();
        }
        DEBUG << "Ensured quotes database indexes exist";
        break;
    }
}
