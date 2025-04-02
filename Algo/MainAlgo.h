#pragma once
#include <QLoggingCategory>
#include <QObject>
#include <QFile>

#include "StockScreener/StockScreener.h"
#include "BreakingNewsFetcher/BreakingNewsFetcher.h"

Q_DECLARE_LOGGING_CATEGORY(MainAlgoLog)


class QThread;

class MainAlgo : public QObject
{
    Q_OBJECT
public:
    MainAlgo();

    void start();

private slots:
    void onThreadStarted();

    void onStockScreenerFinished();
    void onNewNewsFound(StockNewsResult newNews);

private:
    QThread thread;

    StockScreener stockScreener;
    BreakingNewsFetcher breakingNewsFetcher;

    QTextStream *algoLogFile;
    QFile file;
};
