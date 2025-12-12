#pragma once

#include <QLoggingCategory>
#include <QMap>
#include <QReadWriteLock>
#include <QSqlDatabase>
#include <QFuture>

#include "Bar.h"
#include "StreamBars.h"
#include "TSClient.h"

Q_DECLARE_LOGGING_CATEGORY(BarCacheLog)

class BarCache : public QObject
{
    Q_OBJECT

public:
    // Trading hours constants (America/New_York timezone)
    static constexpr int TRADING_START_HOUR = 6;   // 6:00 AM ET
    static constexpr int TRADING_END_HOUR = 20;    // 8:00 PM ET (20:00)
    static constexpr int MONDAY = 1;               // Qt::Monday
    static constexpr int FRIDAY = 5;               // Qt::Friday
    
    explicit BarCache(const QString &symbol, bool isStreaming = false, QObject *parent = nullptr);
    ~BarCache();

    const QString& getSymbol() const { return m_symbol;};

    unsigned int getNumberOfBars() const { return m_barCacheOneMinute.size();};

    bool warmUpBarsOfDayUntilNow(QDate date = QDateTime::currentDateTime().date());

    /*
     * Get Bars between two date-times
     *
     * @note : Both date-times must be in America/New_York timezone
     */
    typedef std::variant<QVector<Bar>, QFuture<QVector<Bar>>> GetBarsResult_t;
    GetBarsResult_t getBars(const QDateTime &first, const QDateTime &last);
    
    QVector<QPair<QDateTime, QDateTime>> identifyMissingRanges(const QDateTime &start, const QDateTime &end, const QVector<Bar>& cachedBars) const;
    QVector<QPair<QDateTime, QDateTime>> splitIntoTradingDayRanges(const QDateTime &first, const QDateTime &last) const;

    qsizetype getDuplicateStoreCount() const { return m_duplicateStoreCount; }

    void clearDatabase();

signals:
    void receivedNewBar(QString symbol, Bar newBar);

private slots:

    void onReceivedNewLiveBar(Bar newBar);

private:

    GetBarsResult_t getBarsInRange(const QDateTime &first, const QDateTime &last);

    void storeBarInCache(const Bar& bar);
    void storeBarsInCache(const QVector<Bar>& bars);
    void storeBarsInDatabase(const QVector<Bar>& bars);

    QVector<Bar> getBarsFromCache(QDateTime start, QDateTime end) const;
    QVector<Bar> getBarsFromDatabase(QDateTime start, QDateTime end) const;

    void handleReceivedAllPendingGetBarsRequests();
    QVector<Bar> fillHolesOfReceivedRequest(const QDateTime& first, const QDateTime& last, const QVector<Bar>& barsFromAPI);


    QString m_symbol;
    bool m_isStreaming;
    QMap<QDateTime, Bar> m_barCacheOneMinute; // This is the in-memory cache of bars
    mutable QReadWriteLock m_barCacheOneMinuteRwLock; // TODO study if this is really needed
    QSqlDatabase m_db;
    StreamBars* m_stream = nullptr;
    mutable quint64 m_duplicateStoreCount = 0;
};
