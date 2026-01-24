#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QPushButton>
#include <QShortcut>
#include <QKeySequence>
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
    void onTradeStationAccountsReceived(QVector<Account> results) override;
    void onMemoryUsageUpdate(qsizetype newDataUsage) override;
    void onStreamCountUpdate(int count) override;
    void onCurrentHighlightedStockBarReceived(QString symbol, Bar bar) override;
    void onCurrentHighlightedReceivedNewMarketDepthQuote(QString symbol,
                                                         MarketDepthQuote quote,
                                                         double bidAskImbalance,
                                                         double bidDWP,
                                                         double askDWP) override;
    void onNewPositionReceived(QString account, Position position) override;
    void onPositionDeleted(QString account, QString positionID) override;
    void onNewOrderReceived(QString account, Order order) override;
    void onBalanceUpdated(Balance balance) override;

  public:
    QString getSelectedAccountId() const;

  private slots:
    void onTradeStationAuthStateChanged(bool isAuthenticated, QString reason);
    void onNewDisplayedStockSelection();
    void updateLiveLogDisplay(const QString& message);
    void onLoggerVisibilityChanged(bool visible);
    void onLogDepthChanged(int maxLines);
    void onOrderPlaced(const PlaceOrderRequest& order);
    void onShortcutChanged(ShortcutSettings::ShortcutId p_id, const QKeySequence& p_newSequence);
    void onCancelAllOrders();
    void onAccountInfoButtonClicked();

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
    QPushButton* tradeStationLoginButton; // Login button in status bar
    QPushButton* m_accountInfoButton;     // Info button for account details

    QShortcut* m_quitShortcut;            // Quit application shortcut
    QShortcut* m_focusShortcut;           // Focus stock input shortcut
    QShortcut* m_buyShortcut;             // Execute buy order shortcut
    QShortcut* m_sellShortcut;            // Execute sell order shortcut
    QShortcut* m_buyToCoverShortcut;      // Execute buy to cover order shortcut
    QShortcut* m_sellToCoverShortcut;     // Execute sell to cover order shortcut
    QShortcut* m_cancelAllOrdersShortcut; // Cancel all orders shortcut

    qsizetype TSClientDataUsage = 0;
    qint64 memoryUsage = 0;
    int streamCount = 0;

    int maxLiveLogLines = 1000; // Maximum lines in live log display

    bool m_hasRestoredLastStock = false; // Track if we've restored the last stock

    QVector<Account> m_accounts; // Store available accounts
};
