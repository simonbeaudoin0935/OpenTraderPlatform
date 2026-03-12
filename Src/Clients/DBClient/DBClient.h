#pragma once

#include <atomic>
#include <memory>
#include <variant>

#include <QDateTime>
#include <QLoggingCategory>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QVector>

#include <databento/live_threaded.hpp>
#include <databento/historical.hpp>
#include <databento/symbol_map.hpp>

#include "Core/Models/Bar.h"
#include "Core/Models/Level2.h"
#include "Core/Models/Trade.h"
#include "Misc/TimeFrame.h"
#include "PlaybackTypes.h"

Q_DECLARE_LOGGING_CATEGORY(DBClientLog)

/**
 * @brief Databento API client — singleton on dedicated thread
 *
 * Manages the Databento API key, live streaming via LiveThreaded, and
 * historical bar fetching via the Historical client. Translates raw Databento
 * records into L2Trader domain types and emits Qt signals for downstream
 * consumers (MainAlgo receivers).
 *
 * ## Threading model
 * - DBClient lives on its own QThread (like TSClient).
 * - LiveThreaded spawns an internal thread; our RecordCallback runs on that thread.
 *   Qt signals emitted from the callback are auto-queued to the receiver's thread.
 * - Historical fetching runs on QThreadPool to avoid blocking the DBClient thread.
 * - State-modifying methods self-route to the DBClient thread if called from elsewhere.
 *
 * ## Subscription model
 * - Each symbol subscribes to Schema::Mbp10 (Level 2 book) + Schema::Trades.
 * - Subscriptions accumulate for the session — Databento does NOT support unsubscribe.
 * - Data for non-displayed symbols is silently ignored by receivers.
 */
