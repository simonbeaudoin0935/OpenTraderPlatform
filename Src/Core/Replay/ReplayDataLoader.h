#pragma once

#include <QByteArray>
#include <QDate>
#include <QFuture>
#include <QLoggingCategory>
#include <QMutex>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QTime>
#include <QVector>

Q_DECLARE_LOGGING_CATEGORY(ReplayDataLoaderLog)

/**
 * @class ReplayDataLoader
 * @brief Loads and manages recorded market data from SQLite databases
 *
 * Handles:
 * - Loading bars and market depth data from replay database files
 * - Merging both data types into a single chronological sequence
 * - Ping-pong buffer strategy for memory efficiency with large datasets
 * - Async prefetching of next buffer while current is consumed
 *
 * Database files are organized by date in the recorder data directory:
 * - Bars: {cacheLocation}/RecordedData/Bars/RecordedLiveBars_{date}.db
 * - Depth: {cacheLocation}/RecordedData/MarketDepthQuotes/RecordedLiveMarketDepthQuotes_{date}.db
 *
 * Threading: Main operations run in caller's thread, prefetch uses QtConcurrent
 */
class ReplayDataLoader : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Single data point from replay database
     */
    struct ReplayDataPoint
    {
        qint64 id = 0; // Database ID for cursor tracking
        QString stockTicker;
        qint64 epochMs = 0;
        QByteArray jsonRawData;

        enum class Type
        {
            Bar,
            MarketDepthQuote
        } type = Type::Bar;

        /**
         * @brief Comparison for chronological sorting
         */
        bool operator<(const ReplayDataPoint& p_other) const
        {
            return epochMs < p_other.epochMs;
        }
    };

    /// Number of records per buffer (configurable for testing)
    static constexpr int BUFFER_SIZE = 1000;

    /// Prefetch threshold: trigger prefetch when this % of buffer consumed
    static constexpr int PREFETCH_THRESHOLD_PERCENT = 80;

    explicit ReplayDataLoader(QObject* p_parent = nullptr);
    ~ReplayDataLoader() override;

    Q_DISABLE_COPY(ReplayDataLoader)

    /**
     * @brief Load replay database for specified date
     * @param p_date Date to load
     * @param p_startTime Start time (load data from this time forward)
     * @return true if database loaded successfully and has data
     */
    bool loadDatabase(QDate p_date, QTime p_startTime);

    /**
     * @brief Reset loader state, close databases
     */
    void reset();

    /**
     * @brief Check if more data points are available in active buffer
     */
    [[nodiscard]] bool hasMoreData() const;

    /**
     * @brief Check if async prefetch is in progress
     */
    [[nodiscard]] bool isPrefetching() const;

    /**
     * @brief Get next data point and advance position
     *
     * May trigger buffer swap and prefetch when threshold reached.
     * @return Next data point (call hasMoreData() first)
     */
    ReplayDataPoint getNextDataPoint();

    /**
     * @brief Peek at next data point without advancing
     * @return Reference to next data point
     */
    [[nodiscard]] const ReplayDataPoint& peekNextDataPoint() const;

    /**
     * @brief Get first timestamp in loaded data
     */
    [[nodiscard]] QDateTime getFirstTimestamp() const;

    /**
     * @brief Get last timestamp in loaded data (from database, not buffer)
     */
    [[nodiscard]] QDateTime getLastTimestamp() const;

    /**
     * @brief Get list of stock tickers available in this replay
     */
    [[nodiscard]] QStringList getAvailableStocks() const;

    /**
     * @brief Get total record count for progress calculation
     */
    [[nodiscard]] qint64 getTotalRecordCount() const;

    /**
     * @brief Get current position for progress calculation
     */
    [[nodiscard]] qint64 getCurrentPosition() const;

  signals:
    /**
     * @brief Emitted when prefetch buffer is ready and swapped
     */
    void bufferReady();

    /**
     * @brief Emitted when all data has been consumed
     */
    void dataExhausted();

  private:
    // Database connections (unique names per instance)
    QSqlDatabase m_barsDb;
    QSqlDatabase m_depthDb;
    QString m_barsDbConnectionName;
    QString m_depthDbConnectionName;

    // Ping-pong buffers
    QVector<ReplayDataPoint> m_bufferA;
    QVector<ReplayDataPoint> m_bufferB;
    QVector<ReplayDataPoint>* m_activeBuffer = nullptr;
    QVector<ReplayDataPoint>* m_loadingBuffer = nullptr;
    int m_activeBufferIndex = 0;

    // Database cursor tracking (next ID to read)
    qint64 m_nextBarsId = 1;
    qint64 m_nextDepthId = 1;

    // State
    QDate m_loadedDate;
    QTime m_startTime;
    qint64 m_startEpochMs = 0;
    bool m_isPrefetching = false;
    bool m_endOfBarsReached = false;
    bool m_endOfDepthReached = false;
    bool m_loadingBufferReady = false;

    // Async prefetch
    QFuture<void> m_prefetchFuture;
    mutable QMutex m_bufferMutex;

    // Statistics
    qint64 m_totalBarsCount = 0;
    qint64 m_totalDepthCount = 0;
    qint64 m_recordsConsumed = 0;

    /**
     * @brief Build database file paths for the given date
     */
    [[nodiscard]] QString getBarsDbPath(QDate p_date) const;
    [[nodiscard]] QString getDepthDbPath(QDate p_date) const;

    /**
     * @brief Open database connections
     */
    bool openDatabases(QDate p_date);

    /**
     * @brief Close database connections
     */
    void closeDatabases();

    /**
     * @brief Ensure database indexes exist for efficient queries
     * @param p_db Database connection
     * @param p_isBarsDb true for bars database, false for depth database
     */
    void ensureIndexes(QSqlDatabase& p_db, bool p_isBarsDb);

    /**
     * @brief Load initial buffer of data from databases
     */
    bool loadInitialBuffer();

    /**
     * @brief Load a chunk of data into the specified buffer
     * @param p_buffer Buffer to fill
     * @param p_isInitial true if this is the initial load (use time-based query)
     */
    void loadBufferChunk(QVector<ReplayDataPoint>* p_buffer, bool p_isInitial);

    /**
     * @brief Merge bars and depth data chronologically
     */
    void mergeChronologically(QVector<ReplayDataPoint>& p_bars,
                              QVector<ReplayDataPoint>& p_depth,
                              QVector<ReplayDataPoint>* p_output);

    /**
     * @brief Trigger async prefetch of next buffer chunk
     */
    void triggerPrefetch();

    /**
     * @brief Check if we should trigger prefetch based on consumption
     */
    [[nodiscard]] bool shouldPrefetch() const;

    /**
     * @brief Swap active and loading buffers
     */
    void swapBuffers();

    /**
     * @brief Called when prefetch completes (in worker thread context)
     */
    void onPrefetchComplete();
};
