#pragma once
#include <QLoggingCategory>
#include <QObject>
#include <QFile>

#include "StockScreener/StockScreener.h"
#include "BreakingNewsFetcher/BreakingNewsFetcher.h"
#include "StockBarsReceiver/StockBarsReceiver.h"
#include "MarketDepthQuoteReceiver/MarketDepthQuoteReceiver.h"
#include "PositionsReceiver/PositionsReceiver.h"
#include "Clients/TSClient/Brokerage/GetAccounts/Account.h"

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
    void currentHighlightedReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote);
    void receivedNewPosition(QString account, Position position);

public slots:
    void onTradeStationAuthStateChanged(bool isAuthenticated, QString reason);

private slots:
    void onThreadStarted();

    void onStockScreenerFinished();
    void onNewNewsFound(StockNewsResult newNews);
    void onReceivedNewPosition(QString account, Position position);
    void onTradeStationAccountsReceived(QVector<Account> results);


private:
    QThread thread;

    QVector<Account> accounts;

    StockScreener stockScreener;
    BreakingNewsFetcher breakingNewsFetcher;
    StockBarsReceiver stockBarsReceiver;
    MarketDepthQuoteReceiver marketDepthQuoteReceiver;
    PositionsReceiver positionReceiver;

    QTextStream *algoLogFile;
    QFile file;
};
