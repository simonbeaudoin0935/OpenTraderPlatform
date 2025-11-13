#pragma once

#include "AppFrontend.h"

class TerminalFrontend : public AppFrontend {
    Q_OBJECT
public:
    explicit TerminalFrontend(QObject* parent = nullptr);

public slots:

    // Usage uptade
    void onFMPClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onTSClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onMemoryUsageUpdate(qsizetype newDataUsage) override;
    void onStreamCountUpdate(int count) override;

    void onTradeStationAccountsReceived(QVector<Account> results) override;
    void onNewPositionReceived(QString account, Position position) override;
    void onCurrentHighlightedStockBarReceived(QString symbol, Bar bar) override;
    void onCurrentHighlightedReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP) override;
    void onRequestedMissingBarsDisplayedStockReceived(QVector<Bar>) override;
    void onMarketDepthNotAvailable();

};

