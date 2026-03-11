#include "ReplayEngine.h"
#include "DBClient.h"
#include "DBRecordTranslator.h"
#include "MainApp.h"
#include "Logging.h"
#include "Assume.h"
#include "CONSTANTS.h"

#include <QFile>
#include <chrono>
#include <memory>
#include <databento/dbn_file_store.hpp>

#define LOGGING_CATEGORY ReplayEngineLog

Q_LOGGING_CATEGORY(ReplayEngineLog, "ReplayEngine")

ReplayEngine::ReplayEngine(QObject* p_parent) : QObject(p_parent)
{
    m_timer.setSingleShot(true);

    bool connected = connect(&m_timer, &QTimer::timeout, this, &ReplayEngine::onTimerTick, Qt::UniqueConnection);
    ASSUME_TRUE(connected);

    DEBUG << "ReplayEngine created";
}

ReplayEngine::~ReplayEngine()
{
    if (m_state != PlaybackState::Stopped)
    {
        stopReplay();
    }
    DEBUG << "ReplayEngine destroyed";
}

// ---------------------------------------------------------------------------
// Streaming helpers
// ---------------------------------------------------------------------------

static qint64 toEpochMs(databento::UnixNanos ts)
{
    return static_cast<qint64>(std::chrono::duration_cast<std::chrono::milliseconds>(ts.time_since_epoch()).count());
}

void ReplayEngine::advanceMbp10()
{
    m_nextMbp10.valid = false;
    if (m_mbp10Store == nullptr)
        return;

    const databento::Record* record = nullptr;
    while ((record = m_mbp10Store->NextRecord()) != nullptr)
    {
        if (record->Holds<databento::Mbp10Msg>())
        {
            const auto& msg = record->Get<databento::Mbp10Msg>();
            const qint64 epochMs = toEpochMs(msg.hd.ts_event);
            if (epochMs >= m_startEpochMs)
            {
                m_nextMbp10 = {epochMs, DBRecordTranslator::toLevel2(m_currentSymbol, msg), true};
                return;
            }
        }
    }
    // EOF — close store to free memory
    m_mbp10Store.reset();
}

void ReplayEngine::advanceTrade()
{
    m_nextTrade.valid = false;
    if (m_tradesStore == nullptr)
        return;

    const databento::Record* record = nullptr;
    while ((record = m_tradesStore->NextRecord()) != nullptr)
    {
        if (record->Holds<databento::TradeMsg>())
        {
            const auto& msg = record->Get<databento::TradeMsg>();
            const qint64 epochMs = toEpochMs(msg.hd.ts_event);
            if (epochMs >= m_startEpochMs)
            {
                m_nextTrade = {epochMs, DBRecordTranslator::toTrade(m_currentSymbol, msg), true};
                return;
            }
        }
    }
    m_tradesStore.reset();
}

bool ReplayEngine::openReplayStreams(const QString& p_symbol, QDate p_date, QTime p_startTime)
{
    closeStreams();

    m_currentSymbol = p_symbol;
    m_startEpochMs = QDateTime(p_date, p_startTime, TradingHours::MARKET_TIMEZONE).toMSecsSinceEpoch();

    const QString mbp10Path = DBClient::getReplayFilePath(p_date, p_symbol, "mbp10");
    const QString tradesPath = DBClient::getReplayFilePath(p_date, p_symbol, "trades");

    if (QFile::exists(mbp10Path))
    {
        try
        {
            m_mbp10Store = std::make_unique<databento::DbnFileStore>(std::filesystem::path(mbp10Path.toStdString()));
            advanceMbp10(); // Prime lookahead
            INFO << "Opened Mbp10 stream:" << mbp10Path;
        }
        catch (const std::exception& ex)
        {
            WARNING << "Failed to open Mbp10 file:" << ex.what();
            m_mbp10Store.reset();
        }
    }
    else
    {
        DEBUG << "No Mbp10 file found:" << mbp10Path;
    }

    if (QFile::exists(tradesPath))
    {
        try
        {
            m_tradesStore = std::make_unique<databento::DbnFileStore>(std::filesystem::path(tradesPath.toStdString()));
            advanceTrade(); // Prime lookahead
            INFO << "Opened Trades stream:" << tradesPath;
        }
        catch (const std::exception& ex)
        {
            WARNING << "Failed to open Trades file:" << ex.what();
            m_tradesStore.reset();
        }
    }
    else
    {
        DEBUG << "No Trades file found:" << tradesPath;
    }

    return m_nextMbp10.valid || m_nextTrade.valid;
}

