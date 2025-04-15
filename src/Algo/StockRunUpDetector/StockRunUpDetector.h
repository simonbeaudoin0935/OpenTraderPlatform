#pragma once

#include <QObject>
#include <QDate>
#include "BarCache.h"

struct stats {
    double averageVolumePerBar = 0;
    double averagePriceChangePerBar = 0;
    double maxVolumeChange = 0;
    double maxPriceChange = 0;
    double voidBarsRatio = 0;
    qsizetype nonVoidBars = 0;
};

class StockRunUpDetector : public QObject
{
    Q_OBJECT
public:
    explicit StockRunUpDetector(const QString &symbol, QObject *parent = nullptr);

    void computeStatsOnLastAfterMarket();

public slots:
    void start(QDate startDate);
signals:

private:
    BarCache barCache;
    struct stats afterMarketStats;
    QDate startDate;
    QDate yesterday;
};
