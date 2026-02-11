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

    m_barTimer.setSingleShot(true);
    m_depthTimer.setSingleShot(true);

    bool connected = connect(&m_barTimer, &QTimer::timeout, this, &ReplayEngine::onBarTimerTick, Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    connected = connect(&m_depthTimer, &QTimer::timeout, this, &ReplayEngine::onDepthTimerTick, Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    DEBUG << "ReplayEngine created";
}

ReplayEngine::~ReplayEngine()
{
    if (m_state != PlaybackState::Stopped)
    {
        stopReplay();
    }

    cleanupLoaders();

    DEBUG << "ReplayEngine destroyed";
}

bool ReplayEngine::initLoaders(QDate p_date, QTime p_startTime)
{
    // Create bar loader
    if (m_barLoader == nullptr)
    {
        m_barLoader = new ReplayDataLoader(ReplayDataLoader::DataType::Bar, this);
        bool connected = connect(m_barLoader,
                                 &ReplayDataLoader::bufferReady,
                                 this,
                                 &ReplayEngine::onBarBufferReady,
                                 Qt::UniqueConnection);
        ASSUME_TRUE(connected);
    }

    // Create depth loader
    if (m_depthLoader == nullptr)
    {
        m_depthLoader = new ReplayDataLoader(ReplayDataLoader::DataType::MarketDepthQuote, this);
        bool connected = connect(m_depthLoader,
                                 &ReplayDataLoader::bufferReady,
                                 this,
                                 &ReplayEngine::onDepthBufferReady,
                                 Qt::UniqueConnection);
        ASSUME_TRUE(connected);
    }

    // Load databases — either or both may succeed
    bool barsLoaded = m_barLoader->loadDatabase(p_date, p_startTime);
    bool depthLoaded = m_depthLoader->loadDatabase(p_date, p_startTime);

    if (!barsLoaded)
    {
        DEBUG << "No bar data available for replay";
        m_barStreamEnded = true;
    }
    if (!depthLoaded)
    {
        DEBUG << "No depth data available for replay";
        m_depthStreamEnded = true;
    }

    if (!barsLoaded && !depthLoaded)
    {
        CRITICAL << "Failed to load any replay database for" << p_date.toString(Qt::ISODate);
        return false;
    }

    return true;
}

void ReplayEngine::cleanupLoaders()
{
    delete m_barLoader;
    m_barLoader = nullptr;

    delete m_depthLoader;
    m_depthLoader = nullptr;
}

void ReplayEngine::startReplay(QDate p_date, QTime p_startTime, PlaybackSpeed p_speed)
{
    if (m_state != PlaybackState::Stopped)
    {
        WARNING << "Cannot start replay - already active. Stop current replay first.";
        return;
    }

    m_barTimer.stop();
    m_depthTimer.stop();

    INFO << "Starting replay for" << p_date.toString(Qt::ISODate) << "at" << p_startTime.toString("hh:mm:ss")
         << "speed:" << static_cast<int>(p_speed);

    m_speed = p_speed;
    m_barStreamEnded = false;
    m_depthStreamEnded = false;
    m_isPausedBeforePlay = false; // No longer in paused-before-play state

    if (!initLoaders(p_date, p_startTime))
    {
        return;
    }

    // Find earliest first data point across both streams
    qint64 initialEpoch = 0;
    if (m_barLoader->hasMoreData())
    {
        initialEpoch = m_barLoader->peekNextDataPoint().epochMs;
    }
    if (m_depthLoader->hasMoreData())
    {
        qint64 depthEpoch = m_depthLoader->peekNextDataPoint().epochMs;
        if (initialEpoch == 0 || (depthEpoch > 0 && depthEpoch < initialEpoch))
        {
            initialEpoch = depthEpoch;
        }
    }

    if (initialEpoch > 0)
    {
        QDateTime initialTime = QDateTime::fromMSecsSinceEpoch(initialEpoch, TradingHours::MARKET_TIMEZONE);
        MainApp::currentAppReplayTime = initialTime;
        INFO << "Initialized replay time to:" << initialTime.toString("yyyy-MM-dd hh:mm:ss.zzz");
    }

    // Set wall-clock anchor: this replay-epoch maps to "now"
    m_replayEpochAnchorMs = initialEpoch;
    m_wallClockAnchorMs = QDateTime::currentMSecsSinceEpoch();

    m_state = PlaybackState::Playing;
    emit replayStarted();

    // Emit first data point from each stream and schedule next
    if (!m_barStreamEnded)
    {
        emitNextBar();
        scheduleNextBar();
    }
    if (!m_depthStreamEnded)
    {
        emitNextDepth();
        scheduleNextDepth();
    }
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
    m_barStreamEnded = false;
    m_depthStreamEnded = false;
    m_isPausedBeforePlay = true; // Set flag: paused before first play

    if (!initLoaders(p_date, p_startTime))
    {
        return;
    }

    // Find earliest first data point across both streams
    qint64 initialEpoch = 0;
    if (m_barLoader->hasMoreData())
    {
        initialEpoch = m_barLoader->peekNextDataPoint().epochMs;
    }
    if (m_depthLoader->hasMoreData())
    {
        qint64 depthEpoch = m_depthLoader->peekNextDataPoint().epochMs;
        if (initialEpoch == 0 || (depthEpoch > 0 && depthEpoch < initialEpoch))
        {
            initialEpoch = depthEpoch;
        }
    }

    if (initialEpoch > 0)
    {
        QDateTime initialTime = QDateTime::fromMSecsSinceEpoch(initialEpoch, TradingHours::MARKET_TIMEZONE);
        MainApp::currentAppReplayTime = initialTime;
        INFO << "Initialized replay time to:" << initialTime.toString("yyyy-MM-dd hh:mm:ss.zzz");
    }

    // Wall-clock anchor will be set on resume; for now just record the replay start epoch
    m_replayEpochAnchorMs = initialEpoch;
    m_wallClockAnchorMs = 0; // Will be set when user resumes

    // Emit first data points then immediately pause
    m_state = PlaybackState::Playing;
    emit replayStarted();

    if (!m_barStreamEnded)
    {
        emitNextBar();
    }
    if (!m_depthStreamEnded)
    {
        emitNextDepth();
    }

    // Immediately pause instead of scheduling next data points
    m_state = PlaybackState::Paused;
    emit replayPaused();

    INFO << "Replay started in paused state after first data points";
}

void ReplayEngine::stopReplay()
{
    if (m_state == PlaybackState::Stopped)
    {
        DEBUG << "stopReplay called but already stopped";
        return;
    }

    INFO << "Stopping replay";

    m_barTimer.stop();
    m_depthTimer.stop();
    m_state = PlaybackState::Stopped;
    m_wallClockAnchorMs = 0;
    m_replayEpochAnchorMs = 0;
    m_pauseWallClockMs = 0;
    m_barStreamEnded = false;
    m_depthStreamEnded = false;
    m_isPausedBeforePlay = false; // Reset flag when stopping

    // Clean up data loaders
    if (m_barLoader != nullptr)
    {
        m_barLoader->reset();
    }
    if (m_depthLoader != nullptr)
    {
        m_depthLoader->reset();
    }

    emit replayStopped();
}

void ReplayEngine::setSpeed(PlaybackSpeed p_speed)
{
    if (m_speed == p_speed)
    {
        return;
    }

    PlaybackSpeed oldSpeed = m_speed;
    m_speed = p_speed;

    INFO << "Speed changed from" << static_cast<int>(oldSpeed) << "to" << static_cast<int>(p_speed);

    // Re-anchor wall clock so the new speed applies from this instant
    if (m_state == PlaybackState::Playing)
    {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        qint64 currentReplayMs = MainApp::currentAppReplayTime.isValid()
                                     ? MainApp::currentAppReplayTime.toMSecsSinceEpoch()
                                     : m_replayEpochAnchorMs;

        m_wallClockAnchorMs = now;
        m_replayEpochAnchorMs = currentReplayMs;

        // Reschedule both timers with the new anchor/speed
        m_barTimer.stop();
        m_depthTimer.stop();

        if (!m_barStreamEnded)
        {
            scheduleNextBar();
        }
        if (!m_depthStreamEnded)
        {
            scheduleNextDepth();
        }
    }
}

void ReplayEngine::pauseReplay()
{
    if (m_state != PlaybackState::Playing)
    {
        WARNING << "Cannot pause - not currently playing";
        return;
    }

    DEBUG << "Pausing replay";

    m_barTimer.stop();
    m_depthTimer.stop();
    m_pauseWallClockMs = QDateTime::currentMSecsSinceEpoch();
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

    // Shift wall-clock anchor forward by the pause duration so timing stays correct
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_wallClockAnchorMs == 0)
    {
        // First resume after startReplayPaused — anchor to now
        m_wallClockAnchorMs = now;
    }
    else if (m_pauseWallClockMs > 0)
    {
        m_wallClockAnchorMs += (now - m_pauseWallClockMs);
    }
    m_pauseWallClockMs = 0;

    m_state = PlaybackState::Playing;

    // Resume both streams from where they left off
    if (!m_barStreamEnded)
    {
        scheduleNextBar();
    }
    if (!m_depthStreamEnded)
    {
        scheduleNextDepth();
    }

    emit replayResumed();
}

