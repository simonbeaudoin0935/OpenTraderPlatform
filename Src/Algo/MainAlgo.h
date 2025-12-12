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
#include "OrdersReceiver.h"
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

class MainAlgo final : public QObject
{
    Q_OBJECT
public:
    // Singleton : Instance getter  and delete copy and assignment
    static MainAlgo* getInstance();
    MainAlgo(const MainAlgo&) = delete;
    MainAlgo& operator=(const MainAlgo&) = delete;

    void start();

    void startBalancePolling();
    void stopBalancePolling();
    [[nodiscard]] Balance getCurrentBalance() const;

    BarCache::GetBarsResult_t requestMissingBarsDisplayedStock(QDateTime first, QDateTime last);

signals:
    void displayedStockReceivedNewBar(QString symbol, Bar bar);
    void displayedStockReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP);

    void receivedNewPosition(QString account, Position position);
    void receivedNewOrder(QString account, Order order);
    void tradeStationAccountsReceived(QVector<Account> accounts);
    void balanceUpdated(Balance balance);

public slots:
    void onTradeStationAuthStateChanged(bool isAuthenticated, QString reason);
    void onSelectDisplayedStock(QString symbol);

private slots:
    void onThreadStarted();

    void onReceivedNewPosition(QString account, Position position);
    void onReceivedNewOrder(QString account, Order order);

    void onReceivedAsyncGetAccounts(const QVector<Account>& results);

    void onBalanceReceived(const QVector<Balance>& results);
    void requestBalance();


private:
    static MainAlgo* m_instance;
     explicit MainAlgo(); // Singleton : private constructor
    ~MainAlgo();

    QThread thread;

    QMap<QString, StockInstruments*> stockInstruments;
    StockInstruments* currentDisplayedStockInstrument = nullptr;

    PositionsReceiver* m_positionReceiver = nullptr;
    OrdersReceiver* m_orderReceiver = nullptr;
    bool positionStreamStarted = false;
    bool orderStreamStarted = false;

    QTextStream *algoLogFile;
    QFile file;

    bool m_havePastSuccessfulExchanges = false;

    Account m_activeAccount;
    Balance m_currentBalance;

    QTimer* m_balancePollingTimer;

    bool m_balancePollingStarted = false;
};