void ReplayEngine::closeStreams()
{
    m_mbp10Store.reset();
    m_tradesStore.reset();
    m_nextMbp10.valid = false;
    m_nextTrade.valid = false;
}

// ---------------------------------------------------------------------------
// Playback control
// ---------------------------------------------------------------------------

void ReplayEngine::startReplay(const QString& p_symbol, QDate p_date, QTime p_startTime, PlaybackSpeed p_speed)
{
    if (m_state != PlaybackState::Stopped)
    {
        WARNING << "Cannot start replay - already active";
        return;
    }

    m_timer.stop();

    INFO << "Starting replay for" << p_symbol << "on" << p_date.toString(Qt::ISODate) << "at"
         << p_startTime.toString("hh:mm:ss") << "speed:" << static_cast<int>(p_speed);

    m_speed = p_speed;

    if (!openReplayStreams(p_symbol, p_date, p_startTime))
    {
        CRITICAL << "Failed to load replay data";
        emit replayDataLoadFailed(
            QString("No replay data found for %1 on %2").arg(p_symbol, p_date.toString(Qt::ISODate)));
        return;
    }

    // Use the earliest available record timestamp as anchor
    qint64 initialEpoch = m_startEpochMs;
    if (m_nextMbp10.valid)
        initialEpoch = m_nextMbp10.epochMs;
    if (m_nextTrade.valid && m_nextTrade.epochMs < initialEpoch)
        initialEpoch = m_nextTrade.epochMs;

    QDateTime initialTime = QDateTime::fromMSecsSinceEpoch(initialEpoch, TradingHours::MARKET_TIMEZONE);
    MainApp::currentAppReplayTime = initialTime;
    INFO << "Initialized replay time to:" << initialTime.toString("yyyy-MM-dd hh:mm:ss.zzz");

    m_replayEpochAnchorMs = initialEpoch;
    m_wallClockAnchorMs = QDateTime::currentMSecsSinceEpoch();

    m_state = PlaybackState::Playing;
    emit replayStarted();

    emitNextRecord();
    scheduleNext();
}

void ReplayEngine::startReplayPaused(const QString& p_symbol, QDate p_date, QTime p_startTime, PlaybackSpeed p_speed)
{
    if (m_state != PlaybackState::Stopped)
    {
        WARNING << "Cannot start replay - already active";
        return;
    }

    INFO << "Starting replay (paused) for" << p_symbol << "on" << p_date.toString(Qt::ISODate) << "at"
         << p_startTime.toString("hh:mm:ss");

    m_speed = p_speed;

    if (!openReplayStreams(p_symbol, p_date, p_startTime))
    {
        CRITICAL << "Failed to load replay data";
        emit replayDataLoadFailed(
            QString("No replay data found for %1 on %2").arg(p_symbol, p_date.toString(Qt::ISODate)));
        return;
    }

    qint64 initialEpoch = m_startEpochMs;
    if (m_nextMbp10.valid)
        initialEpoch = m_nextMbp10.epochMs;
    if (m_nextTrade.valid && m_nextTrade.epochMs < initialEpoch)
        initialEpoch = m_nextTrade.epochMs;

    QDateTime initialTime = QDateTime::fromMSecsSinceEpoch(initialEpoch, TradingHours::MARKET_TIMEZONE);
    MainApp::currentAppReplayTime = initialTime;

    m_replayEpochAnchorMs = initialEpoch;
    m_wallClockAnchorMs = 0; // Will be set on resume

    // Emit first record then pause
    m_state = PlaybackState::Playing;
    emit replayStarted();

    emitNextRecord();

    m_state = PlaybackState::Paused;
    emit replayPaused();

    INFO << "Replay started in paused state after first record";
}

