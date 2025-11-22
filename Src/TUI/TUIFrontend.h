#pragma once

#include "AppFrontend.h"
#include "Ticker.h"

// Forward declaration
class MainAlgo;

class TUIFrontend : public AppFrontend {
    Q_OBJECT
public:
    explicit TUIFrontend(MainAlgo* mainAlgo, QObject* parent = nullptr);

public slots:

    // Usage update
    void onFMPClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onTSClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onMemoryUsageUpdate(qsizetype newDataUsage) override;
    void onStreamCountUpdate(int count) override;

    void onTradeStationAccountsReceived(QVector<Account> results) override;
    void onNewPositionReceived(QString account, Position position) override;
    void onCurrentHighlightedStockBarReceived(Ticker symbol, Bar bar) override;
    void onCurrentHighlightedReceivedNewMarketDepthQuote(Ticker symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP) override;
    void onRequestedMissingBarsDisplayedStockReceived(QVector<Bar> bars) override;

private:
    MainAlgo* mainAlgo;
};
