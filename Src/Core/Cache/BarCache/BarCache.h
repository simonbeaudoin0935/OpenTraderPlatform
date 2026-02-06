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

    mutable QReadWriteLock m_barCacheRwLock; // Protects m_barCacheByDay, mutable for use in const methods
    // Day-based storage: one QVector per trading day. Vector index maps to minute within trading day.
    QMap<QDate, QVector<Bar>> m_barCacheByDay;
};
