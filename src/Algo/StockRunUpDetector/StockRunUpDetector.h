#pragma once

#include <QObject>

#include "BarCache.h"

struct stats {
    double averageVolumePerBar = 0;
    double averagePriceChangePerBar = 0;
    double maxVolumeChange = 0;
    double maxPriceChange = 0;
};

class StockRunUpDetector : public QObject
{
    Q_OBJECT
public:
    explicit StockRunUpDetector(QObject *parent = nullptr);

    void computeStatsOnLastAfterMarket();

public slots:
    void start(QVector<QString> &watchlist);
signals:

private:
    QMap<QString, BarCache*> watchlistBarCache;
    QMap<QString, struct stats> statsPerSymbol;
};
