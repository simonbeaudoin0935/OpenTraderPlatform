#pragma once

#include <QObject>
#include <QDate>
#include <QQueue>
#include <QTimeZone>
#include "BarCache.h"

struct stats
{
    double averageVolumePerBar = 0;
    double averagePriceChangePerBar = 0;
    double maxVolumeChange = 0;
    double maxPriceChange = 0;
    double voidBarsRatio = 0;
    qsizetype nonVoidBars = 0;
};

class RunUpDetector : public QObject
{
    Q_OBJECT
  public:
    explicit RunUpDetector(BarCache* p_barCache, QObject* parent = nullptr);

    void computeStatsOnLastAfterMarket();

    void computeNextCandle();

  public slots:
    void start(QDate p_startDate, qsizetype p_runUpWindowWidth = 20);
  signals:

  private:
    const QTimeZone NYTZ;

    BarCache* barCache;
    QQueue<Bar> deque;
    struct stats afterMarketStats;
    QDate startDate;
    QDate yesterday;
    qsizetype runUpWindowWidth;

    QDateTime timestampLastBarEnqued;

    void detectRunUp();
    double calculateRSI(int period);

    QVector<QDateTime> runUps;
};
