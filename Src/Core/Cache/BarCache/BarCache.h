#pragma once

#include <QLoggingCategory>
#include <QMap>
#include <QReadWriteLock>
#include <QFuture>

#include <expected>

#include "Bar.h"
#include "TSClient.h"
#include "CONSTANTS.h"

Q_DECLARE_LOGGING_CATEGORY(BarCacheLog)

class BarCache : public QObject
{
    Q_OBJECT

  public:
    explicit BarCache(const QString& symbol, QObject* parent = nullptr);
    ~BarCache();

    [[nodiscard]] const QString& getSymbol() const
    {
        return m_symbol;
    };

    typedef std::variant<std::shared_ptr<QVector<Bar>>,
                         QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>
        GetBarsResult_t;

    /*
     * Get Bars between two date-times
     *
     * @note : Both date-times must be in America/New_York timezone
     */
    GetBarsResult_t getBars(const QDate& day, const QTime& first, const QTime& last);

    void clearDatabase();

    /**
     * @brief Store a single bar in the cache.
     *
     * This is called by BarReceiver when new bars arrive from the stream.
     */
    void storeBar(const Bar& bar);

  private:
    [[nodiscard]]
    static constexpr QVector<std::tuple<QDate, QTime, QTime>> splitIntoTradingDayRanges(const QDateTime& first,
                                                                                        const QDateTime& last);

    std::optional<std::unique_ptr<QVector<Bar>>>
    getBarsFromCache(const QDate& date, const QTime& start, const QTime& end) const;

    void storeBarInCache(const Bar& bar);

    void storeBarsInCache(const QDate& date, const std::shared_ptr<QVector<Bar>>& bars);

    void handleReceivedAllPendingGetBarsRequests();
    QVector<Bar>
    fillHolesOfReceivedRequest(const QDateTime& first, const QDateTime& last, const QVector<Bar>& barsFromAPI) const;

    QVector<Bar>& getOrCreateDayVector(const QDate& date);

    const QString m_symbol;
    QString m_dbPath; // Path to the database file (managed by DatabaseThread)

    /**
     * @brief Thread-safety lock for in-memory bar cache
     * 
     * Lock Granularity Analysis (Architecture_Improvements.md point 1.3):
     * 
     * Current Design: Single QReadWriteLock per BarCache instance
     * - Each symbol has its own BarCache instance (via StockInstruments)
     * - Each BarCache has its own m_barCacheRwLock
     * - Multiple symbols accessing data simultaneously use DIFFERENT locks (no contention)
     * - Lock contention only occurs when SAME symbol requests different days simultaneously (rare)
     * 
     * Alternative Design: Per-day locking (considered but not implemented)
     * - Pros: Better concurrency for single-symbol multi-day requests
     * - Cons: More complex code, higher memory overhead, more lock objects
     * 
     * Decision: Current single-lock design is optimal because:
     * 1. Lock contention across symbols is already prevented (separate BarCache instances)
     * 2. Single-symbol multi-day concurrent requests are rare (typical access is sequential)
     * 3. QReadWriteLock allows multiple concurrent readers (getBarsFromCache)
     * 4. Write operations (storeBarInCache, storeBarsInCache) are fast (in-memory only)
     * 5. Profiling has not shown lock contention issues
     * 
     * Thread Safety Guarantees:
     * - Read operations (getBarsFromCache): Multiple threads can read simultaneously via QReadLocker
     * - Write operations (storeBarInCache, storeBarsInCache): Exclusive access via QWriteLocker
     * - Map modifications (getOrCreateDayVector): Protected by QWriteLocker
     * 
     * @note Marked mutable for use in const methods like getBarsFromCache()
     * @see Architecture_Improvements.md section 1.3 for full analysis
     */
    mutable QReadWriteLock m_barCacheRwLock;
    
    // Day-based storage: one QVector per trading day. Vector index maps to minute within trading day.
    QMap<QDate, QVector<Bar>> m_barCacheByDay;
};
