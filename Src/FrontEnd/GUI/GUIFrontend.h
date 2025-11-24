#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QPushButton>

#include "FrontEnd.h"
#include "MainAlgo.h"

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
    void onFMPClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onTSClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onTradeStationAccountsReceived(QVector<Account> results) override;
    void onMemoryUsageUpdate(qsizetype newDataUsage) override;
    void onStreamCountUpdate(int count) override;
    void onCurrentHighlightedStockBarReceived(QString symbol, Bar bar) override;
    void onCurrentHighlightedReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP) override;
    void onNewPositionReceived(QString account, Position position) override;
    void onRequestedMissingBarsDisplayedStockReceived(QVector<Bar> bars) override;

private slots:
    void onTradeStationLoginClicked();
    void onTradeStationAuthStateChanged(bool isAuthenticated, QString reason);
    void onNewDisplayedStockSelection();
    void updateLiveLogDisplay(const QString& message);
    void onLoggerVisibilityChanged(bool visible);
    void onLogDepthChanged(int maxLines);

private:
    void setupDarkTheme(QMainWindow* mainWindow);
    bool isValidStockSymbol(const QString& symbol) const;
    MainAlgo *mainAlgo;
    QString currentlyDisplayedSymbol;

    static QString bytesToString(qint64 bytes);

    Ui::GUIFrontend* ui;  // Pointer to the UI object
    QPushButton* tradeStationLoginButton;  // Login button in status bar

    qsizetype FMPClientDataUsage = 0;
    qsizetype TSClientDataUsage = 0;
    qint64 memoryUsage = 0;
    int streamCount = 0;
    
    int maxLiveLogLines = 1000;  // Maximum lines in live log display
};

