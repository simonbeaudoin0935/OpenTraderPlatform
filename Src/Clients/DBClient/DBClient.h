#pragma once

#include <memory>

#include <QLoggingCategory>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <databento/live_threaded.hpp>
#include <databento/historical.hpp>
#include <databento/symbol_map.hpp>

#include "Core/Models/Bar.h"
#include "Core/Models/Level2.h"
#include "Core/Models/Trade.h"

Q_DECLARE_LOGGING_CATEGORY(DBClientLog)

/**
 * @brief Databento API client — singleton
 *
 * Manages the Databento API key, live streaming via LiveThreaded, and
 * historical bar fetching via the Historical client. Translates raw Databento
 * records into L2Trader domain types and emits Qt signals for downstream
 * consumers (MainAlgo receivers).
 *
 * ## Threading model
 * - LiveThreaded spawns an internal thread; our RecordCallback runs on that thread.
 *   Qt signals emitted from the callback are auto-queued to the receiver's thread.
 * - Historical fetching runs on QThreadPool to avoid blocking the main thread.
 * - API key management runs on the main/GUI thread.
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
     * @brief Fetch historical 1-minute OHLCV bars for a symbol.
     * Runs asynchronously on QThreadPool. Emits historicalBarsReceived on completion.
     * @param p_symbol Ticker symbol
     * @param p_start  Range start (inclusive, market timezone)
     * @param p_end    Range end (exclusive, market timezone)
     */
    void fetchHistoricalBars(const QString& p_symbol, const QDateTime& p_start, const QDateTime& p_end);

    // ── Replay data download ──────────────────────────────────────────

    /**
     * @brief Download replay data (Mbp10 + Trades) for a symbol and date.
     * Runs asynchronously on QThreadPool. Emits replayDownloadFinished on completion.
     * Files are saved to ~/.cache/L2Trader/ReplayData/{YYYY-MM-DD}/
     * @param p_symbol Ticker symbol
     * @param p_date   Trading date to download
     */
    void downloadReplayData(const QString& p_symbol, const QDate& p_date);

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
     * @brief New Level 1 (BBO) snapshot from live stream
     * Thread context: Emitted from Databento callback thread
     * @param p_symbol Resolved ticker symbol
     * @param p_level1 The bid/ask BBO
     */
    void newLevel1(const QString& p_symbol, const Level1& p_level1);

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

    static DBClient* m_instance;

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

    static constexpr const char* k_defaultDataset = "XNAS.ITCH";
    static constexpr const char* k_settingsKeyDataset = "Databento/Dataset";
};
