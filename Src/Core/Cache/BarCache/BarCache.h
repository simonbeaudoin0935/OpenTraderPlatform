#pragma once

#include <QLoggingCategory>
#include <QMap>
#include <QReadWriteLock>
#include <QFuture>

#include <expected>

#include "Bar.h"
#include "StreamBars.h"
#include "TSClient.h"

Q_DECLARE_LOGGING_CATEGORY(BarCacheLog)

class BarCache : public QObject
{
    Q_OBJECT

  public:
    explicit BarCache(const QString& symbol, bool isStreaming = false, QObject* parent = nullptr);
    ~BarCache();

    const QString& getSymbol() const
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

    // Converts a QTime timestamp to the corresponding index in the daily bar cache vector
    static size_t timeToIndex(const QTime& time);

    // Converts a daily bar cache index to the corresponding bar timestamp (QTime)
    static QTime indexToTime(size_t index);

    // Trading hours constants (America/New_York timezone)
    static inline const QTime TRADING_START_TIME = QTime(6, 1); // 6:01 AM ET
    static inline const QTime TRADING_END_TIME = QTime(20, 0);  // 8:00 PM ET (20:00)

    static constexpr unsigned int BARS_PER_DAY = 840; // From 6:01 AM to 8:00 PM, 1-minute bars

  signals:
    void receivedNewBar(QString symbol, Bar newBar);

  private slots:

    void onReceivedNewLiveBar(Bar newBar);

  private:
    void startStream();

    [[nodiscard]]
    static constexpr QVector<std::tuple<QDate, QTime, QTime>> splitIntoTradingDayRanges(const QDateTime& first,
                                                                                        const QDateTime& last);

    std::optional<std::unique_ptr<QVector<Bar>>>
    getBarsFromCache(const QDate& date, const QTime& start, const QTime& end) const;

    void storeBarInCache(const Bar& bar);

    void storeBarsInCache(const QDate& date, std::shared_ptr<QVector<Bar>> bars);

    void handleReceivedAllPendingGetBarsRequests();
    QVector<Bar>
    fillHolesOfReceivedRequest(const QDateTime& first, const QDateTime& last, const QVector<Bar>& barsFromAPI) const;

    QVector<Bar>& getOrCreateDayVector(const QDate& date);

    const QString m_symbol;
    const bool m_isStreaming;
    QString m_dbPath; // Path to the database file (managed by DatabaseThread)
    QPointer<StreamBars> m_stream;

    mutable QReadWriteLock m_barCacheRwLock; // Protects m_barCacheByDay, mutable for use in const methods
    // Day-based storage: one QVector per trading day. Vector index maps to minute within trading day.
    QMap<QDate, QVector<Bar>> m_barCacheByDay;
};