void ReplayEngine::onBarTimerTick()
{
    if (m_state != PlaybackState::Playing)
    {
        return;
    }

    emitNextBar();
    scheduleNextBar();
}

void ReplayEngine::onDepthTimerTick()
{
    if (m_state != PlaybackState::Playing)
    {
        return;
    }

    emitNextDepth();
    scheduleNextDepth();
}

void ReplayEngine::onBarBufferReady()
{
    DEBUG << "Bar prefetch buffer ready";

    if (m_state == PlaybackState::Playing && !m_barTimer.isActive() && !m_barStreamEnded)
    {
        scheduleNextBar();
    }
}

void ReplayEngine::onDepthBufferReady()
{
    DEBUG << "Depth prefetch buffer ready";

    if (m_state == PlaybackState::Playing && !m_depthTimer.isActive() && !m_depthStreamEnded)
    {
        scheduleNextDepth();
    }
}

void ReplayEngine::emitNextBar()
{
    // Loop until we find a bar for a stock we have a stream for
    while (m_barLoader != nullptr && m_barLoader->hasMoreData())
    {
        ReplayDataLoader::ReplayDataPoint dataPoint = m_barLoader->getNextDataPoint();

        if (!hasStreamForStock(dataPoint.stockTicker, true))
        {
            continue;
        }

        updateReplayTime(dataPoint.epochMs);

        auto dataPtr = std::make_shared<const QByteArray>(dataPoint.jsonRawData);
        emit injectBarData(dataPoint.stockTicker, dataPtr);

        QDateTime newTime = QDateTime::fromMSecsSinceEpoch(dataPoint.epochMs, TradingHours::MARKET_TIMEZONE);
        DEBUG << "Emitted bar for" << dataPoint.stockTicker << "at" << newTime.toString("hh:mm:ss.zzz");
        return;
    }
}

