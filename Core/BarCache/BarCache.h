#pragma once

#include <QMap>
#include <QObject>
#include <QReadWriteLock>

#include "../../Clients/TSClient/MarketData/Bars/Bar.h"
#include "../../Clients/TSClient/MarketData/Bars/GetBars.h"
#include "../../Clients/TSClient/MarketData/Bars/StreamBars.h"

class BarCache : public QObject
{
    Q_OBJECT
public:
    explicit BarCache(QString &symbol, QObject *parent = nullptr);

    const Bar* getBar(QDateTime dateTime) const;
signals:

protected:
    void fetchBars();
    void addBar(const Bar& bar);

private:
    QString symbol;
    mutable QReadWriteLock rwLock;
    QMap<QDateTime, Bar> barCache;
    StreamBars* streamBar;
};
