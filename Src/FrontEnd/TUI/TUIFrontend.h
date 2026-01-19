#pragma once

#include "FrontEnd.h"

// Forward declaration
class MainAlgo;

class TUIFrontend : public FrontEnd
{
    Q_OBJECT
  public:
    explicit TUIFrontend(MainAlgo* p_mainAlgo, QObject* parent = nullptr);

  public slots:

    // Usage update
    void onTSClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onMemoryUsageUpdate(qsizetype newDataUsage) override;
    void onStreamCountUpdate(int count) override;

    void onTradeStationAccountsReceived(QVector<Account> results) override;
    void onNewPositionReceived(QString account, Position position) override;
    void onPositionDeleted(QString account, QString positionID) override;
    void onNewOrderReceived(QString account, Order order) override;
    void onBalanceUpdated(Balance balance) override;
    void onCurrentHighlightedStockBarReceived(QString symbol, Bar bar) override;
    void onCurrentHighlightedReceivedNewMarketDepthQuote(QString symbol,
                                                         MarketDepthQuote quote,
                                                         double bidAskImbalance,
                                                         double bidDWP,
                                                         double askDWP) override;

  private:
    MainAlgo* mainAlgo;
};
