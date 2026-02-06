#include "ReplayEngine.h"
#include "ReplayDataLoader.h"
#include "TSClient.h"
#include "MainApp.h"
#include "Logging.h"
#include "Assume.h"

#define LOGGING_CATEGORY ReplayEngineLog

Q_LOGGING_CATEGORY(ReplayEngineLog, "ReplayEngine")

ReplayEngine::ReplayEngine(QObject* p_parent, TSClient* p_tsClient) : QObject(p_parent), m_tsClient(p_tsClient)
{
    ASSUME_TRUE(p_tsClient != nullptr);

    m_playbackTimer.setSingleShot(true);
    bool connected =
        connect(&m_playbackTimer, &QTimer::timeout, this, &ReplayEngine::onTimerTick, Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    DEBUG << "ReplayEngine created";
}

ReplayEngine::~ReplayEngine()
{
    if (m_state != PlaybackState::Stopped)
    {
        stopReplay();
    }

    delete m_dataLoader;
    m_dataLoader = nullptr;

    DEBUG << "ReplayEngine destroyed";
}

void ReplayEngine::startReplay(QDate p_date, QTime p_startTime, PlaybackSpeed p_speed)
{
    if (m_state != PlaybackState::Stopped)
    {
        WARNING << "Cannot start replay - already active. Stop current replay first.";
        return;
    }

    // Ensure any stale timer is stopped
    m_playbackTimer.stop();

    INFO << "Starting replay for" << p_date.toString(Qt::ISODate) << "at" << p_startTime.toString("hh:mm:ss")
         << "speed:" << static_cast<int>(p_speed);

    m_speed = p_speed;

    // Create data loader if needed
    if (m_dataLoader == nullptr)
    {
        m_dataLoader = new ReplayDataLoader(this);
        bool connected = connect(m_dataLoader,
                                 &ReplayDataLoader::bufferReady,
                                 this,
                                 &ReplayEngine::onBufferReady,
                                 Qt::UniqueConnection);
        ASSUME_TRUE(connected);
    }

    // Load database for the selected date, starting from specified time
    if (!m_dataLoader->loadDatabase(p_date, p_startTime))
    {
        CRITICAL << "Failed to load replay database for" << p_date.toString(Qt::ISODate);
        return;
    }

    // Check if we have any data
    if (!m_dataLoader->hasMoreData())
    {
        WARNING << "No data available for replay on" << p_date.toString(Qt::ISODate) << "at" << p_startTime.toString();
        return;
    }

    // Initialize replay time to first data point timestamp
    // This ensures MainApp::getCurrentAppTime() returns valid time immediately
    const ReplayDataLoader::ReplayDataPoint& firstPoint = m_dataLoader->peekNextDataPoint();
    QDateTime initialTime = QDateTime::fromMSecsSinceEpoch(firstPoint.epochMs, TradingHours::MARKET_TIMEZONE);
    MainApp::currentAppReplayTime = initialTime;
    INFO << "Initialized replay time to first data point:" << initialTime.toString("yyyy-MM-dd hh:mm:ss.zzz");

    m_state = PlaybackState::Playing;
    m_lastEmittedTimestampMs = 0;
    ++m_replayGeneration; // Invalidate any stale timer events from previous replay

    emit replayStarted();

    // Emit first data point immediately, then schedule subsequent ones
    emitCurrentDataPoint();
    scheduleNextDataPoint();
}

void ReplayEngine::startReplayPaused(QDate p_date, QTime p_startTime, PlaybackSpeed p_speed)
{
    if (m_state != PlaybackState::Stopped)
    {
        WARNING << "Cannot start replay - already active. Stop current replay first.";
        return;
    }

    INFO << "Starting replay (paused) for" << p_date.toString(Qt::ISODate) << "at" << p_startTime.toString("hh:mm:ss");

    m_speed = p_speed;

    // Create data loader if needed
    if (m_dataLoader == nullptr)
    {
        m_dataLoader = new ReplayDataLoader(this);
        bool connected = connect(m_dataLoader,
                                 &ReplayDataLoader::bufferReady,
                                 this,
                                 &ReplayEngine::onBufferReady,
                                 Qt::UniqueConnection);
        ASSUME_TRUE(connected);
    }

    // Load database for the selected date, starting from specified time
    if (!m_dataLoader->loadDatabase(p_date, p_startTime))
    {
        CRITICAL << "Failed to load replay database for" << p_date.toString(Qt::ISODate);
        return;
    }

    // Check if we have any data
    if (!m_dataLoader->hasMoreData())
    {
        WARNING << "No data available for replay on" << p_date.toString(Qt::ISODate) << "at" << p_startTime.toString();
        return;
    }

    // Initialize replay time to first data point timestamp
    const ReplayDataLoader::ReplayDataPoint& firstPoint = m_dataLoader->peekNextDataPoint();
    QDateTime initialTime = QDateTime::fromMSecsSinceEpoch(firstPoint.epochMs, TradingHours::MARKET_TIMEZONE);
    MainApp::currentAppReplayTime = initialTime;
    INFO << "Initialized replay time to first data point:" << initialTime.toString("yyyy-MM-dd hh:mm:ss.zzz");

    ++m_replayGeneration; // Invalidate any stale timer events from previous replay

    // Emit first data point to trigger chart population
    m_state = PlaybackState::Playing;
    m_lastEmittedTimestampMs = 0;
    emit replayStarted();
    emitCurrentDataPoint();

    // Immediately pause instead of scheduling next data point
    m_state = PlaybackState::Paused;
    emit replayPaused();

    INFO << "Replay started in paused state after first data point";
}

void ReplayEngine::stopReplay()
{
    if (m_state == PlaybackState::Stopped)
    {
        DEBUG << "stopReplay called but already stopped";
        return;
    }

    INFO << "Stopping replay";

    m_playbackTimer.stop();
    m_state = PlaybackState::Stopped;
    m_lastEmittedTimestampMs = 0;

    // Clean up data loader
    if (m_dataLoader != nullptr)
    {
        m_dataLoader->reset();
    }

    emit replayStopped();
}

void ReplayEngine::pauseReplay()
{
    if (m_state != PlaybackState::Playing)
    {
        WARNING << "Cannot pause - not currently playing";
        return;
    }

    DEBUG << "Pausing replay";

    m_playbackTimer.stop();
    m_state = PlaybackState::Paused;

    emit replayPaused();
}

void ReplayEngine::resumeReplay()
{
    if (m_state != PlaybackState::Paused)
    {
        WARNING << "Cannot resume - not currently paused";
        return;
    }

    DEBUG << "Resuming replay";

    m_state = PlaybackState::Playing;

    // Schedule next data point (resume from where we left off)
    scheduleNextDataPoint();

    emit replayResumed();
}

void ReplayEngine::onTimerTick()
{
    DEBUG << "onTimerTick ENTRY";
    if (m_state != PlaybackState::Playing)
    {
        DEBUG << "onTimerTick: not playing, returning";
        return;
    }

    emitCurrentDataPoint();
    scheduleNextDataPoint();
    DEBUG << "onTimerTick EXIT";
}

void ReplayEngine::onBufferReady()
{
    DEBUG << "Prefetch buffer ready";

    // If we were waiting for data and are still playing, continue
    if (m_state == PlaybackState::Playing && !m_playbackTimer.isActive())
    {
        scheduleNextDataPoint();
    }
}

void ReplayEngine::emitCurrentDataPoint()
{
    DEBUG << "emitCurrentDataPoint ENTRY";
    // Loop until we find a data point for a stock we have a stream for
    while (m_dataLoader != nullptr && m_dataLoader->hasMoreData())
    {
        ReplayDataLoader::ReplayDataPoint dataPoint = m_dataLoader->getNextDataPoint();

        // Check if we have an active stream for this stock
        bool isBar = (dataPoint.type == ReplayDataLoader::ReplayDataPoint::Type::Bar);
        if (!hasStreamForStock(dataPoint.stockTicker, isBar))
        {
            // No stream for this stock - skip and try next in same call
            continue;
        }

        // Update replay time (discrete jumps)
        QDateTime newTime = QDateTime::fromMSecsSinceEpoch(dataPoint.epochMs, TradingHours::MARKET_TIMEZONE);
        MainApp::currentAppReplayTime = newTime;
        m_lastEmittedTimestampMs = dataPoint.epochMs;

        emit replayTimeUpdated(newTime);

        // Create shared_ptr to avoid copying data across thread boundary
        auto dataPtr = std::make_shared<const QByteArray>(dataPoint.jsonRawData);

        // Emit signal for cross-thread data injection into TSClient
        if (isBar)
        {
            emit injectBarData(dataPoint.stockTicker, dataPtr);
        }
        else
        {
            emit injectDepthData(dataPoint.stockTicker, dataPtr);
        }

        DEBUG << "Emitted" << (isBar ? "bar" : "depth") << "for" << dataPoint.stockTicker << "at"
              << newTime.toString("hh:mm:ss.zzz");
        return;
    }
}

void ReplayEngine::scheduleNextDataPoint()
{
    if (m_dataLoader == nullptr)
    {
        DEBUG << "scheduleNextDataPoint: m_dataLoader is null";
        return;
    }

    // Check if more data is available
    if (!m_dataLoader->hasMoreData())
    {
        // Check if we're waiting for a buffer prefetch
        if (m_dataLoader->isPrefetching())
        {
            DEBUG << "Waiting for buffer prefetch...";
            return; // onBufferReady will reschedule when data arrives
        }

        INFO << "Replay reached end of data";
        m_state = PlaybackState::Stopped;
        emit replayEndReached();
        emit replayStopped();
        return;
    }

    // Peek at next data point to calculate delay
    const ReplayDataLoader::ReplayDataPoint& nextPoint = m_dataLoader->peekNextDataPoint();

    DEBUG << "scheduleNextDataPoint: next is" << nextPoint.stockTicker << "at epoch" << nextPoint.epochMs;

    qint64 deltaMs = 0;
    if (m_lastEmittedTimestampMs > 0)
    {
        deltaMs = nextPoint.epochMs - m_lastEmittedTimestampMs;

        // Sanity check: negative delta means data is out of order
        if (deltaMs < 0)
        {
            WARNING << "Negative timestamp delta detected:" << deltaMs << "ms - data may be out of order";
            deltaMs = 0;
        }
    }

    qint64 scaledDelay = calculateScaledDelay(deltaMs);

    DEBUG << "scheduleNextDataPoint: deltaMs=" << deltaMs << "scaledDelay=" << scaledDelay;

    m_playbackTimer.start(static_cast<int>(scaledDelay));
}

qint64 ReplayEngine::calculateScaledDelay(qint64 p_deltaMs) const
{
    if (m_speed == PlaybackSpeed::AsFastAsPossible)
    {
        return 0; // 0ms timer gives event loop minimal breathing room
    }

    // Speed is stored as percentage (100 = 1x, 200 = 2x, 50 = 0.5x)
    // For 2x speed, we want half the delay: delta / 2
    // For 0.5x speed, we want double the delay: delta * 2
    // Formula: scaledDelay = delta * 100 / speed

    int speedValue = static_cast<int>(m_speed);
    DEBUG << "calculateScaledDelay: m_speed=" << speedValue << "p_deltaMs=" << p_deltaMs;

    if (speedValue <= 0)
    {
        return 0;
    }

    qint64 scaledDelay = (p_deltaMs * 100) / speedValue;

    // Clamp to reasonable bounds
    if (scaledDelay < 0)
    {
        scaledDelay = 0;
    }
    else if (scaledDelay > 60000) // Cap at 1 minute max delay
    {
        scaledDelay = 60000;
    }

    return scaledDelay;
}

bool ReplayEngine::hasStreamForStock(const QString& p_symbol, bool p_isBar) const
{
    if (p_isBar)
    {
        return m_tsClient->hasOpenBarStream(p_symbol);
    }
    else
    {
        return m_tsClient->hasOpenMarketDepthStream(p_symbol);
    }
}
