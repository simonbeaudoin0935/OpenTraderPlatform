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
#include "Ticker.h"

Q_DECLARE_LOGGING_CATEGORY(MainAlgoLog)


class StockInstruments : public QObject{

public:
    explicit StockInstruments(const Ticker &symbol);
    ~StockInstruments();

    Ticker symbol;
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
    void displayedStockReceivedNewBar(Ticker symbol, Bar bar);
    void displayedStockReceivedNewMarketDepthQuote(Ticker symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP);
    void requestedMissingBarsDisplayedStockReceived(QVector<Bar>);

    void receivedNewPosition(QString account, Position position);

public slots:
    void onTradeStationAuthStateChanged(bool isAuthenticated, QString reason);
    void onSelectDisplayedStock(Ticker symbol);
    void onRequestMissingBarsDisplayedStock(QDateTime first, QDateTime last);

private slots:
    void onThreadStarted();

    void onStockScreenerFinished();
    void onNewNewsFound(StockNewsResult newNews);
    void onReceivedNewPosition(QString account, Position position);

private:
    QThread thread;

    QVector<Account> accounts;
    QMap<Ticker, StockInstruments*> stockInstruments;
    StockInstruments* currentDisplayedStockInstrument = nullptr;

    StockScreener stockScreener;
    BreakingNewsFetcher breakingNewsFetcher;


    PositionsReceiver positionReceiver;
    bool positionStreamStarted = false;

    QTextStream *algoLogFile;
    QFile file;
};
