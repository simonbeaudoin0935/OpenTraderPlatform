#pragma once

#include <QObject>
#include <QJsonObject>

#include "Account.h"
#include "Position.h"
#include "Order.h"
#include "Bar.h"
#include "MarketDepthQuote.h"
#include "Balance.h"

class FrontEnd : public QObject
{
    Q_OBJECT
  public:
    explicit FrontEnd(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~FrontEnd() = default;

  signals:
    /**
     * @brief Frontend signals (forwarded from backend components to GUI/TUI)
     * 
     * Thread Safety (Architecture_Improvements.md point 1.4):
     * - Emitted from: Main/GUI thread (FrontEnd runs on main thread)
     * - Received on: Main/GUI thread (same thread, typically uses Qt::DirectConnection)
     * - Thread-safe: Yes (receives data from MainAlgo/TSClient via Qt::QueuedConnection)
     * 
     * Data Flow Pattern:
     * 1. Backend components (TSClient, MainAlgo) emit signals from their worker threads
     * 2. MainApp receives signals via Qt::QueuedConnection (cross-thread, queued)
     * 3. MainApp forwards to FrontEnd (same main thread)
     * 4. FrontEnd emits to GUI widgets (same main thread)
     * 
     * Note: All GUI updates must occur on the main thread per Qt requirements
     */
    void tradeStationAuthStateChanged(bool isAuthenticated, QString reason);
    void tradeStationAccountsReceived(QVector<Account> results);

    void tradeStationDataUsageUpdated(qsizetype newDataUsage);
    void streamCountUpdated(int count);

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

  public slots:

    // Usage update
    virtual void onTSClientDataUsageUpdate(qsizetype newDataUsage) = 0;
    virtual void onMemoryUsageUpdate(qsizetype newDataUsage) = 0;
    virtual void onStreamCountUpdate(int count) = 0;

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

    // Replay mode notifications
    virtual void onReplayModeEntered() = 0;
    virtual void onReplayModeExited() = 0;
    virtual void onReplayTimeUpdated(QDateTime currentTime) = 0;
};
