#pragma once

#include <QDateTime>
#include <QLoggingCategory>
#include <QObject>
#include <QTimer>
#include <memory>

Q_DECLARE_LOGGING_CATEGORY(ReplayEngineLog)

class TSClient;
class ReplayDataLoader;

/**
 * @class ReplayEngine
 * @brief Orchestrates replay of recorded market data at configurable speeds
 *
 * ReplayEngine is composed into MainAlgo and runs in MainAlgoThread.
 * It manages two independent playback streams — one for bars, one for market
 * depth quotes — each with its own timer, data loader, and timestamp tracker.
 *
 * Key responsibilities:
 * - Replay state (stopped, playing, paused)
 * - Playback speed control (shared by both streams)
 * - Independent timing for bars and depth via separate single-shot timers
 * - Coordination with two ReplayDataLoader instances for data retrieval
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
        SuperSlow = 1,        ///< 0.01x speed (delays multiplied by 100)
        VerySlow = 10,        ///< 0.1x speed (delays multiplied by 10)
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
     * Loads data from replay databases starting at p_startTime.
     * First data point from each stream will be emitted immediately,
     * subsequent points scheduled based on timestamp deltas and playback speed.
     */
    void startReplay(QDate p_date, QTime p_startTime, PlaybackSpeed p_speed);

    /**
     * @brief Start replay in paused state, emitting only the first data points
     *
     * Used when entering replay mode to pre-populate the chart.
     * Emits first bar and first depth then immediately pauses.
     * User clicks Play to continue.
     */
    void startReplayPaused(QDate p_date, QTime p_startTime, PlaybackSpeed p_speed);

    /**
     * @brief Stop replay and clean up resources
     *
     * Stops timers, clears data loaders, resets state to Stopped.
     * Does NOT restore live mode - that's MainAlgo/MainApp's responsibility.
     */
    void stopReplay();

    /**
     * @brief Pause replay (timers stopped, state preserved)
     */
    void pauseReplay();

    /**
     * @brief Resume replay from paused state
     */
    void resumeReplay();

    /**
     * @brief Set playback speed on the fly (affects both streams)
     * Can be called while replay is playing or paused.
     */
    void setSpeed(PlaybackSpeed p_speed)
    {
        m_speed = p_speed;
    }

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
     * @brief Emitted when replay reaches end of available data (both streams)
     */
    void replayEndReached();

    /**
     * @brief Inject bar data into TSClient (cross-thread via QueuedConnection)
     * @param p_symbol Stock ticker symbol
     * @param p_data Shared pointer to JSON data (avoids deep copy across threads)
     */
    void injectBarData(const QString& p_symbol, std::shared_ptr<const QByteArray> p_data);

    /**
     * @brief Inject market depth data into TSClient (cross-thread via QueuedConnection)
     * @param p_symbol Stock ticker symbol
     * @param p_data Shared pointer to JSON data (avoids deep copy across threads)
     */
    void injectDepthData(const QString& p_symbol, std::shared_ptr<const QByteArray> p_data);

  private slots:
    /**
     * @brief Bar timer tick — processes next bar data point
     */
    void onBarTimerTick();

    /**
     * @brief Depth timer tick — processes next depth data point
     */
    void onDepthTimerTick();

    /**
     * @brief Called when bar loader has prefetched next buffer
     */
    void onBarBufferReady();

    /**
     * @brief Called when depth loader has prefetched next buffer
     */
    void onDepthBufferReady();

  private:
    // Independent timers for each data stream
    QTimer m_barTimer;
    QTimer m_depthTimer;

    // Independent data loaders (owned, created on startReplay)
    ReplayDataLoader* m_barLoader = nullptr;
    ReplayDataLoader* m_depthLoader = nullptr;

    TSClient* m_tsClient; // Reference, not owned

    PlaybackState m_state = PlaybackState::Stopped;
    PlaybackSpeed m_speed = PlaybackSpeed::Normal;

    // Wall-clock anchor: maps replay-epoch time to real wall-clock time
    // targetWallMs = m_wallClockAnchorMs + (replayEpochMs - m_replayEpochAnchorMs) * 100 / speed
    qint64 m_wallClockAnchorMs = 0;
    qint64 m_replayEpochAnchorMs = 0;
    qint64 m_pauseWallClockMs = 0; // Wall-clock time when paused (to adjust anchor on resume)

    // Track which streams have reached end of data
    bool m_barStreamEnded = false;
    bool m_depthStreamEnded = false;

    /**
     * @brief Initialize both loaders, loading databases for the given date/time
     * @return true if at least one loader has data
     */
    bool initLoaders(QDate p_date, QTime p_startTime);

    /**
     * @brief Clean up both loaders
     */
    void cleanupLoaders();

    /**
     * @brief Emit next bar data point, skipping stocks without active streams
     */
    void emitNextBar();

    /**
     * @brief Emit next depth data point, skipping stocks without active streams
     */
    void emitNextDepth();

    /**
     * @brief Schedule next bar emission using wall-clock anchor
     */
    void scheduleNextBar();

    /**
     * @brief Schedule next depth emission using wall-clock anchor
     */
    void scheduleNextDepth();

    /**
     * @brief Calculate wall-clock delay for a data point based on its replay timestamp
     * @param p_replayEpochMs The replay-epoch timestamp of the next data point
     * @return Delay in ms from now until the data point should fire (min 0)
     */
    [[nodiscard]] qint64 calculateWallClockDelay(qint64 p_replayEpochMs) const;

    /**
     * @brief Update MainApp::currentAppReplayTime to the max of current and new time
     */
    void updateReplayTime(qint64 p_epochMs);

    /**
     * @brief Check if both streams have ended and emit replayEndReached if so
     */
    void checkAllStreamsEnded();

    /**
     * @brief Check if TSClient has an active stream for the given stock
     * @param p_symbol Stock ticker symbol
     * @param p_isBar true for bar stream, false for market depth
     * @return true if stream exists and data should be injected
     */
    [[nodiscard]] bool hasStreamForStock(const QString& p_symbol, bool p_isBar) const;
};