void ReplayEngine::emitNextDepth()
{
    // Loop until we find a depth quote for a stock we have a stream for
    while (m_depthLoader != nullptr && m_depthLoader->hasMoreData())
    {
        ReplayDataLoader::ReplayDataPoint dataPoint = m_depthLoader->getNextDataPoint();

        if (!hasStreamForStock(dataPoint.stockTicker, false))
        {
            continue;
        }

        updateReplayTime(dataPoint.epochMs);

        auto dataPtr = std::make_shared<const QByteArray>(dataPoint.jsonRawData);
        emit injectDepthData(dataPoint.stockTicker, dataPtr);

        QDateTime newTime = QDateTime::fromMSecsSinceEpoch(dataPoint.epochMs, TradingHours::MARKET_TIMEZONE);
        DEBUG << "Emitted depth for" << dataPoint.stockTicker << "at" << newTime.toString("hh:mm:ss.zzz");
        return;
    }
}

void ReplayEngine::scheduleNextBar()
{
    if (m_barLoader == nullptr)
    {
        return;
    }

    if (!m_barLoader->hasMoreData())
    {
        if (m_barLoader->isPrefetching())
        {
            DEBUG << "Bar stream: waiting for buffer prefetch...";
            return; // onBarBufferReady will reschedule
        }

        INFO << "Bar stream reached end of data";
        m_barStreamEnded = true;
        checkAllStreamsEnded();
        return;
    }

    const ReplayDataLoader::ReplayDataPoint& nextPoint = m_barLoader->peekNextDataPoint();
    qint64 delay = calculateWallClockDelay(nextPoint.epochMs);
    m_barTimer.start(static_cast<int>(delay));
}

