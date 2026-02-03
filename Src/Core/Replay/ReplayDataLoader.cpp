#include "ReplayDataLoader.h"
#include "Logging.h"
#include "Assume.h"
#include "CONSTANTS.h"

#define LOGGING_CATEGORY ReplayDataLoaderLog

Q_LOGGING_CATEGORY(ReplayDataLoaderLog, "ReplayDataLoader")

// Static empty data point for peekNextDataPoint when no data available
static const ReplayDataLoader::ReplayDataPoint s_emptyDataPoint{};

ReplayDataLoader::ReplayDataLoader(QObject* p_parent) : QObject(p_parent)
{
    DEBUG << "ReplayDataLoader created";
}

ReplayDataLoader::~ReplayDataLoader()
{
    reset();
    DEBUG << "ReplayDataLoader destroyed";
}

bool ReplayDataLoader::loadDatabase(QDate p_date, QTime p_startTime)
{
    reset();

    m_loadedDate = p_date;
    m_startTime = p_startTime;

    INFO << "Loading replay database for" << p_date.toString(Qt::ISODate) << "starting at"
         << p_startTime.toString("hh:mm:ss");

    // TODO: Phase 1.2 - Implement actual database loading
    // For now, return false indicating no data loaded
    // This will be replaced with:
    // 1. Find database files for the date
    // 2. Open both bars and market_depth databases
    // 3. Query data from startTime forward
    // 4. Merge into single chronological buffer

    WARNING << "ReplayDataLoader::loadDatabase - not yet implemented (Phase 1.2)";

    return loadInitialBuffer();
}

void ReplayDataLoader::reset()
{
    m_buffer.clear();
    m_currentIndex = 0;
    m_isPrefetching = false;
    m_loadedDate = QDate();
    m_startTime = QTime();

    DEBUG << "ReplayDataLoader reset";
}

bool ReplayDataLoader::hasMoreData() const
{
    return m_currentIndex < m_buffer.size();
}

bool ReplayDataLoader::isPrefetching() const
{
    return m_isPrefetching;
}

ReplayDataLoader::ReplayDataPoint ReplayDataLoader::getNextDataPoint()
{
    ASSUME_TRUE(hasMoreData());

    ReplayDataPoint point = m_buffer[m_currentIndex];
    ++m_currentIndex;

    // Check if we need to prefetch next chunk
    // TODO: Phase 1.2 - Implement ping-pong buffer logic
    // When current buffer is ~80% consumed, trigger prefetch

    return point;
}

const ReplayDataLoader::ReplayDataPoint& ReplayDataLoader::peekNextDataPoint() const
{
    if (!hasMoreData())
    {
        WARNING << "peekNextDataPoint called with no data available";
        return s_emptyDataPoint;
    }

    return m_buffer[m_currentIndex];
}

QDateTime ReplayDataLoader::getFirstTimestamp() const
{
    if (m_buffer.isEmpty())
    {
        return QDateTime();
    }

    return QDateTime::fromMSecsSinceEpoch(m_buffer.first().epochMs, TradingHours::MARKET_TIMEZONE);
}

QDateTime ReplayDataLoader::getLastTimestamp() const
{
    if (m_buffer.isEmpty())
    {
        return QDateTime();
    }

    return QDateTime::fromMSecsSinceEpoch(m_buffer.last().epochMs, TradingHours::MARKET_TIMEZONE);
}

QStringList ReplayDataLoader::getAvailableStocks() const
{
    QSet<QString> stocks;
    for (const auto& point: m_buffer)
    {
        stocks.insert(point.stockTicker);
    }
    return stocks.values();
}

bool ReplayDataLoader::loadInitialBuffer()
{
    // TODO: Phase 1.2 - Implement actual database loading
    // This is a stub that returns false (no data)

    DEBUG << "loadInitialBuffer - stub implementation, no data loaded";
    return false;
}

void ReplayDataLoader::triggerPrefetch()
{
    if (m_isPrefetching)
    {
        return;
    }

    m_isPrefetching = true;

    // TODO: Phase 1.2 - Implement async prefetch with QtConcurrent
    // QtConcurrent::run([this]() {
    //     // Load next chunk from database
    //     // Merge with existing data
    //     // Emit bufferReady when done
    // });

    DEBUG << "triggerPrefetch - stub implementation";
}
