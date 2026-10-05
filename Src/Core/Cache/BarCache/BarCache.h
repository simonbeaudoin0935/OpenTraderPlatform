#pragma once

#include <QLoggingCategory>
#include <QMap>
#include <QReadWriteLock>
#include <QFuture>

#include <expected>

#include "Bar.h"
#include "TSClient.h"
#include "CONSTANTS.h"
#include "TimeFrame.h"

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

    /**
     * @brief Get bars between two times on a single trading day.
     *
     * @param tf    Timescale of bars to retrieve
     * @param day   Trading day
     * @param first Start time (inclusive, America/New_York)
     * @param last  End time (inclusive, America/New_York)
     */
    GetBarsResult_t getBars(TimeFrame tf, const QDate& day, const QTime& first, const QTime& last);

    void clearDatabase();
    void warmCurrentDayCacheForLive(const QDateTime& p_now);
    [[nodiscard]] std::optional<QDateTime> getLatestClosedBarTimestamp(TimeFrame tf, const QDate& date) const;

    /**
     * @brief Store a single bar in the cache.
     *
     * Called by BarReceiver (live 1m bars) and by the aggregation pipeline
     * (higher-TF derived bars). The caller must supply the TimeFrame explicitly
     * because bars do not carry a timescale field.
     *
     * @param tf  Timescale of the bar
     * @param bar The bar to store
     */
    void storeBar(TimeFrame tf, const Bar& bar);

  private:
    [[nodiscard]] static constexpr QVector<std::tuple<QDate, QTime, QTime>>
    splitIntoTradingDayRanges(const QDateTime& first, const QDateTime& last);

    std::optional<std::unique_ptr<QVector<Bar>>>
    getBarsFromCache(TimeFrame tf, const QDate& date, const QTime& start, const QTime& end) const;
    [[nodiscard]] bool
    wasCurrentDayTailBackfillAttemptedForAnchor(TimeFrame tf, const QDate& date, const QDateTime& latestClosed) const;
    void markCurrentDayTailBackfillAttemptForAnchor(TimeFrame tf, const QDate& date, const QDateTime& latestClosed);

    void storeBarInCache(TimeFrame tf, const Bar& bar);
    void prefillNullBarsThrough(TimeFrame tf, const QDate& date, const QTime& lastInclusive);
    [[nodiscard]] static QTime floorTimeToBarBoundary(TimeFrame tf, const QTime& time);

    void storeBarsInCache(TimeFrame tf, const QDate& date, const std::shared_ptr<QVector<Bar>>& bars);

    void handleReceivedAllPendingGetBarsRequests();
    QVector<Bar> fillHolesOfReceivedRequest(TimeFrame tf,
                                            const QDateTime& first,
                                            const QDateTime& last,
                                            const QVector<Bar>& barsFromAPI) const;

    QVector<Bar>& getOrCreateDayVector(TimeFrame tf, const QDate& date);

    const QString m_symbol;
    QString m_dbPath; // Path to the database file (managed by DatabaseThread)

    // Protects m_barCacheByTimeFrame. Each BarCache instance has its own lock.
    // QReadWriteLock allows concurrent readers, exclusive writer access.
    mutable QReadWriteLock m_barCacheRwLock;

    // Two-level storage: timescale → (day → bar vector).
    // Vector indices map to bar slots within a trading day per BarUtils::barsPerDay(tf).
    QMap<TimeFrame, QMap<QDate, QVector<Bar>>> m_barCacheByTimeFrame;

    // Tracks the latest-closed anchor timestamp we already used to force a current-day
    // historical tail backfill, so we don't re-trigger fetches for the same stale state.
    QMap<TimeFrame, QMap<QDate, QDateTime>> m_lastCurrentDayTailBackfillAnchorByTimeFrame;
    QFuture<bool> m_openDatabaseFuture;
};