void ReplayEngine::scheduleNextDepth()
{
    if (m_depthLoader == nullptr)
    {
        return;
    }

    if (!m_depthLoader->hasMoreData())
    {
        if (m_depthLoader->isPrefetching())
        {
            DEBUG << "Depth stream: waiting for buffer prefetch...";
            return; // onDepthBufferReady will reschedule
        }

        INFO << "Depth stream reached end of data";
        m_depthStreamEnded = true;
        checkAllStreamsEnded();
        return;
    }

    const ReplayDataLoader::ReplayDataPoint& nextPoint = m_depthLoader->peekNextDataPoint();
    qint64 delay = calculateWallClockDelay(nextPoint.epochMs);
    m_depthTimer.start(static_cast<int>(delay));
}

qint64 ReplayEngine::calculateWallClockDelay(qint64 p_replayEpochMs) const
{
    if (m_speed == PlaybackSpeed::AsFastAsPossible)
    {
        return 0;
    }

    int speedValue = static_cast<int>(m_speed);
    if (speedValue <= 0)
    {
        return 0;
    }

    // How far into the replay this data point is (in replay-time ms)
    qint64 replayOffsetMs = p_replayEpochMs - m_replayEpochAnchorMs;

    // Scale by speed: at 2x (200%), wall-clock offset is half the replay offset
    // Formula: wallClockOffset = replayOffset * 100 / speed
    qint64 wallClockOffsetMs = (replayOffsetMs * 100) / speedValue;

    // Target wall-clock time for this data point
    qint64 targetWallMs = m_wallClockAnchorMs + wallClockOffsetMs;

    // Delay = how long from now until target
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 delay = targetWallMs - now;

    // Clamp: if we're behind schedule, fire immediately
    if (delay < 0)
    {
        delay = 0;
    }
    else if (delay > 60000) // Cap at 1 minute
    {
        delay = 60000;
    }

    return delay;
}

void ReplayEngine::updateReplayTime(qint64 p_epochMs)
{
    // Only advance time forward — never backward
    QDateTime currentTime = MainApp::currentAppReplayTime;
    qint64 currentMs = currentTime.isValid() ? currentTime.toMSecsSinceEpoch() : 0;

    if (p_epochMs > currentMs)
    {
        QDateTime newTime = QDateTime::fromMSecsSinceEpoch(p_epochMs, TradingHours::MARKET_TIMEZONE);
        MainApp::currentAppReplayTime = newTime;
        emit replayTimeUpdated(newTime);
    }
}

void ReplayEngine::checkAllStreamsEnded()
{
    if (m_barStreamEnded && m_depthStreamEnded)
    {
        INFO << "All replay streams reached end of data";
        m_state = PlaybackState::Stopped;
        emit replayEndReached();
        emit replayStopped();
    }
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
