#pragma once

#include <QDateTime>
#include <QLoggingCategory>
#include <QObject>
#include <QTimer>
#include <memory>
#include <variant>

#include "Level2.h"
#include "Trade.h"

// Forward declaration — full type only needed in .cpp
namespace databento
{
    class DbnFileStore;
}

Q_DECLARE_LOGGING_CATEGORY(ReplayEngineLog)

/**
 * @class ReplayEngine
 * @brief Orchestrates replay of recorded market data from .dbn.zst files
 *
 * ReplayEngine is composed into MainAlgo and runs in MainAlgoThread.
 * It reads Databento .dbn.zst replay files (Mbp10 + Trades), merges them
 * into a single time-ordered stream, and emits records at configurable speeds.
 *
 * Records are streamed directly from the compressed files — no preloading into
 * RAM. The two files are merged on the fly using a 2-way lookahead: one peeked
 * record from each stream, always emitting whichever has the earlier timestamp.
 *
 * Key responsibilities:
 * - Replay state (stopped, playing, paused)
 * - Playback speed control
 * - Timer-based record scheduling using wall-clock anchoring
 * - Emit Level2, Trade data via signals (consumed by SymbolContext receivers)
 *
 * Threading: Runs in MainAlgoThread (inherits parent's thread via Qt parenting)
 */
class ReplayEngine : public QObject
{
    Q_OBJECT

  public:
    enum class PlaybackState
    {
        Stopped,
        Playing,
        Paused
    };
    Q_ENUM(PlaybackState)

    enum class PlaybackSpeed
    {
        SuperSlow = 1,        ///< 0.01x speed
        VerySlow = 10,        ///< 0.1x speed
        Half = 50,            ///< 0.5x speed
        Normal = 100,         ///< 1.0x speed (real-time)
        Double = 200,         ///< 2.0x speed
        Fast5x = 500,         ///< 5.0x speed
        Fast10x = 1000,       ///< 10.0x speed
        Fast50x = 5000,       ///< 50.0x speed
        Fast100x = 10000,     ///< 100.0x speed
        AsFastAsPossible = -1 ///< 0ms timer
    };
    Q_ENUM(PlaybackSpeed)

    explicit ReplayEngine(QObject* p_parent);
    ~ReplayEngine() override;

    Q_DISABLE_COPY(ReplayEngine)

    /**
     * @brief Start replay from specified date and time
     * @param p_symbol Symbol to replay
     * @param p_date Date to replay
     * @param p_startTime Time of day to start replay
     * @param p_speed Initial playback speed
     */
    void startReplay(const QString& p_symbol, QDate p_date, QTime p_startTime, PlaybackSpeed p_speed);

    /**
     * @brief Start replay in paused state, emitting only the first data points
     */
    void startReplayPaused(const QString& p_symbol, QDate p_date, QTime p_startTime, PlaybackSpeed p_speed);

    void stopReplay();
    void pauseReplay();
    void resumeReplay();
    void setSpeed(PlaybackSpeed p_speed);

    [[nodiscard]] PlaybackState getState() const
    {
        return m_state;
    }
    [[nodiscard]] PlaybackSpeed getSpeed() const
    {
        return m_speed;
    }
    [[nodiscard]] bool isActive() const
    {
        return m_state != PlaybackState::Stopped;
    }

  signals:
    /// @brief Replay lifecycle signals
    /// Thread context: Emitted from MainAlgo worker thread
    void replayStarted();
    void replayStopped();
    void replayPaused();
    void replayResumed();
    void replayTimeUpdated(QDateTime p_currentTime);
    void replayEndReached();
    void replayDataLoadFailed(const QString& p_errorMessage);

    /**
     * @brief Replay data signals — connected to SymbolContext receivers by MainAlgo
     * Thread context: Emitted from MainAlgo worker thread
     */
    void replayLevel2(const QString& p_symbol, const Level2& p_level2);
    void replayTrade(const QString& p_symbol, const Trade& p_trade);

  private slots:
    void onTimerTick();

  private:
    /**
     * @brief A single lookahead record peeked from one of the two streams
     */
    struct PeekedRecord
    {
        qint64 epochMs = 0;
        std::variant<Level2, Trade> data;
        bool valid = false;
    };

    /**
     * @brief Open .dbn.zst files and prime the lookahead records, skipping before startTime
     * @return true if at least one stream has records at or after startTime
     */
    bool openReplayStreams(const QString& p_symbol, QDate p_date, QTime p_startTime);

    /**
     * @brief Read the next mbp10 record at or after m_startEpochMs into m_nextMbp10
     */
    void advanceMbp10();

    /**
     * @brief Read the next trade record at or after m_startEpochMs into m_nextTrade
     */
    void advanceTrade();

    void emitNextRecord();
    void scheduleNext();
    [[nodiscard]] qint64 calculateWallClockDelay(qint64 p_replayEpochMs) const;
    void updateReplayTime(qint64 p_epochMs);
    void closeStreams();

    QTimer m_timer;

    // Streaming state — open file stores, one peeked record each
    std::unique_ptr<databento::DbnFileStore> m_mbp10Store;
    std::unique_ptr<databento::DbnFileStore> m_tradesStore;
    PeekedRecord m_nextMbp10;
    PeekedRecord m_nextTrade;
    qint64 m_startEpochMs = 0;
    QString m_currentSymbol;

    PlaybackState m_state = PlaybackState::Stopped;
    PlaybackSpeed m_speed = PlaybackSpeed::Normal;

    // Wall-clock anchor for timing
    qint64 m_wallClockAnchorMs = 0;
    qint64 m_replayEpochAnchorMs = 0;
    qint64 m_pauseWallClockMs = 0;
};