class DBClient : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Connection lifecycle states
     */
    enum class ConnectionState : quint8
    {
        Disconnected, ///< No live session active
        Connecting,   ///< LiveThreaded built, Start() called, waiting for metadata
        Connected,    ///< Metadata received, session active and streaming
        Reconnecting, ///< Exception occurred, auto-reconnect in progress
    };
    Q_ENUM(ConnectionState)

    [[nodiscard]] static DBClient* getInstance();
    static void destroyInstance();
    [[nodiscard]] static bool isInstantiated()
    {
        return m_instance != nullptr;
    }

    Q_DISABLE_COPY_MOVE(DBClient)

    void start()
    {
        m_thread.start();
    }

    // ── API Key management ─────────────────────────────────────────────

    void loadApiKey();
    void storeApiKey(const QString& p_apiKey);
    [[nodiscard]] bool hasApiKey() const;
    [[nodiscard]] QString getApiKey() const;

    // ── Live streaming ─────────────────────────────────────────────────

    /**
     * @brief Build a LiveThreaded client and start the session.
     * Requires a valid API key. Emits connectionStateChanged(Connecting).
     * On metadata receipt, transitions to Connected.
     */
    void connectLive();

    /**
     * @brief Stop the live session and destroy the LiveThreaded client.
     * Emits connectionStateChanged(Disconnected).
     */
    void disconnectLive();

    /**
     * @brief Subscribe a symbol to live Level 2 + Trades data.
     * No-op if the symbol is already subscribed or not connected.
     * @param p_symbol Ticker symbol (e.g. "AAPL")
     */
    void subscribeLive(const QString& p_symbol);

    // ── Historical data ────────────────────────────────────────────────

    /**
     * @brief Fetch historical OHLCV bars for a symbol at the requested timescale.
     *
     * Runs asynchronously on QThreadPool. Emits historicalBarsReceived on completion.
     * Non-native timescales are fetched from their source schema and aggregated in-app:
     *   5m / 15m / 30m  — fetched as 1m then aggregated
     *   4h              — fetched as 1h then aggregated
     *   1w / 1M         — fetched as 1d then aggregated
     *
     * @param p_symbol Ticker symbol
     * @param p_start  Range start (inclusive, market timezone)
     * @param p_end    Range end (inclusive, market timezone)
     * @param p_tf     Timescale of bars to return (default: ONE_MINUTE)
     */
    void fetchHistoricalBars(const QString& p_symbol,
                             const QDateTime& p_start,
                             const QDateTime& p_end,
                             TimeFrame p_tf = TimeFrame::ONE_MINUTE);

    // ── Replay data download ──────────────────────────────────────────

    /**
     * @brief Download replay data (Mbp10 + Trades) for a symbol and date.
     * Runs asynchronously on QThreadPool. Emits replayDownloadFinished on completion.
     * Files are saved to ~/.local/share/L2Trader/ReplayData/{YYYY-MM-DD}/
     * @param p_symbol Ticker symbol
     * @param p_date   Trading date to download
     */
    void downloadReplayData(const QString& p_symbol, const QDate& p_date);

    /**
     * @brief Get or set the base directory where replay data is stored.
     * Defaults to ~/.local/share/L2Trader/ReplayData. Can be overridden to point
     * to an external drive or transferred folder.
     */
    [[nodiscard]] static QString getReplayBaseDir();
    static void setReplayBaseDir(const QString& p_dir);

    /**
     * @brief Get the directory path for replay data of a given date.
     */
    [[nodiscard]] static QString getReplayDataDir(const QDate& p_date);

    /**
     * @brief Get the file path for a specific schema's replay file.
     * @param p_date Trading date
     * @param p_symbol Ticker symbol
     * @param p_schema "mbp10" or "trades"
     */
    [[nodiscard]] static QString
    getReplayFilePath(const QDate& p_date, const QString& p_symbol, const QString& p_schema);

    /**
     * @brief Check if replay data exists for a symbol and date.
     * @return true if both Mbp10 and Trades .dbn.zst files exist
     */
    [[nodiscard]] static bool hasReplayData(const QDate& p_date, const QString& p_symbol);

    // ── Replay playback ───────────────────────────────────────────────

    using PlaybackState = Playback::State;
    using PlaybackSpeed = Playback::Speed;

    /**
     * @brief Start replay from specified date and time.
     * Opens .dbn.zst files for the symbol, merges Level2+Trade into a time-ordered
     * stream, emits records via newLevel2/newTrade (same as live).
     */
    void startReplay(const QString& p_symbol, QDate p_date, QTime p_startTime, PlaybackSpeed p_speed);

    /**
     * @brief Start replay in paused state, emitting only the first data point(s).
     * Used for chart pre-loading before user hits play.
     */
    void startReplayPaused(const QString& p_symbol, QDate p_date, QTime p_startTime, PlaybackSpeed p_speed);

    void stopReplay();
    void pauseReplay();
    void resumeReplay();
    void setReplaySpeed(PlaybackSpeed p_speed);

    [[nodiscard]] PlaybackState getPlaybackState() const
    {
        return m_playbackState;
    }
    [[nodiscard]] PlaybackSpeed getPlaybackSpeed() const
    {
        return m_playbackSpeed;
    }
    [[nodiscard]] bool isReplayActive() const
    {
        return m_playbackState != PlaybackState::Stopped;
    }

    // ── State queries ──────────────────────────────────────────────────

    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] ConnectionState getConnectionState() const;

    /**
     * @brief Get or set the Databento dataset identifier.
     * Defaults to "XNAS.ITCH" (NASDAQ TotalView). Persisted in QSettings.
     */
    [[nodiscard]] QString getDataset() const;
    void setDataset(const QString& p_dataset);

  signals:
    /**
     * @brief Emitted when the API-key-based connection state changes (legacy, kept for GUI button)
     * Thread context: Emitted from Main/GUI thread
     * @param p_isConnected true if a valid API key is present
     */
    void connectionStateChanged(bool p_isConnected);

    /**
     * @brief Emitted on live connection lifecycle transitions
     * Thread context: Emitted from Databento callback thread (auto-queued)
     * @param p_state The new connection state
     */
    void liveConnectionStateChanged(DBClient::ConnectionState p_state);

    /**
     * @brief New Level 2 (10-level book) snapshot from live stream
     * Thread context: Emitted from Databento callback thread
     * @param p_symbol Resolved ticker symbol
     * @param p_level2 The 10-level bid/ask snapshot
     */
    void newLevel2(const QString& p_symbol, const Level2& p_level2);

    /**
     * @brief New trade print from live stream
     * Thread context: Emitted from Databento callback thread
     * @param p_symbol Resolved ticker symbol
     * @param p_trade  The trade data
     */
    void newTrade(const QString& p_symbol, const Trade& p_trade);

    /**
     * @brief Historical bars received for a symbol (async result)
     * Thread context: Emitted from QThreadPool worker thread (auto-queued)
     * @param p_symbol Ticker symbol
     * @param p_bars   Vector of 1-minute OHLCV bars
     */
    void historicalBarsReceived(const QString& p_symbol, const QVector<Bar>& p_bars);

    /**
     * @brief Replay data download completed
     * Thread context: Emitted from QThreadPool worker thread (auto-queued)
     * @param p_symbol Ticker symbol
     * @param p_date   Date downloaded
     * @param p_success true if download succeeded
     * @param p_errorMessage Error description (empty on success)
     */
    void
    replayDownloadFinished(const QString& p_symbol, const QDate& p_date, bool p_success, const QString& p_errorMessage);

    /**
     * @brief Trading status update for a symbol from the live status stream.
     * Thread context: Emitted from Databento callback thread (auto-queued)
     * @param p_symbol              Resolved ticker symbol
     * @param p_isHalted            true if trading is halted/paused/suspended
     * @param p_haltReason          Human-readable reason for the halt (empty if not halted)
     * @param p_isShortSellRestricted true if short selling is restricted (SSR/HTB)
     */
    void newStatus(const QString& p_symbol, bool p_isHalted, const QString& p_haltReason, bool p_isShortSellRestricted);

    /**
     * @brief Error received from the Databento Live Subscription Gateway.
     * Thread context: Emitted from Databento callback thread (auto-queued)
     * @param p_errorText   Human-readable error message from the gateway
     * @param p_isFatal     true if this error terminates the session (e.g. InvalidSubscription, AuthFailed)
     */
    void liveGatewayError(const QString& p_errorText, bool p_isFatal);

    /**
     * @brief Cumulative data usage updated (live + historical bytes received)
     * Thread context: Emitted from Databento callback thread or QThreadPool worker
     * @param p_totalBytes Total bytes received since application start
     */
    void dataUsageUpdated(qsizetype p_totalBytes);

    // ── Replay lifecycle signals ──────────────────────────────────────
    /// Thread context: Emitted from DBClient thread

    void replayStarted();
    void replayStopped();
    void replayPaused();
    void replayResumed();
    void replayTimeUpdated(QDateTime p_currentTime);
    void replayEndReached();
    void replayDataLoadFailed(const QString& p_errorMessage);

  private:
    explicit DBClient();
    ~DBClient() override;

    // ── Live callbacks (run on Databento's internal thread) ────────────
    void onMetadataReceived(databento::Metadata&& p_metadata);
    databento::KeepGoing onRecordReceived(const databento::Record& p_record);
    databento::LiveThreaded::ExceptionAction onException(const std::exception& p_exception);

    // ── Helpers ────────────────────────────────────────────────────────
    void setConnectionState(ConnectionState p_state);
    [[nodiscard]] QString resolveSymbol(const databento::Record& p_record) const;

    // ── Replay playback internals ─────────────────────────────────────

    struct PeekedRecord
    {
        qint64 epochMs = 0;
        std::variant<Level2, Trade> data;
        bool valid = false;
    };

    bool openReplayStreams(const QString& p_symbol, QDate p_date, QTime p_startTime);
    void advanceMbp10();
    void advanceTrade();
    void emitNextReplayRecord();
    void scheduleNextReplayTick();
    [[nodiscard]] qint64 calculateWallClockDelay(qint64 p_replayEpochMs) const;
    void updateReplayTime(qint64 p_epochMs);
    void closeReplayStreams();

  private slots:
    void onReplayTimerTick();

  private:
    static DBClient* m_instance;
    static QString m_replayBaseDir; ///< Empty = use default cache location

    // API key
    QString m_apiKey;

    // Live streaming
    std::unique_ptr<databento::LiveThreaded> m_liveClient;
    databento::PitSymbolMap m_symbolMap;
    QSet<QString> m_subscribedSymbols;
    ConnectionState m_connectionState = ConnectionState::Disconnected;
    mutable QMutex m_symbolMapMutex; ///< Guards m_symbolMap access across threads

    // Historical
    std::unique_ptr<databento::Historical> m_historicalClient;

    // Configuration
    QString m_dataset;

    // Data usage tracking
    std::atomic<qsizetype> m_totalDataReceivedBytes{0};
    std::atomic<int> m_recordCounter{0};
    static constexpr int k_emitEveryNRecords = 100; ///< Throttle dataUsageUpdated signal

    static constexpr const char* k_defaultDataset = "XNAS.ITCH";
    static constexpr const char* k_settingsKeyDataset = "Databento/Dataset";

    // Replay playback state
    QTimer m_replayTimer;
    std::unique_ptr<databento::DbnFileStore> m_mbp10Store;
    std::unique_ptr<databento::DbnFileStore> m_tradesStore;
    PeekedRecord m_nextMbp10;
    PeekedRecord m_nextTrade;
    qint64 m_startEpochMs = 0;
    QString m_replaySymbol;
    PlaybackState m_playbackState = PlaybackState::Stopped;
    PlaybackSpeed m_playbackSpeed = PlaybackSpeed::Normal;
    qint64 m_wallClockAnchorMs = 0;
    qint64 m_replayEpochAnchorMs = 0;
    qint64 m_pauseWallClockMs = 0;

    QThread m_thread;
};
