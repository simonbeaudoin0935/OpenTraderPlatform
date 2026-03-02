#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QPushButton>
#include <QShortcut>
#include <QKeySequence>
#include <QLabel>
#include <QEvent>
#include <memory>

#include "FrontEnd.h"
#include "MainAlgo.h"
#include "Misc/ShortcutSettings.h"

// Forward declarations
class PlaceOrderRequest;

// Forward declare the generated UI class
namespace Ui
{
    class GUIFrontend;
}

Q_DECLARE_LOGGING_CATEGORY(GUIFrontendLog)

class GUIFrontend : public FrontEnd
{
    Q_OBJECT
  public:
    explicit GUIFrontend(MainAlgo* p_mainAlgo, QObject* parent = nullptr);
    ~GUIFrontend() override;

  public slots:
    void onTSClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onDBClientDataUsageUpdate(qsizetype newDataUsage) override;
    void onTradeStationAccountsReceived(QVector<Account> results) override;
    void onMemoryUsageUpdate(qsizetype newDataUsage) override;
    void onCurrentHighlightedStockBarReceived(QString symbol, Bar bar) override;
    void onCurrentHighlightedReceivedNewLevel2(QString symbol,
                                               Level2 level2,
                                               double bidAskImbalance,
                                               double bidDWP,
                                               double askDWP) override;
    void onCurrentHighlightedReceivedNewTrade(QString symbol, Trade trade) override;
    void onNewPositionReceived(QString account, Position position) override;
    void onPositionDeleted(QString account, QString positionID) override;
    void onNewOrderReceived(QString account, Order order) override;
    void onBalanceUpdated(Balance balance) override;

    // Replay mode notifications
    void onReplayModeEntered() override;
    void onReplayModeExited() override;
    void onReplayTimeUpdated(QDateTime currentTime) override;

  public:
    QString getSelectedAccountId() const;

  protected:
    bool eventFilter(QObject* p_watched, QEvent* p_event) override;

  private slots:
    void onTradeStationAuthStateChanged(bool isAuthenticated, TSClient::AuthStateReason reason, QString message);
    void onDatabentoConnectionStateChanged(bool isConnected);
    void onDatabentoStatusUpdate(const QString& symbol, bool isHalted, const QString& haltReason, bool isSsr);
    void onNewDisplayedStockSelection();
    void updateLiveLogDisplay(const QString& message);
    void onLoggerVisibilityChanged(bool visible);
    void onLogDepthChanged(int maxLines);
    void onOrderPlaced(const PlaceOrderRequest& order);
    void onShortcutChanged(ShortcutSettings::ShortcutId p_id, const QKeySequence& p_newSequence);
    void onCancelAllOrders();
    void onToggleReplayPlayPause();
    void onToggleReplayMode();
    void onAccountInfoButtonClicked();
    void updateSessionLabel();
    void updateTimeDisplay();

  private:
    void setupDarkTheme(QMainWindow* mainWindow);
    void requestMissingBarsFromCache(const QDateTime& from, const QDateTime& to);
    bool isValidStockSymbol(const QString& symbol) const;
    void displayStock(const QString& symbol);
    void saveLastDisplayedStock(const QString& symbol);
    void restoreLastDisplayedStock();
    QString formatAccountInfo(const Account& account) const;
    MainAlgo* mainAlgo;
    QString currentlyDisplayedSymbol;

    static QString bytesToString(qint64 bytes);

    std::unique_ptr<Ui::GUIFrontend> ui;  // Pointer to the UI object
    QMainWindow* m_mainWindow = nullptr;  // Main application window (owned by this)
    QPushButton* tradeStationLoginButton; // Login button in status bar
    QPushButton* m_databentoButton;       // Databento connection button in status bar
    QPushButton* m_accountInfoButton;     // Info button for account details

    QShortcut* m_quitShortcut;                  // Quit application shortcut
    QShortcut* m_focusShortcut;                 // Focus stock input shortcut
    QShortcut* m_buyShortcut;                   // Execute buy order shortcut
    QShortcut* m_sellShortcut;                  // Execute sell order shortcut
    QShortcut* m_buyToCoverShortcut;            // Execute buy to cover order shortcut
    QShortcut* m_sellToCoverShortcut;           // Execute sell to cover order shortcut
    QShortcut* m_cancelAllOrdersShortcut;       // Cancel all orders shortcut
    QShortcut* m_toggleReplayPlayPauseShortcut; // Toggle replay play/pause shortcut
    QShortcut* m_toggleReplayModeShortcut;      // Toggle replay mode on/off shortcut

    qsizetype TSClientDataUsage = 0;
    qsizetype m_dbClientDataUsage = 0;
    qint64 memoryUsage = 0;

    void updateStatusBar();

    int maxLiveLogLines = 1000; // Maximum lines in live log display

    bool m_hasRestoredLastStock = false; // Track if we've restored the last stock

    QVector<Account> m_accounts; // Store available accounts

    QLabel* m_tradingModeLabel = nullptr; // Trading mode indicator (SIM/LIVE)
    QLabel* m_dataSourceLabel = nullptr;  // Data source indicator (LIVE/REPLAY)
    QLabel* m_sessionLabel = nullptr;     // Trading session indicator
    QLabel* m_timeDisplayLabel = nullptr; // Application time display (live or replay)
    QTimer* m_timeUpdateTimer = nullptr;  // Timer to update time display

    // MarketFlags status labels
    QLabel* m_haltedLabel = nullptr;       // "HALTED" - red
    QLabel* m_delayedLabel = nullptr;      // "DELAYED" - yellow
    QLabel* m_hardToBorrowLabel = nullptr; // "HTB" - orange
};
