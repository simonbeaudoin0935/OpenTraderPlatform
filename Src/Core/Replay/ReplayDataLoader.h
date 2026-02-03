#pragma once

#include <QByteArray>
#include <QDate>
#include <QLoggingCategory>
#include <QObject>
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
 * Database files are organized by date in the recorder data directory.
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

    explicit ReplayDataLoader(QObject* p_parent = nullptr);
    ~ReplayDataLoader() override;

    Q_DISABLE_COPY(ReplayDataLoader)

    /**
     * @brief Load replay database for specified date
     * @param p_date Date to load
     * @param p_startTime Start time (load data from this time forward)
     * @return true if database loaded successfully
     */
    bool loadDatabase(QDate p_date, QTime p_startTime);

    /**
     * @brief Reset loader state, close databases
     */
    void reset();

    /**
     * @brief Check if more data points are available
     */
    [[nodiscard]] bool hasMoreData() const;

    /**
     * @brief Check if async prefetch is in progress
     */
    [[nodiscard]] bool isPrefetching() const;

    /**
     * @brief Get next data point and advance position
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
     * @brief Get last timestamp in loaded data
     */
    [[nodiscard]] QDateTime getLastTimestamp() const;

    /**
     * @brief Get list of stock tickers available in this replay
     */
    [[nodiscard]] QStringList getAvailableStocks() const;

  signals:
    /**
     * @brief Emitted when prefetch buffer is ready
     */
    void bufferReady();

  private:
    // Ping-pong buffer implementation (Phase 1.2)
    // For now, simple single-buffer implementation

    QVector<ReplayDataPoint> m_buffer;
    int m_currentIndex = 0;
    bool m_isPrefetching = false;

    QDate m_loadedDate;
    QTime m_startTime;

    /**
     * @brief Load initial buffer of data from database
     */
    bool loadInitialBuffer();

    /**
     * @brief Trigger async prefetch of next buffer chunk
     */
    void triggerPrefetch();
};
