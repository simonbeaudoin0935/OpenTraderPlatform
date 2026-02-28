#include "ReplayEngine.h"
#include "DBClient.h"
#include "DBRecordTranslator.h"
#include "MainApp.h"
#include "Logging.h"
#include "Assume.h"
#include "CONSTANTS.h"

#include <QFile>
#include <databento/dbn_file_store.hpp>
#include <algorithm>
#include <chrono>

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

bool ReplayEngine::loadReplayFiles(const QString& p_symbol, QDate p_date, QTime p_startTime)
{
    m_records.clear();
    m_currentIndex = 0;

    const QString mbp10Path = DBClient::getReplayFilePath(p_date, p_symbol, "mbp10");
    const QString tradesPath = DBClient::getReplayFilePath(p_date, p_symbol, "trades");

    const qint64 startEpochMs = QDateTime(p_date, p_startTime, TradingHours::MARKET_TIMEZONE).toMSecsSinceEpoch();

    // Load Mbp10 (Level 2) records
    if (QFile::exists(mbp10Path))
    {
        try
        {
            databento::DbnFileStore store(std::filesystem::path(mbp10Path.toStdString()));
            const databento::Record* record = nullptr;
            while ((record = store.NextRecord()) != nullptr)
            {
                if (record->Holds<databento::Mbp10Msg>())
                {
                    const auto& msg = record->Get<databento::Mbp10Msg>();
                    const qint64 epochMs = static_cast<qint64>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(msg.hd.ts_event.time_since_epoch())
                            .count());
                    if (epochMs >= startEpochMs)
                    {
                        m_records.append({epochMs, p_symbol, DBRecordTranslator::toLevel2(p_symbol, msg)});
                    }
                }
            }
            INFO << "Loaded" << m_records.size() << "Mbp10 records from" << mbp10Path;
        }
        catch (const std::exception& ex)
        {
            WARNING << "Failed to load Mbp10 file:" << ex.what();
        }
    }
    else
    {
        DEBUG << "No Mbp10 file found:" << mbp10Path;
    }

    // Load Trades records
    const int mbp10Count = m_records.size();
    if (QFile::exists(tradesPath))
    {
        try
        {
            databento::DbnFileStore store(std::filesystem::path(tradesPath.toStdString()));
            const databento::Record* record = nullptr;
            while ((record = store.NextRecord()) != nullptr)
            {
                if (record->Holds<databento::TradeMsg>())
                {
                    const auto& msg = record->Get<databento::TradeMsg>();
                    const qint64 epochMs = static_cast<qint64>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(msg.hd.ts_event.time_since_epoch())
                            .count());
                    if (epochMs >= startEpochMs)
                    {
                        m_records.append({epochMs, p_symbol, DBRecordTranslator::toTrade(p_symbol, msg)});
                    }
                }
            }
            INFO << "Loaded" << (m_records.size() - mbp10Count) << "Trade records from" << tradesPath;
        }
        catch (const std::exception& ex)
        {
            WARNING << "Failed to load Trades file:" << ex.what();
        }
    }
    else
    {
        DEBUG << "No Trades file found:" << tradesPath;
    }

    if (m_records.isEmpty())
    {
        return false;
    }

    // Sort all records by timestamp to produce a single merged timeline
    std::sort(m_records.begin(),
              m_records.end(),
              [](const ReplayRecord& a, const ReplayRecord& b) { return a.epochMs < b.epochMs; });

    INFO << "Loaded" << m_records.size() << "total replay records, time range:"
         << QDateTime::fromMSecsSinceEpoch(m_records.first().epochMs, TradingHours::MARKET_TIMEZONE)
                .toString("hh:mm:ss.zzz")
         << "to"
         << QDateTime::fromMSecsSinceEpoch(m_records.last().epochMs, TradingHours::MARKET_TIMEZONE)
                .toString("hh:mm:ss.zzz");

    return true;
}

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

    if (!loadReplayFiles(p_symbol, p_date, p_startTime))
    {
        CRITICAL << "Failed to load replay data";
        emit replayDataLoadFailed(
            QString("No replay data found for %1 on %2").arg(p_symbol, p_date.toString(Qt::ISODate)));
        return;
    }

    const qint64 initialEpoch = m_records.first().epochMs;
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

    if (!loadReplayFiles(p_symbol, p_date, p_startTime))
    {
        CRITICAL << "Failed to load replay data";
        emit replayDataLoadFailed(
            QString("No replay data found for %1 on %2").arg(p_symbol, p_date.toString(Qt::ISODate)));
        return;
    }

    const qint64 initialEpoch = m_records.first().epochMs;
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
    m_records.clear();
    m_records.squeeze();
    m_currentIndex = 0;

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
    if (m_currentIndex >= m_records.size())
        return;

    const ReplayRecord& rec = m_records[m_currentIndex];
    m_currentIndex++;

    updateReplayTime(rec.epochMs);

    if (std::holds_alternative<Level2>(rec.data))
    {
        emit replayLevel2(rec.symbol, std::get<Level2>(rec.data));
    }
    else if (std::holds_alternative<Trade>(rec.data))
    {
        emit replayTrade(rec.symbol, std::get<Trade>(rec.data));
    }
}

void ReplayEngine::scheduleNext()
{
    if (m_currentIndex >= m_records.size())
    {
        INFO << "Replay reached end of data (" << m_records.size() << " records played)";
        m_state = PlaybackState::Stopped;
        emit replayEndReached();
        emit replayStopped();
        return;
    }

    const qint64 delay = calculateWallClockDelay(m_records[m_currentIndex].epochMs);
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
        emit replayTimeUpdated(newTime);
    }
}
