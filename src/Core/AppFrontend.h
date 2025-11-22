#pragma once

#include <QObject>
#include <QJsonObject>

#include "Account.h"
#include "Position.h"
#include "Bar.h"
#include "MarketDepthQuote.h"

class AppFrontend : public QObject {
    Q_OBJECT
public:
    explicit AppFrontend(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~AppFrontend() = default;

signals:
    void tradeStationAuthStateChanged(bool isAuthenticated, QString reason);
    void tradeStationAccountsReceived(QVector<Account> results);

    void fmpDataUsageUpdated(qsizetype newDataUsage);
    void tradeStationDataUsageUpdated(qsizetype newDataUsage);
    void streamCountUpdated(int count);

    void newPositionReceived(QString account, Position position);

    void currentHighlightedStockBarReceived(QString symbol, Bar bar);
    void currentHighlightedReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP);

    // Coming from the StockPriceChart
    void requestMissingBars(QDateTime viewStartTimeRounded, QDateTime firstBarTime);

public slots:

    // Usage update
    virtual void onFMPClientDataUsageUpdate(qsizetype newDataUsage) = 0;
    virtual void onTSClientDataUsageUpdate(qsizetype newDataUsage) = 0;
    virtual void onMemoryUsageUpdate(qsizetype newDataUsage) = 0;
    virtual void onStreamCountUpdate(int count) = 0;

    virtual void onTradeStationAccountsReceived(QVector<Account> results) = 0;
    virtual void onNewPositionReceived(QString account, Position position) = 0;

    virtual void onCurrentHighlightedStockBarReceived(QString symbol, Bar bar) = 0;
    virtual void onCurrentHighlightedReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP) = 0;
    virtual void onRequestedMissingBarsDisplayedStockReceived(QVector<Bar> bars) = 0;
};
