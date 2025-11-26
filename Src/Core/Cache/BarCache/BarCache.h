#pragma once

#include <QLoggingCategory>
#include <QMap>
#include <QObject>
#include <QReadWriteLock>
#include <QSqlDatabase>

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

    const QString& getSymbol() const { return symbol;};

    unsigned int getNumberOfBars() const { return barCacheOneMinute.size();};

    bool warmUpBarsOfDayUntilNow(QDate date = QDateTime::currentDateTime().date());

    const QVector<Bar> getBars(const QDateTime &first, const QDateTime &last);
    
    const QVector<Bar> getAfterHourBars(const QDate &date);

    void clearDatabase();

    QVector<QPair<QDateTime, QDateTime>> identifyMissingRanges(const QDateTime &start, const QDateTime &end, const QVector<Bar>& cachedBars) const;
    QVector<QPair<QDateTime, QDateTime>> splitIntoTradingDayRanges(const QDateTime &first, const QDateTime &last) const;

    enum class HitType{
        None,
        Hit,
        Miss,
        PartialHit
    };

    HitType getLastHitType() const { return lastHitType; }

    qsizetype getDuplicateStoreCount() const { return duplicateStoreCount; }
    qsizetype getLastNumberFetchedBars() const { return lastNumberFetchedBars; }

signals:
    void receivedNewBar(QString symbol, Bar newBar);

private slots:

    static void onReceivedAsyncGetBars(size_t requestID, RESTClient::RequestStatus status, QString symbol, QVector<Bar> bars);
    void onReceivedNewLiveBar(QString symbol, Bar newBar);
    void onStreamError(Stream::StreamError error, QString errorMessage);

private:
    QString symbol;
    bool isStreaming;
    QMap<QDateTime, Bar> barCacheOneMinute;
    mutable QReadWriteLock rwLock;

    QSqlDatabase db;

    void storeBarInCache(const Bar& bar);
    void storeBarsInCache(const QVector<Bar>& bars);

    QVector<Bar> getBarsFromCache(QDateTime start, QDateTime end) const;
    QVector<Bar> getBarsFromDatabase(QDateTime start, QDateTime end) const;
    void storeBarsInDatabase(const QVector<Bar>& bars);

    StreamBars* streamBar = nullptr;

    HitType lastHitType = HitType::None;

    mutable quint64 duplicateStoreCount = 0;
    mutable quint64 lastNumberFetchedBars = 0;
};
