#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QPushButton>

#include "FrontEnd.h"
#include "MainAlgo.h"

// Forward declarations
class PlaceOrderRequest;

// Forward declare the generated UI class
namespace Ui {
class GUIFrontend;
}

class GUIFrontend : public FrontEnd {
    Q_OBJECT
public:
    explicit GUIFrontend(MainAlgo* mainAlgo, QObject* parent = nullptr);
    ~GUIFrontend() override;

public slots:
    void onTSClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onTradeStationAccountsReceived(QVector<Account> results) override;
    void onMemoryUsageUpdate(qsizetype newDataUsage) override;
    void onStreamCountUpdate(int count) override;
    void onCurrentHighlightedStockBarReceived(QString symbol, Bar bar) override;
    void onCurrentHighlightedReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP) override;
    void onNewPositionReceived(QString account, Position position) override;
    void onPositionDeleted(QString account, QString positionID) override;
    void onNewOrderReceived(QString account, Order order) override;
    void onBalanceUpdated(Balance balance) override;

private slots:
    void onTradeStationAuthStateChanged(bool isAuthenticated, QString reason);
    void onNewDisplayedStockSelection();
    void updateLiveLogDisplay(const QString& message);
    void onLoggerVisibilityChanged(bool visible);
    void onLogDepthChanged(int maxLines);
    void onOrderPlaced(const PlaceOrderRequest& order);

private:
    void setupDarkTheme(QMainWindow* mainWindow);
    bool isValidStockSymbol(const QString& symbol) const;
    void displayStock(const QString& symbol);
    void saveLastDisplayedStock(const QString& symbol);
    void restoreLastDisplayedStock();
    MainAlgo *mainAlgo;
    QString currentlyDisplayedSymbol;

    static QString bytesToString(qint64 bytes);

    Ui::GUIFrontend* ui;  // Pointer to the UI object
    QPushButton* tradeStationLoginButton;  // Login button in status bar

    qsizetype TSClientDataUsage = 0;
    qint64 memoryUsage = 0;
    int streamCount = 0;
    
    int maxLiveLogLines = 1000;  // Maximum lines in live log display
    
    bool m_hasRestoredLastStock = false;  // Track if we've restored the last stock
};

