#pragma once
#include <QLoggingCategory>
#include <QObject>
#include <QFile>

#include "StockScreener/StockScreener.h"
#include "BreakingNewsFetcher/BreakingNewsFetcher.h"
#include "StockBarsReceiver/StockBarsReceiver.h"

Q_DECLARE_LOGGING_CATEGORY(MainAlgoLog)


class QThread;

class MainAlgo : public QObject
{
    Q_OBJECT
public:
    MainAlgo();

    void start();

signals:
    void currentHighlightedReceivedNewBar(QString symbol, Bar bar);

private slots:
    void onThreadStarted();

    void onStockScreenerFinished();
    void onNewNewsFound(StockNewsResult newNews);

private:
    QThread thread;

    StockScreener stockScreener;
    BreakingNewsFetcher breakingNewsFetcher;
    StockBarsReceiver stockBarsReceiver;

    QTextStream *algoLogFile;
    QFile file;
};
