#pragma once

#include <QObject>
#include <QJsonObject>

#include "Account.h"
#include "Position.h"
#include "Order.h"
#include "Bar.h"
#include "Level2.h"
#include "Trade.h"
#include "Balance.h"
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
    void databentoDataUsageUpdated(qsizetype newDataUsage);

    void newPositionReceived(QString account, Position position);
    void positionDeleted(QString account, QString positionID);
    void newOrderReceived(QString account, Order order);
    void balanceUpdated(Balance balance);

    void currentHighlightedStockBarReceived(QString symbol, Bar bar);
    void currentHighlightedReceivedNewLevel2(QString symbol, Level2 level2);
    void currentHighlightedReceivedNewTrade(QString symbol, Trade trade);

  public slots:

    // Usage update
    virtual void onTSClientDataUsageUpdate(qsizetype newDataUsage) = 0;
    virtual void onDBClientDataUsageUpdate(qsizetype newDataUsage) = 0;
    virtual void onMemoryUsageUpdate(qsizetype newDataUsage) = 0;

    virtual void onTradeStationAccountsReceived(QVector<Account> results) = 0;
    virtual void onNewPositionReceived(QString account, Position position) = 0;
    virtual void onPositionDeleted(QString account, QString positionID) = 0;
    virtual void onNewOrderReceived(QString account, Order order) = 0;
    virtual void onBalanceUpdated(Balance balance) = 0;

    virtual void onCurrentHighlightedStockBarReceived(QString symbol, Bar bar) = 0;
    virtual void onCurrentHighlightedReceivedNewLevel2(QString symbol, Level2 level2) = 0;
    virtual void onCurrentHighlightedReceivedNewTrade(QString /*symbol*/, Trade /*trade*/) {}

    // Replay mode notifications
    virtual void onReplayModeEntered() = 0;
    virtual void onReplayModeExited() = 0;
    virtual void onReplayTimeUpdated(QDateTime currentTime) = 0;
};
