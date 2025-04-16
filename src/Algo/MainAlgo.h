#pragma once
#include <QLoggingCategory>
#include <QObject>
#include <QFile>
#include <QThread>
#include <QMap>

#include "RunUpDetector.h"
#include "StockScreener.h"
#include "BreakingNewsFetcher.h"
#include "MarketDepthQuoteReceiver.h"
#include "PositionsReceiver.h"
#include "Account.h"
#include "BarCache.h"

Q_DECLARE_LOGGING_CATEGORY(MainAlgoLog)


class StockInstruments : public QObject{

public:
    explicit StockInstruments(const QString &symbol);
    ~StockInstruments();

    QString symbol;
    BarCache barCache;
    RunUpDetector runUpDetector;
    MarketDepthQuoteReceiver marketDepthQuoteReceiver;
};

class MainAlgo : public QObject
{
    Q_OBJECT
public:
    MainAlgo();

    void start();

signals:
    void displayedStockReceivedNewBar(QString symbol, Bar bar);
    void displayedStockReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP);
    void requestedMissingBarsDisplayedStockReceived(QVector<Bar>);

    void receivedNewPosition(QString account, Position position);

public slots:
    void onTradeStationAuthStateChanged(bool isAuthenticated, QString reason);
    void onSelectDisplayedStock(QString symbol);
    void onRequestMissingBarsDisplayedStock(QDateTime first, QDateTime last);

private slots:
    void onThreadStarted();

    void onStockScreenerFinished();
    void onNewNewsFound(StockNewsResult newNews);
    void onReceivedNewPosition(QString account, Position position);

private:
    QThread thread;

    QVector<Account> accounts;
    QMap<QString, StockInstruments*> stockInstruments;
    StockInstruments* currentDisplayedStock = nullptr;

    StockScreener stockScreener;
    BreakingNewsFetcher breakingNewsFetcher;


    PositionsReceiver positionReceiver;

    QTextStream *algoLogFile;
    QFile file;
};
