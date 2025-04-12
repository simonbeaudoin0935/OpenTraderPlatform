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
    quint64 getCacheHitCount() const { return cacheHitCount; }
    quint64 getCacheMissCount() const { return cacheMissCount; }

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
    mutable quint64 cacheHitCount = 0;
    mutable quint64 cacheMissCount = 0;
};
