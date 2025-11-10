#pragma once

#include <QLoggingCategory>
#include <QMap>
#include <QObject>
#include <QReadWriteLock>
#include <QSqlDatabase>

#include "Bar.h"
#include "StreamBars.h"

Q_DECLARE_LOGGING_CATEGORY(BarCacheLog)

class BarCache : public QObject
{
    Q_OBJECT
public:
    explicit BarCache(const QString &symbol, bool isStreaming = false, QObject *parent = nullptr);
    ~BarCache();

    const QString& getSymbol() const { return symbol;};

    unsigned int getNumberOfBars() const { return barCacheOneMinute.size();};

    bool warmUpBarsOfDayUntilNow(QDate date = QDateTime::currentDateTime().date());

    const QVector<Bar> getBars(const QDateTime &first, const QDateTime &last);
    const QVector<Bar> getAfterHourBars(const QDate &date);

    void clearDatabase();

    QVector<QPair<QDateTime, QDateTime>> identifyMissingRanges(QDateTime start, QDateTime end, const QVector<Bar>& cachedBars) const;

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
    void onReceivedNewBar(QString symbol, Bar newBar);

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
