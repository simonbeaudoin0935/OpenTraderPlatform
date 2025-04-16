#pragma once

#include <QLoggingCategory>
#include <QMap>
#include <QObject>
#include <QReadWriteLock>

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

    bool warmUpBars(QDateTime first, QDateTime last);

    const QVector<Bar> getBars(const QDateTime &first, const QDateTime &last);
    const QVector<Bar> getAfterHourBars(const QDate &date);

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

    void storeBarInCache(const Bar& bar);
    void storeBarsInCache(const QVector<Bar>& bars);

    QVector<Bar> getBarsFromCache(QDateTime start, QDateTime end) const;
    StreamBars* streamBar = nullptr;

    HitType lastHitType = HitType::None;

    mutable quint64 duplicateStoreCount = 0;
    mutable quint64 lastNumberFetchedBars = 0;
};
