#pragma once

#include <QObject>
#include <QJsonObject>

#include "Account.h"
#include "Position.h"
#include "Order.h"
#include "Bar.h"
#include "MarketDepthQuote.h"
#include "Balance.h"
#include "Quote.h"
#include "TSClient.h" // For TSClient::AuthStateReason enum

class FrontEnd : public QObject
{
    Q_OBJECT
  public:
    explicit FrontEnd(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~FrontEnd() = default;

  signals:
    /**
     * @brief Frontend signals (forwarded from backend components to GUI/TUI)
     * Thread context: Emitted from Main/GUI thread
     * 
     * Data flow: Backend threads (TSClient, MainAlgo) → MainApp (queued) → FrontEnd → GUI widgets
     * Note: All GUI updates must occur on the main thread per Qt requirements
     */
    void tradeStationAuthStateChanged(bool isAuthenticated, TSClient::AuthStateReason reason, QString message);
    void tradeStationAccountsReceived(QVector<Account> results);

    void tradeStationDataUsageUpdated(qsizetype newDataUsage);
    void streamCountsUpdated(size_t barsCount, size_t marketDepthCount);

    void newPositionReceived(QString account, Position position);
    void positionDeleted(QString account, QString positionID);
    void newOrderReceived(QString account, Order order);
    void balanceUpdated(Balance balance);

    void currentHighlightedStockBarReceived(QString symbol, Bar bar);
    void currentHighlightedReceivedNewMarketDepthQuote(QString symbol,
                                                       MarketDepthQuote quote,
                                                       double bidAskImbalance,
                                                       double bidDWP,
                                                       double askDWP);
    void currentHighlightedReceivedNewQuote(QString symbol, Quote quote);

  public slots:

    // Usage update
    virtual void onTSClientDataUsageUpdate(qsizetype newDataUsage) = 0;
    virtual void onMemoryUsageUpdate(qsizetype newDataUsage) = 0;
    virtual void onStreamCountUpdate(size_t barsCount, size_t marketDepthCount) = 0;

    virtual void onTradeStationAccountsReceived(QVector<Account> results) = 0;
    virtual void onNewPositionReceived(QString account, Position position) = 0;
    virtual void onPositionDeleted(QString account, QString positionID) = 0;
    virtual void onNewOrderReceived(QString account, Order order) = 0;
    virtual void onBalanceUpdated(Balance balance) = 0;

    virtual void onCurrentHighlightedStockBarReceived(QString symbol, Bar bar) = 0;
    virtual void onCurrentHighlightedReceivedNewMarketDepthQuote(QString symbol,
                                                                 MarketDepthQuote quote,
                                                                 double bidAskImbalance,
                                                                 double bidDWP,
                                                                 double askDWP) = 0;
    virtual void onCurrentHighlightedReceivedNewQuote(QString symbol, Quote quote) = 0;

    // Replay mode notifications
    virtual void onReplayModeEntered() = 0;
    virtual void onReplayModeExited() = 0;
    virtual void onReplayTimeUpdated(QDateTime currentTime) = 0;
};
