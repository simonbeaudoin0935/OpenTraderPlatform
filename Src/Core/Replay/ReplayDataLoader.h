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
 * @brief Loads recorded market data of a single type from a SQLite database
 *
 * Each instance handles ONE data type (bars OR market depth quotes) with its
 * own database connection, cursor, and ping-pong buffers. ReplayEngine creates
 * two instances — one for bars, one for depth — each with an independent timer.
 *
 * Handles:
 * - Loading data from a single replay database file
 * - Ping-pong buffer strategy for memory efficiency with large datasets
 * - Async prefetching of next buffer while current is consumed
 *
 * Database files are organized by date in the recorder data directory:
 * - Bars: {cacheLocation}/RecordedLiveData/Bars/{YYYY-MM-DD}.db
 * - Depth: {cacheLocation}/RecordedLiveData/MarketDepthQuotes/{YYYY-MM-DD}.db
 *
 * Threading: Main operations run in caller's thread, prefetch uses QtConcurrent
 */
class ReplayDataLoader : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief The type of data this loader handles
     */
    enum class DataType
    {
        Bar,
        MarketDepthQuote,
        Quote
    };

    /**
     * @brief Single data point from replay database
     */
    struct ReplayDataPoint
    {
        qint64 id = 0; // Database ID for cursor tracking
        QString stockTicker;
        qint64 epochMs = 0;
        QString objectType; // For quotes: "QuoteStream", "Heartbeat", "Error"
        QByteArray jsonRawData;
    };

    /// Number of records per buffer (configurable for testing)
    static constexpr int BUFFER_SIZE = 1000;

    /// Prefetch threshold: trigger prefetch when this % of buffer consumed
    static constexpr int PREFETCH_THRESHOLD_PERCENT = 80;

    explicit ReplayDataLoader(DataType p_dataType, QObject* p_parent = nullptr);
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
     * @brief Reset loader state, close database
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

    /**
     * @brief Get the data type this loader handles
     */
    [[nodiscard]] DataType getDataType() const
    {
        return m_dataType;
    }

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
    const DataType m_dataType;

    // Database connection (unique name per instance)
    QSqlDatabase m_db;
    QString m_dbConnectionName;

    // Ping-pong buffers
    QVector<ReplayDataPoint> m_bufferA;
    QVector<ReplayDataPoint> m_bufferB;
    QVector<ReplayDataPoint>* m_activeBuffer = nullptr;
    QVector<ReplayDataPoint>* m_loadingBuffer = nullptr;
    int m_activeBufferIndex = 0;

    // Database cursor tracking (next ID to read)
    qint64 m_nextId = 1;

    // State
    QDate m_loadedDate;
    QTime m_startTime;
    qint64 m_startEpochMs = 0;
    bool m_isPrefetching = false;
    bool m_endOfDataReached = false;
    bool m_loadingBufferReady = false;

    // Async prefetch
    QFuture<void> m_prefetchFuture;
    mutable QMutex m_bufferMutex;

    // Statistics
    qint64 m_totalCount = 0;
    qint64 m_recordsConsumed = 0;

    /**
     * @brief Build database file path for the given date and data type
     */
    [[nodiscard]] QString getDbPath(QDate p_date) const;

    /**
     * @brief Open database connection
     */
    bool openDatabase(QDate p_date);

    /**
     * @brief Close database connection
     */
    void closeDatabase();

    /**
     * @brief Ensure database indexes exist for efficient queries
     */
    void ensureIndexes();

    /**
     * @brief Load initial buffer of data from database
     */
    bool loadInitialBuffer();

    /**
     * @brief Load a chunk of data into the specified buffer
     * @param p_buffer Buffer to fill
     * @param p_isInitial true if this is the initial load (use time-based query)
     */
    void loadBufferChunk(QVector<ReplayDataPoint>* p_buffer, bool p_isInitial);

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
