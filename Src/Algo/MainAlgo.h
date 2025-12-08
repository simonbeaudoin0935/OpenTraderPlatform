#pragma once
#include <QLoggingCategory>
#include <QObject>
#include <QFile>
#include <QThread>
#include <QMap>
#include <QTimer>
#include <QVector>

#include "RunUpDetector.h"
#include "MarketDepthQuoteReceiver.h"
#include "PositionsReceiver.h"
#include "Account.h"
#include "BarCache.h"
#include "Balance.h"

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
    ~MainAlgo();

    void start();

    void startBalancePolling();
    void stopBalancePolling();
    [[nodiscard]] Balance getCurrentBalance() const;

signals:
    void displayedStockReceivedNewBar(QString symbol, Bar bar);
    void displayedStockReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP);
    void requestedMissingBarsDisplayedStockReceived(QVector<Bar>);

    void receivedNewPosition(QString account, Position position);
    void tradeStationAccountsReceived(QVector<Account> accounts);
    void balanceUpdated(Balance balance);

public slots:
    void onTradeStationAuthStateChanged(bool isAuthenticated, QString reason);
    void onSelectDisplayedStock(QString symbol);
    void onRequestMissingBarsDisplayedStock(QDateTime first, QDateTime last);

private slots:
    void onThreadStarted();

    void onReceivedNewPosition(QString account, Position position);

    void onReceivedAsyncGetAccounts(TSClient::AsyncRequestID_t requestID, TSClient::AsyncRequestStatus_e status, QVector<Account> results);

    void onBalanceReceived(TSClient::AsyncRequestID_t requestID, TSClient::AsyncRequestStatus_e status, QVector<Balance> results);
    void requestBalance();


private:
    QThread thread;

    QMap<QString, StockInstruments*> stockInstruments;
    StockInstruments* currentDisplayedStockInstrument = nullptr;

    PositionsReceiver positionReceiver;
    bool positionStreamStarted = false;

    QTextStream *algoLogFile;
    QFile file;

    TSClient::AsyncRequestID_t m_savedGetAccountsRequestID;
    bool m_havePastSuccessfulExchanges = false;

    Account m_activeAccount;
    Balance m_currentBalance;

    QTimer* m_balancePollingTimer;
    TSClient::AsyncRequestID_t m_savedGetBalancesRequestID;
    bool m_balancePollingStarted = false;
};
