#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QPushButton>
#include <QShortcut>
#include <QKeySequence>
#include <QLabel>
#include <QSplitter>
#include <QEvent>
#include <memory>

#include "FrontEnd.h"
#include "MainAlgo.h"
#include "Misc/ShortcutSettings.h"
#include "Misc/TimeFrame.h"
#include "Widgets/ReplayControlsBar/ReplayControlsBar.h"
#include "Widgets/TradingModeBar/TradingModeBar.h"

// Forward declarations
class PlaceOrderRequest;
class StrategyLogWidget;
class WindowManager;

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
    void onNewPositionReceived(QString account, Position position) override;
    void onPositionDeleted(QString account, QString positionID) override;
    void onNewOrderReceived(QString account, Order order) override;
    void onBalanceUpdated(Balance balance) override;

    // Replay mode notifications
    void onReplayModeEntered() override;
    void onReplayModeExited() override;
    void onReplayConfigurationChanged(const QDate& date, const QTime& startTime, Playback::Speed speed) override;
    void onReplayPlaybackStateChanged(Playback::State state) override;
    void onTradingModeConfigured(TradingMode mode);

  public:
    QString getSelectedAccountId() const;

  protected:
    bool eventFilter(QObject* p_watched, QEvent* p_event) override;

  private slots:
    void onTradeStationAuthStateChanged(bool isAuthenticated, TSClient::AuthStateReason reason, QString message);
    void onDatabentoConnectionStateChanged(bool isConnected);
    void onDatabentoStatusUpdate(const QString& symbol, bool isHalted, const QString& haltReason, bool isSsr);
    void onDatabentoGatewayError(const QString& errorText, bool isFatal);
    void onNewDisplayedStockSelection();
    void updateLiveLogDisplay(const QString& message);
    void onLoggerVisibilityChanged(bool visible);
    void onLogDepthChanged(int maxLines);
    void onOrderPlaced(const PlaceOrderRequest& order);
    void onShortcutChanged(ShortcutSettings::ShortcutId p_id, const QKeySequence& p_newSequence);
    void onCancelAllOrders();
    void onCloseAllPositions();
    void onToggleReplayPlayPause();
    void onToggleReplayMode();
    void onAccountInfoButtonClicked();
    void updateSessionLabel();
    void updateTimeDisplay();
    void onDisplayRefreshTick();

  private:
    void setupDarkTheme(QMainWindow* mainWindow);
    void requestMissingBarsFromCache(const QDateTime& from, const QDateTime& to);
    void onTimeFrameChanged(TimeFrame tf);
    bool isValidStockSymbol(const QString& symbol) const;
    void displayStock(const QString& symbol);
    void saveLastDisplayedStock(const QString& symbol);
    void restoreLastDisplayedStock();
    void saveReplayState(bool active, const QDate& date = QDate(), const QTime& startTime = QTime());
    void restoreReplayState();
    void saveMainWindowGeometry();
    void restoreMainWindowGeometry();
    QString formatAccountInfo(const Account& account) const;
    MainAlgo* mainAlgo;
    QString currentlyDisplayedSymbol;
    TimeFrame m_currentTimeFrame = TimeFrame::ONE_MINUTE;

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
    QShortcut* m_closeAllPositionsShortcut;     // Close all open positions shortcut
    QShortcut* m_toggleReplayPlayPauseShortcut; // Toggle replay play/pause shortcut
    QShortcut* m_toggleReplayModeShortcut;      // Toggle replay mode on/off shortcut
    QShortcut* m_newChartWindowShortcut;        // Open new chart window shortcut

    // Timescale shortcuts
    QShortcut* m_timeFrame10sShortcut;
    QShortcut* m_timeFrame1mShortcut;
    QShortcut* m_timeFrame5mShortcut;
    QShortcut* m_timeFrame15mShortcut;
    QShortcut* m_timeFrame30mShortcut;
    QShortcut* m_timeFrame1hShortcut;
    QShortcut* m_timeFrame4hShortcut;
    QShortcut* m_timeFrame1dShortcut;
    QShortcut* m_timeFrame1wShortcut;
    QShortcut* m_timeFrame1MShortcut;

    qsizetype TSClientDataUsage = 0;
    qsizetype m_dbClientDataUsage = 0;
    qint64 memoryUsage = 0;

    void updateStatusBar();

    int maxLiveLogLines = 1000; // Maximum lines in live log display

    bool m_hasRestoredLastStock = false; // Track if we've restored the last stock

    QVector<Account> m_accounts; // Store available accounts

    QLabel* m_sessionLabel = nullptr;           // Trading session indicator
    QLabel* m_timeDisplayLabel = nullptr;       // Application time display (live or replay)
    QTimer* m_timeUpdateTimer = nullptr;        // Timer to update time display
    QTimer m_displayRefreshTimer;               // 30 Hz pull-based display refresh timer
    TradingModeBar* m_tradingModeBar = nullptr; // LIVE / SIM / REPLAY tristate mode indicator

    ReplayControlsBar* m_replayControlsBar = nullptr; // Replay controls in top toolbar
    WindowManager* m_windowManager = nullptr;         // Manages secondary chart windows

    // Bottom logger split: platform log (left) + strategy log (right, shown on demand)
    QWidget* m_loggerContainer = nullptr;             ///< Outer container replacing liveLogDisplay in mainSplitter
    QSplitter* m_loggerSplitter = nullptr;            ///< Horizontal splitter inside m_loggerContainer
    StrategyLogWidget* m_strategyLogWidget = nullptr; ///< Strategy log panel (right; hidden until "Display Logs")
};
