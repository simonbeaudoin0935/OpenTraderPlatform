#pragma once

#include <QDateTime>
#include <QLoggingCategory>
#include <QObject>
#include <QTimer>

Q_DECLARE_LOGGING_CATEGORY(ReplayEngineLog)

class TSClient;
class ReplayDataLoader;

/**
 * @class ReplayEngine
 * @brief Orchestrates replay of recorded market data at configurable speeds
 *
 * ReplayEngine is composed into MainAlgo and runs in MainAlgoThread.
 * It manages:
 * - Replay state (stopped, playing, paused)
 * - Playback speed control
 * - Timing between data emissions using timestamp deltas
 * - Coordination with ReplayDataLoader for data retrieval
 * - Injection of data into TSClient's MockNetworkReply objects
 *
 * Threading: Runs in MainAlgoThread (inherits parent's thread via Qt parenting)
 */
class ReplayEngine : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief State of the replay playback
     */
    enum class PlaybackState
    {
        Stopped, ///< No replay active
        Playing, ///< Actively replaying data
        Paused   ///< Replay paused, can be resumed
    };
    Q_ENUM(PlaybackState)

    /**
     * @brief Playback speed multiplier
     *
     * Values represent percentage of real-time (100 = 1x speed).
     * AsFastAsPossible uses 0ms timer delays.
     */
    enum class PlaybackSpeed
    {
        Half = 50,            ///< 0.5x speed (delays multiplied by 2)
        Normal = 100,         ///< 1.0x speed (real-time)
        Double = 200,         ///< 2.0x speed (delays divided by 2)
        Fast5x = 500,         ///< 5.0x speed
        Fast10x = 1000,       ///< 10.0x speed
        AsFastAsPossible = -1 ///< 0ms timer (event loop breathing room only)
    };
    Q_ENUM(PlaybackSpeed)

    /**
     * @brief Construct ReplayEngine
     * @param p_parent Parent object (MainAlgo), determines thread affinity
     * @param p_tsClient Reference to TSClient singleton for data injection
     */
    explicit ReplayEngine(QObject* p_parent, TSClient* p_tsClient);
    ~ReplayEngine() override;

    Q_DISABLE_COPY(ReplayEngine)

    /**
     * @brief Start replay from specified date and time
     * @param p_date Date to replay
     * @param p_startTime Time of day to start replay
     * @param p_speed Initial playback speed
     *
     * Loads data from replay database starting at p_startTime (not market open).
     * First data point will be emitted immediately, subsequent points scheduled
     * based on timestamp deltas and playback speed.
     */
    void startReplay(QDate p_date, QTime p_startTime, PlaybackSpeed p_speed);

    /**
     * @brief Stop replay and clean up resources
     *
     * Stops timer, clears data loader, resets state to Stopped.
     * Does NOT restore live mode - that's MainAlgo/MainApp's responsibility.
     */
    void stopReplay();

    /**
     * @brief Pause replay (timer stopped, state preserved)
     */
    void pauseReplay();

    /**
     * @brief Resume replay from paused state
     */
    void resumeReplay();

    /**
     * @brief Get current playback state
     */
    [[nodiscard]] PlaybackState getState() const
    {
        return m_state;
    }

    /**
     * @brief Get current playback speed
     */
    [[nodiscard]] PlaybackSpeed getSpeed() const
    {
        return m_speed;
    }

    /**
     * @brief Check if replay is currently active (playing or paused)
     */
    [[nodiscard]] bool isActive() const
    {
        return m_state != PlaybackState::Stopped;
    }

  signals:
    /**
     * @brief Emitted when replay starts successfully
     */
    void replayStarted();

    /**
     * @brief Emitted when replay stops (user stop or end of data)
     */
    void replayStopped();

    /**
     * @brief Emitted when replay pauses
     */
    void replayPaused();

    /**
     * @brief Emitted when replay resumes
     */
    void replayResumed();

    /**
     * @brief Emitted when replay time advances
     * @param p_currentTime New current replay time (discrete jumps)
     */
    void replayTimeUpdated(QDateTime p_currentTime);

    /**
     * @brief Emitted when replay reaches end of available data
     */
    void replayEndReached();

  private slots:
    /**
     * @brief Timer tick handler - processes next data point
     */
    void onTimerTick();

    /**
     * @brief Called when ReplayDataLoader has prefetched next buffer
     */
    void onBufferReady();

  private:
    QTimer m_playbackTimer;
    ReplayDataLoader* m_dataLoader = nullptr; // Owned, created on startReplay
    TSClient* m_tsClient;                     // Reference, not owned

    PlaybackState m_state = PlaybackState::Stopped;
    PlaybackSpeed m_speed = PlaybackSpeed::Normal;

    qint64 m_lastEmittedTimestampMs = 0;

    /**
     * @brief Emit current data point to appropriate TSClient stream
     */
    void emitCurrentDataPoint();

    /**
     * @brief Schedule next data point emission based on timestamp delta and speed
     */
    void scheduleNextDataPoint();

    /**
     * @brief Calculate timer delay based on timestamp delta and playback speed
     * @param p_deltaMs Raw delta between timestamps in milliseconds
     * @return Scaled delay for timer (0 for AsFastAsPossible)
     */
    [[nodiscard]] qint64 calculateScaledDelay(qint64 p_deltaMs) const;

    /**
     * @brief Check if TSClient has an active stream for the given stock
     * @param p_symbol Stock ticker symbol
     * @param p_isBar true for bar stream, false for market depth
     * @return true if stream exists and data should be injected
     */
    [[nodiscard]] bool hasStreamForStock(const QString& p_symbol, bool p_isBar) const;
};
