#pragma once

#include <QObject>
#include <QJsonObject>
#include "Clients/TSClient/Brokerage/GetAccounts/Account.h"
#include "Clients/TSClient/Brokerage/StreamPositions/Position.h"
#include "Clients/TSClient/MarketData/StreamBars/Bar.h"
#include "Clients/TSClient/MarketData/StreamMarketDepthQuote/MarketDepthQuote.h"

class AppFrontend : public QObject {
    Q_OBJECT
public:
    explicit AppFrontend(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~AppFrontend() = default;

signals:
    void tradeStationAuthStateChanged(bool isAuthenticated, QString reason);
    void tradeStationAccountsReceived(QVector<Account> results);
    void marketDepthNotAvailable();

    void fmpDataUsageUpdated(qsizetype newDataUsage);
    void tradeStationDataUsageUpdated(qsizetype newDataUsage);

    void newPositionReceived(QString account, Position position);

    void currentHighlightedStockBarReceived(QString symbol, Bar bar);
    void currentHighlightedReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP);

public slots:

    // Usage uptade
    virtual void onFMPClientDataUsageUpdate(qsizetype newDataUsage) = 0;
    virtual void onTSClientDataUsageUpdate(qsizetype newDataUsage) = 0;
    virtual void onMemoryUsageUpdate(qint64 newDataUsage) = 0;

    virtual void onTradeStationAccountsReceived(QVector<Account> results) = 0;
    virtual void onMarketDepthNotAvailable() = 0;
    virtual void onNewPositionReceived(QString account, Position position) = 0;

    virtual void onCurrentHighlightedStockBarReceived(QString symbol, Bar bar) = 0;
    virtual void onCurrentHighlightedReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP) = 0;
};
