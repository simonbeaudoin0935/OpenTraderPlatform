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

    const QVector<Bar> getBars(QDateTime first, QDateTime last);

    enum class HitType{
        None,
        Hit,
        Miss,
        PartialHit
    };

    HitType getLastHitType() const { return lastHitType; }

    quint64 getDuplicateStoreCount() const { return duplicateStoreCount; }
    quint64 getLastNumberFetchedBars() const { return lastNumberFetchedBars; }

signals:

protected:
    void fetchBars();

private slots:
    void onGetBarsReceived(QString symbol, QVector<Bar> newBars);

private:
    QString symbol;
    bool isStreaming;
    QMap<QDateTime, Bar> barCacheOneMinute;
    mutable QReadWriteLock rwLock;
    void storeBarsInCache(const QVector<Bar>& bars);
    QVector<Bar> getBarsFromCache(QDateTime start, QDateTime end) const;
    StreamBars* streamBar;

    HitType lastHitType = HitType::None;

    mutable quint64 duplicateStoreCount = 0;
    mutable quint64 lastNumberFetchedBars = 0;
};
