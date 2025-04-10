#pragma once
#include <QLoggingCategory>
#include <QObject>
#include <QFile>

#include "StockRunUpDetector.h"
#include "StockScreener.h"
#include "BreakingNewsFetcher.h"
#include "StockBarsReceiver.h"
#include "MarketDepthQuoteReceiver.h"
#include "PositionsReceiver.h"
#include "Account.h"

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
    void currentHighlightedReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP);
    void receivedNewPosition(QString account, Position position);

public slots:
    void onTradeStationAuthStateChanged(bool isAuthenticated, QString reason);

private slots:
    void onThreadStarted();

    void onStockScreenerFinished();
    void onNewNewsFound(StockNewsResult newNews);
    void onReceivedNewPosition(QString account, Position position);

private:
    QThread thread;

    QVector<Account> accounts;

    StockRunUpDetector stockRunUpDetector;
    StockScreener stockScreener;
    BreakingNewsFetcher breakingNewsFetcher;
    StockBarsReceiver stockBarsReceiver;
    MarketDepthQuoteReceiver marketDepthQuoteReceiver;
    PositionsReceiver positionReceiver;

    QTextStream *algoLogFile;
    QFile file;
};
