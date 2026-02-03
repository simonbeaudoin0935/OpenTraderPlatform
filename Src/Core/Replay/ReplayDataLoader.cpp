#include "ReplayDataLoader.h"
#include "CONSTANTS.h"
#include "Assume.h"
#include "Logging.h"
#include "SQL/LiveStreamDBQueries.h"
#include "SQL/ReplayDataQueries.h"
#include "Settings.h"

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

ReplayDataLoader::ReplayDataLoader(QObject* p_parent) : QObject(p_parent)
{
    // Generate unique connection names for this instance
    int instanceId = s_connectionCounter.fetch_add(1);
    m_barsDbConnectionName = QString("ReplayBarsDB_%1").arg(instanceId);
    m_depthDbConnectionName = QString("ReplayDepthDB_%1").arg(instanceId);

    // Initialize buffer pointers
    m_activeBuffer = &m_bufferA;
    m_loadingBuffer = &m_bufferB;

    DEBUG << "ReplayDataLoader created with connection names:" << m_barsDbConnectionName << m_depthDbConnectionName;
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

QString ReplayDataLoader::getBarsDbPath(QDate p_date) const
{
    QString dateStr = p_date.toString("yyyy-MM-dd");
    return getCacheLocation() + "/RecordedData/Bars/RecordedLiveBars_" + dateStr + ".db";
}

QString ReplayDataLoader::getDepthDbPath(QDate p_date) const
{
    QString dateStr = p_date.toString("yyyy-MM-dd");
    return getCacheLocation() + "/RecordedData/MarketDepthQuotes/RecordedLiveMarketDepthQuotes_" + dateStr + ".db";
}

bool ReplayDataLoader::openDatabases(QDate p_date)
{
    QString barsPath = getBarsDbPath(p_date);
    QString depthPath = getDepthDbPath(p_date);

    // Check if files exist
    bool barsExists = QFile::exists(barsPath);
    bool depthExists = QFile::exists(depthPath);

    if (!barsExists && !depthExists)
    {
        WARNING << "No replay databases found for" << p_date.toString(Qt::ISODate);
        WARNING << "  Bars path:" << barsPath;
        WARNING << "  Depth path:" << depthPath;
        return false;
    }

    // Open bars database if exists
    if (barsExists)
    {
        m_barsDb = QSqlDatabase::addDatabase("QSQLITE", m_barsDbConnectionName);
        m_barsDb.setDatabaseName(barsPath);
        if (!m_barsDb.open())
        {
            CRITICAL << "Failed to open bars replay database:" << m_barsDb.lastError().text();
            return false;
        }
        INFO << "Opened bars replay database:" << barsPath;

        // Ensure indexes exist for efficient queries
        ensureIndexes(m_barsDb, true);
    }
    else
    {
        DEBUG << "No bars database for" << p_date.toString(Qt::ISODate);
    }

    // Open depth database if exists
    if (depthExists)
    {
        m_depthDb = QSqlDatabase::addDatabase("QSQLITE", m_depthDbConnectionName);
        m_depthDb.setDatabaseName(depthPath);
        if (!m_depthDb.open())
        {
            CRITICAL << "Failed to open depth replay database:" << m_depthDb.lastError().text();
            closeDatabases();
            return false;
        }
        INFO << "Opened depth replay database:" << depthPath;

        // Ensure indexes exist for efficient queries
        ensureIndexes(m_depthDb, false);
    }
    else
    {
        DEBUG << "No market depth database for" << p_date.toString(Qt::ISODate);
    }

    return true;
}

void ReplayDataLoader::closeDatabases()
{
    if (m_barsDb.isOpen())
    {
        m_barsDb.close();
    }
    if (m_depthDb.isOpen())
    {
        m_depthDb.close();
    }

    // Remove connections
    if (QSqlDatabase::contains(m_barsDbConnectionName))
    {
        QSqlDatabase::removeDatabase(m_barsDbConnectionName);
    }
    if (QSqlDatabase::contains(m_depthDbConnectionName))
    {
        QSqlDatabase::removeDatabase(m_depthDbConnectionName);
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

    INFO << "Loading replay database for" << p_date.toString(Qt::ISODate) << "starting at"
         << p_startTime.toString("hh:mm:ss") << "(epoch:" << m_startEpochMs << ")";

    if (!openDatabases(p_date))
    {
        return false;
    }

    // Get total counts for progress tracking
    if (m_barsDb.isOpen())
    {
        QSqlQuery countQuery(m_barsDb);
        countQuery.prepare(ReplayDataQueries::COUNT_BARS_FROM_TIME);
        countQuery.addBindValue(m_startEpochMs);
        if (countQuery.exec() && countQuery.next())
        {
            m_totalBarsCount = countQuery.value(0).toLongLong();
            DEBUG << "Total bars from start time:" << m_totalBarsCount;
        }
    }

    if (m_depthDb.isOpen())
    {
        QSqlQuery countQuery(m_depthDb);
        countQuery.prepare(ReplayDataQueries::COUNT_MARKET_DEPTH_FROM_TIME);
        countQuery.addBindValue(m_startEpochMs);
        if (countQuery.exec() && countQuery.next())
        {
            m_totalDepthCount = countQuery.value(0).toLongLong();
            DEBUG << "Total depth quotes from start time:" << m_totalDepthCount;
        }
    }

    INFO << "Total records to replay:" << (m_totalBarsCount + m_totalDepthCount) << "(bars:" << m_totalBarsCount
         << ", depth:" << m_totalDepthCount << ")";

    if (m_totalBarsCount == 0 && m_totalDepthCount == 0)
    {
        WARNING << "No data found from" << p_startTime.toString("hh:mm:ss") << "onwards";
        closeDatabases();
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

    closeDatabases();

    m_bufferA.clear();
    m_bufferB.clear();
    m_activeBuffer = &m_bufferA;
    m_loadingBuffer = &m_bufferB;
    m_activeBufferIndex = 0;

    m_nextBarsId = 1;
    m_nextDepthId = 1;
    m_startEpochMs = 0;
    m_isPrefetching = false;
    m_endOfBarsReached = false;
    m_endOfDepthReached = false;
    m_loadingBufferReady = false;

    m_totalBarsCount = 0;
    m_totalDepthCount = 0;
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
    if (m_activeBuffer == nullptr || m_activeBufferIndex >= m_activeBuffer->size())
    {
        return s_emptyDataPoint;
    }

    return (*m_activeBuffer)[m_activeBufferIndex];
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
    // Query database for the actual last timestamp
    qint64 lastEpoch = 0;

    if (m_barsDb.isOpen())
    {
        QSqlQuery query(m_barsDb);
        if (query.exec(ReplayDataQueries::SELECT_LAST_BAR_TIMESTAMP) && query.next())
        {
            lastEpoch = qMax(lastEpoch, query.value(0).toLongLong());
        }
    }

    if (m_depthDb.isOpen())
    {
        QSqlQuery query(m_depthDb);
        if (query.exec(ReplayDataQueries::SELECT_LAST_DEPTH_TIMESTAMP) && query.next())
        {
            lastEpoch = qMax(lastEpoch, query.value(0).toLongLong());
        }
    }

    if (lastEpoch == 0)
    {
        return QDateTime();
    }

    return QDateTime::fromMSecsSinceEpoch(lastEpoch, TradingHours::MARKET_TIMEZONE);
}

QStringList ReplayDataLoader::getAvailableStocks() const
{
    QSet<QString> stocks;

    if (m_barsDb.isOpen())
    {
        QSqlQuery query(m_barsDb);
        if (query.exec(ReplayDataQueries::SELECT_AVAILABLE_STOCKS_BARS))
        {
            while (query.next())
            {
                stocks.insert(query.value(0).toString());
            }
        }
    }

    if (m_depthDb.isOpen())
    {
        QSqlQuery query(m_depthDb);
        if (query.exec(ReplayDataQueries::SELECT_AVAILABLE_STOCKS_DEPTH))
        {
            while (query.next())
            {
                stocks.insert(query.value(0).toString());
            }
        }
    }

    return stocks.values();
}

qint64 ReplayDataLoader::getTotalRecordCount() const
{
    return m_totalBarsCount + m_totalDepthCount;
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
    p_buffer->reserve(BUFFER_SIZE * 2); // Reserve for both bars and depth

    QVector<ReplayDataPoint> barsChunk;
    QVector<ReplayDataPoint> depthChunk;

    // Load bars chunk
    if (m_barsDb.isOpen() && !m_endOfBarsReached)
    {
        QSqlQuery query(m_barsDb);

        if (p_isInitial)
        {
            query.prepare(ReplayDataQueries::SELECT_BARS_FROM_TIME);
            query.addBindValue(m_startEpochMs);
        }
        else
        {
            query.prepare(ReplayDataQueries::SELECT_BARS_CHUNK);
            query.addBindValue(m_nextBarsId);
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
                point.jsonRawData = query.value(3).toByteArray();
                point.type = ReplayDataPoint::Type::Bar;
                barsChunk.append(point);

                // Track next ID for cursor
                m_nextBarsId = point.id + 1;
            }

            if (barsChunk.size() < BUFFER_SIZE)
            {
                m_endOfBarsReached = true;
                DEBUG << "End of bars data reached";
            }
        }
        else
        {
            WARNING << "Failed to query bars:" << query.lastError().text();
        }
    }

    // Load depth chunk
    if (m_depthDb.isOpen() && !m_endOfDepthReached)
    {
        QSqlQuery query(m_depthDb);

        if (p_isInitial)
        {
            query.prepare(ReplayDataQueries::SELECT_MARKET_DEPTH_FROM_TIME);
            query.addBindValue(m_startEpochMs);
        }
        else
        {
            query.prepare(ReplayDataQueries::SELECT_MARKET_DEPTH_CHUNK);
            query.addBindValue(m_nextDepthId);
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
                point.jsonRawData = query.value(3).toByteArray();
                point.type = ReplayDataPoint::Type::MarketDepthQuote;
                depthChunk.append(point);

                // Track next ID for cursor
                m_nextDepthId = point.id + 1;
            }

            if (depthChunk.size() < BUFFER_SIZE)
            {
                m_endOfDepthReached = true;
                DEBUG << "End of depth data reached";
            }
        }
        else
        {
            WARNING << "Failed to query depth:" << query.lastError().text();
        }
    }

    // Merge chronologically
    mergeChronologically(barsChunk, depthChunk, p_buffer);

    DEBUG << "Loaded buffer chunk with" << p_buffer->size() << "records (bars:" << barsChunk.size()
          << ", depth:" << depthChunk.size() << ")";
}

void ReplayDataLoader::mergeChronologically(QVector<ReplayDataPoint>& p_bars,
                                            QVector<ReplayDataPoint>& p_depth,
                                            QVector<ReplayDataPoint>* p_output)
{
    // Simple merge: combine and sort by timestamp
    p_output->reserve(p_bars.size() + p_depth.size());

    for (const auto& bar: p_bars)
    {
        p_output->append(bar);
    }
    for (const auto& depth: p_depth)
    {
        p_output->append(depth);
    }

    // Sort by timestamp (stable to preserve order of same-timestamp records)
    std::stable_sort(p_output->begin(), p_output->end());
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
    if (m_endOfBarsReached && m_endOfDepthReached)
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

void ReplayDataLoader::ensureIndexes(QSqlDatabase& p_db, bool p_isBarsDb)
{
    QSqlQuery query(p_db);

    if (p_isBarsDb)
    {
        if (!query.exec(LiveStreamDBQueries::CREATE_BARS_EPOCH_INDEX))
        {
            WARNING << "Failed to create bars epoch index:" << query.lastError().text();
        }
        if (!query.exec(LiveStreamDBQueries::CREATE_BARS_TICKER_INDEX))
        {
            WARNING << "Failed to create bars ticker index:" << query.lastError().text();
        }
        DEBUG << "Ensured bars database indexes exist";
    }
    else
    {
        if (!query.exec(LiveStreamDBQueries::CREATE_DEPTH_EPOCH_INDEX))
        {
            WARNING << "Failed to create depth epoch index:" << query.lastError().text();
        }
        if (!query.exec(LiveStreamDBQueries::CREATE_DEPTH_TICKER_INDEX))
        {
            WARNING << "Failed to create depth ticker index:" << query.lastError().text();
        }
        DEBUG << "Ensured depth database indexes exist";
    }
}
