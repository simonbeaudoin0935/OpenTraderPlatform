#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QPushButton>

#include "AppFrontend.h"
#include "MainAlgo.h"

// Forward declare the generated UI class
namespace Ui {
class GuiFrontend;
}

class GuiFrontend : public AppFrontend {
    Q_OBJECT
public:
    explicit GuiFrontend(MainAlgo* mainAlgo, QObject* parent = nullptr);
    ~GuiFrontend() override;

public slots:
    void onFMPClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onTSClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onTradeStationAccountsReceived(QVector<Account> results) override;
    void onMemoryUsageUpdate(qsizetype newDataUsage) override;
    void onCurrentHighlightedStockBarReceived(QString symbol, Bar bar) override;
    void onCurrentHighlightedReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP) override;
    void onNewPositionReceived(QString account, Position position) override;
    void onRequestedMissingBarsDisplayedStockReceived(QVector<Bar>) override;

private slots:
    void onTradeStationLoginClicked();
    void onTradeStationAuthStateChanged(bool isAuthenticated, QString reason);
    void onNewDisplayedStockSelection();
    void updateLiveLogDisplay(const QString& message);

private:
    void setupDarkTheme(QMainWindow* mainWindow);
    MainAlgo *mainAlgo;
    QString currentlyDisplayedSymbol;

    static QString bytesToString(qint64 bytes);

    Ui::GuiFrontend* ui;  // Pointer to the UI object
    QPushButton* tradeStationLoginButton;  // Login button in status bar

    qsizetype FMPClientDataUsage = 0;
    qsizetype TSClientDataUsage = 0;
    qint64 memoryUsage = 0;
    
    int maxLiveLogLines = 1000;  // Maximum lines in live log display
};