void ReplayEngine::stopReplay()
{
    if (m_state == PlaybackState::Stopped)
    {
        DEBUG << "stopReplay called but already stopped";
        return;
    }

    INFO << "Stopping replay";

    m_timer.stop();
    m_state = PlaybackState::Stopped;
    m_wallClockAnchorMs = 0;
    m_replayEpochAnchorMs = 0;
    m_pauseWallClockMs = 0;
    clearReplayTime();
    closeStreams();

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

    m_timer.stop();
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

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_wallClockAnchorMs == 0)
    {
        // First resume after startReplayPaused
        m_wallClockAnchorMs = now;
    }
    else if (m_pauseWallClockMs > 0)
    {
        m_wallClockAnchorMs += (now - m_pauseWallClockMs);
    }
    m_pauseWallClockMs = 0;

    m_state = PlaybackState::Playing;

    scheduleNext();

    emit replayResumed();
}

void ReplayEngine::setSpeed(PlaybackSpeed p_speed)
{
    if (m_speed == p_speed)
        return;

    INFO << "Speed changed from" << static_cast<int>(m_speed) << "to" << static_cast<int>(p_speed);
    m_speed = p_speed;

    if (m_state == PlaybackState::Playing)
    {
        // Re-anchor and reschedule
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        qint64 currentReplayMs = MainApp::currentAppReplayTime.isValid()
                                     ? MainApp::currentAppReplayTime.toMSecsSinceEpoch()
                                     : m_replayEpochAnchorMs;
        m_wallClockAnchorMs = now;
        m_replayEpochAnchorMs = currentReplayMs;

        m_timer.stop();
        scheduleNext();
    }
}

void ReplayEngine::onTimerTick()
{
    if (m_state != PlaybackState::Playing)
        return;

    emitNextRecord();
    scheduleNext();
}

void ReplayEngine::emitNextRecord()
{
    // Pick the stream with the earlier next record
    const bool hasMbp10 = m_nextMbp10.valid;
    const bool hasTrade = m_nextTrade.valid;

    if (!hasMbp10 && !hasTrade)
        return;

    bool useMbp10 = hasMbp10 && (!hasTrade || m_nextMbp10.epochMs <= m_nextTrade.epochMs);

    if (useMbp10)
    {
        updateReplayTime(m_nextMbp10.epochMs);
        emit replayLevel2(m_currentSymbol, std::get<Level2>(m_nextMbp10.data));
        advanceMbp10();
    }
    else
    {
        updateReplayTime(m_nextTrade.epochMs);
        emit replayTrade(m_currentSymbol, std::get<Trade>(m_nextTrade.data));
        advanceTrade();
    }
}

void ReplayEngine::scheduleNext()
{
    const bool hasMbp10 = m_nextMbp10.valid;
    const bool hasTrade = m_nextTrade.valid;

    if (!hasMbp10 && !hasTrade)
    {
        INFO << "Replay reached end of data";
        m_state = PlaybackState::Stopped;
        emit replayEndReached();
        emit replayStopped();
        return;
    }

    qint64 nextEpoch = hasMbp10 ? m_nextMbp10.epochMs : m_nextTrade.epochMs;
    if (hasTrade && m_nextTrade.epochMs < nextEpoch)
        nextEpoch = m_nextTrade.epochMs;

    const qint64 delay = calculateWallClockDelay(nextEpoch);
    m_timer.start(static_cast<int>(delay));
}

qint64 ReplayEngine::calculateWallClockDelay(qint64 p_replayEpochMs) const
{
    if (m_speed == PlaybackSpeed::AsFastAsPossible)
        return 0;

    int speedValue = static_cast<int>(m_speed);
    if (speedValue <= 0)
        return 0;

    qint64 replayOffsetMs = p_replayEpochMs - m_replayEpochAnchorMs;
    qint64 wallClockOffsetMs = (replayOffsetMs * 100) / speedValue;
    qint64 targetWallMs = m_wallClockAnchorMs + wallClockOffsetMs;
    qint64 delay = targetWallMs - QDateTime::currentMSecsSinceEpoch();

    return qBound(qint64(0), delay, qint64(60000));
}

void ReplayEngine::updateReplayTime(qint64 p_epochMs)
{
    QDateTime currentTime = MainApp::currentAppReplayTime;
    qint64 currentMs = currentTime.isValid() ? currentTime.toMSecsSinceEpoch() : 0;

    if (p_epochMs > currentMs)
    {
        QDateTime newTime = QDateTime::fromMSecsSinceEpoch(p_epochMs, TradingHours::MARKET_TIMEZONE);
        MainApp::currentAppReplayTime = newTime;
        setCurrentReplayTime(newTime);
        emit replayTimeUpdated(newTime);
    }
}
