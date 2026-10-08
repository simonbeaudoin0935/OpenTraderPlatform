#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QPushButton>
#include <QShortcut>
#include <QKeySequence>
#include <QLabel>
#include <QSplitter>
#include <QEvent>
#include <QMap>
#include <QDateTime>
#include <QSet>
#include <optional>
#include <expected>
#include <memory>
#include <cstdint>

#include "MainAlgo.h"
#include "DBClient.h"
#include "TSClient.h"
#include "Misc/ShortcutSettings.h"
#include "Misc/TimeFrame.h"
#include "Widgets/ReplayControlsBar/ReplayControlsBar.h"
#include "Widgets/ReviewSessionBar/ReviewSessionBar.h"
#include "Widgets/TradingModeBar/TradingModeBar.h"

// Forward declarations
class PlaceOrderRequest;
class DownloadsTab;
class StrategyLogWidget;
class WindowManager;
class RiskTab;
class RiskStatusWidget;

// Forward declare the generated UI class
namespace Ui
{
    class GUIFrontend;
}

Q_DECLARE_LOGGING_CATEGORY(GUIFrontendLog)

class GUIFrontend : public QObject
{
    Q_OBJECT
  public:
    explicit GUIFrontend(MainAlgo* p_mainAlgo, QObject* parent = nullptr);
    ~GUIFrontend() override;

  public slots:
    void onPlatformStarted();
    void onTSClientDataUsageUpdate(qsizetype newDataUsage);
    void onDBClientDataUsageUpdate(qsizetype newDataUsage);
    void onTradeStationAccountsReceived(QVector<Account> results);
    void onMemoryUsageUpdate(qsizetype newDataUsage);
    void onNewPositionReceived(QString account, Position position);
    void onPositionDeleted(QString account, QString positionID);
    void onNewOrderReceived(QString account, Order order);
    void onBalanceUpdated(Balance balance);

    // Replay mode notifications
    void onReplayModeEntered();
    void onReplayModeExited();
    void onReplayConfigurationChanged(const QDate& date, const QTime& startTime, Playback::Speed speed);
    void onReplayPlaybackStateChanged(Playback::State state);
    void onReviewModeEntered();
    void onReviewModeExited();
    void onTradingModeConfigured(TradingMode mode);
    void onStrategyOrderConfirmationRequested(QString confirmationID,
                                              QString symbol,
                                              QString promptText,
                                              int timeoutSec,
                                              QString accountID,
                                              int quantity,
                                              bool isLongSide,
                                              double referencePrice,
                                              double stopPrice);
    void onStrategyOrderConfirmationResolved(QString confirmationID);

  signals:
    /**
     * @brief The main window has completed its first paint.
     * Thread context: Emitted from the Main/GUI thread.
     */
    void platformWindowPainted();
    void tradeStationAuthStateChanged(bool isAuthenticated, TSClient::AuthStateReason reason, QString message);
    void tradeStationAccountsReceived(QVector<Account> results);
    void tradeStationDataUsageUpdated(qsizetype newDataUsage);
    void databentoDataUsageUpdated(qsizetype newDataUsage);
    void newPositionReceived(QString account, Position position);
    void positionDeleted(QString account, QString positionID);
    void newOrderReceived(QString account, Order order);
    void balanceUpdated(Balance balance);

  public:
    QString getSelectedAccountId() const;
    [[nodiscard]] std::expected<DBClient::ReplayDownloadBatchResult, QString>
    startReplayDownloadBatch(const QDate& p_date, const QStringList& p_symbols);

  protected:
    bool eventFilter(QObject* p_watched, QEvent* p_event) override;

  private slots:
    void onTradeStationAuthStateChanged(bool isAuthenticated, TSClient::AuthStateReason reason, QString message);
    void onTradeStationQuoteReceived(const QString& symbol, const Quote& quote);
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
    void onCloseAllPositionsPassive();
    void onClosePosition(const QString& p_positionID);
    void onToggleReplayPlayPause();
    void onToggleReplayMode();
    void onReplayRestartRequested();
    void onAccountInfoButtonClicked();
    void updateSessionLabel();
    void updateTimeDisplay();
    void onDisplayRefreshTick();
    void onManagedBracketOverlayEvent(const StrategyBracketOverlayEntry& p_entry);
    void
    onManagedBracketProtectionDropped(const QString& p_accountID, const QString& p_symbol, const QString& p_reason);
    void onManualArmedBracketAdjusted(const QString& p_symbol, double p_stopPrice, double p_takePrice);
    void onRiskStatusChanged(const QString& p_accountId);
    void onRiskTabConfigChanged(const QString& p_accountId);
    void onRiskTabResetDayRequested(const QString& p_accountId);
    void onRiskTabUnlockRequested(const QString& p_accountId);

  private:
    void setupDarkTheme(QMainWindow* mainWindow);
    void applyAccountsToSelector(const QVector<Account>& p_accounts, const QString& p_preferredAccountId = QString());
    void requestMissingBarsFromCache(const QString& p_symbol,
                                     const QDateTime& from,
                                     const QDateTime& to,
                                     uint64_t p_requestToken);
    void updateTenSecondTimeFrameAvailability();
    void onTimeFrameChanged(TimeFrame tf);
    bool isValidStockSymbol(const QString& symbol) const;
    void displayStock(const QString& symbol);
    void saveLastDisplayedStock(const QString& symbol);
    void restoreLastDisplayedStock();
    void restoreStartupState();
    void saveReplayState(bool active, const QDate& date = QDate(), const QTime& startTime = QTime());
    void restoreReplayState();
    void saveReviewState(bool active, const QString& sessionId = QString());
    void restoreReviewState();
    void loadReviewSessionIntoWidgets();
    void saveMainWindowGeometry();
    void restoreMainWindowGeometry();
    QString formatAccountInfo(const Account& account) const;
    void submitClosePositionsRequest(const ClosePositionsRequest& p_request,
                                     const QString& p_dialogTitle,
                                     const QString& p_noMatchesMessage);
    void refreshRiskWidgetsForSelectedAccount();
    void requestRiskWidgetsRefresh(const QString& p_accountId);
    void applyRiskWidgetsRefreshResult(const QString& p_accountId,
                                       const RiskConfig& p_config,
                                       const RiskStatusSnapshot& p_snapshot);
    MainAlgo* mainAlgo;
    QString currentlyDisplayedSymbol;
    QString m_pendingReplayEntrySymbol;   // Null = use current symbol, explicit empty = enter replay without a symbol
    QString m_preReplayDisplayedSymbol;   // Null outside replay, otherwise preserves the live symbol for UI restore
    QString m_preReplaySelectedAccountId; // Null outside replay; restored when returning to live/sim accounts
    TimeFrame m_currentTimeFrame = TimeFrame::ONE_MINUTE;

    static QString bytesToString(qint64 bytes);

    std::unique_ptr<Ui::GUIFrontend> ui; // Pointer to the UI object
    QMainWindow* m_mainWindow = nullptr; // Main application window (owned by this)
    bool m_platformWindowPainted = false;
    QPushButton* tradeStationLoginButton; // TradeStation connection action button (top controls)
    QPushButton* m_databentoButton;       // Databento connection action button (top controls)
    QPushButton* m_stopLossTightenOnlyLockButton = nullptr;
    bool m_stopLossTightenOnlyLockActive = false;
    QWidget* m_connectivitySection = nullptr;
    QLabel* m_dataUsageLabel = nullptr;
    QPushButton* m_accountInfoButton; // Info button for account details

    QShortcut* m_quitShortcut;                     // Quit application shortcut
    QShortcut* m_focusShortcut;                    // Focus stock input shortcut
    QShortcut* m_buyShortcut;                      // Execute buy order shortcut
    QShortcut* m_sellShortcut;                     // Execute sell order shortcut
    QShortcut* m_buyToCoverShortcut;               // Execute buy to cover order shortcut
    QShortcut* m_sellToCoverShortcut;              // Execute sell to cover order shortcut
    QShortcut* m_cancelAllOrdersShortcut;          // Cancel all orders shortcut
    QShortcut* m_closeAllPositionsShortcut;        // Close all open positions shortcut
    QShortcut* m_closeAllPositionsPassiveShortcut; // Passive close-all positions shortcut
    QShortcut* m_toggleReplayPlayPauseShortcut;    // Toggle replay play/pause shortcut
    QShortcut* m_toggleReplayModeShortcut;         // Toggle replay mode on/off shortcut
    QShortcut* m_newChartWindowShortcut;           // Open new chart window shortcut

    // Timescale shortcuts
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

    bool m_platformStarted = false;
    bool m_hasRestoredLastStock = false; // Track if we've restored the last stock
    uint64_t m_symbolSelectionValidationToken = 0;
    bool m_hasShownDatabentoLiveFailureDialog = false;
    bool m_hasShownDatabentoFallbackWarning = false;
    QString m_activeStrategyConfirmationID;
    enum class StrategyConfirmationInputStage
    {
        Idle,
        AwaitingSwitchAuthorization,
        AwaitingDecision
    };
    StrategyConfirmationInputStage m_strategyConfirmationInputStage = StrategyConfirmationInputStage::Idle;
    QString m_activeStrategyConfirmationSymbol;
    QString m_activeStrategyConfirmationPrompt;
    int m_activeStrategyConfirmationTimeoutSec = 0;
    QDateTime m_activeStrategyConfirmationDeadlineUtc;
    qint64 m_activeStrategyConfirmationRemainingMs = 0;
    bool m_activeStrategyConfirmationCountdownPaused = false;
    bool m_strategyConfirmationAlertsMuted = false;
    QString m_lastStrategyConfirmationCueText;
    QString m_preConfirmationDisplayedSymbol;
    struct ChartViewRangesSnapshot
    {
        double xLower = 0.0;
        double xUpper = 0.0;
        double yLower = 0.0;
        double yUpper = 0.0;
    };
    std::optional<ChartViewRangesSnapshot> m_preConfirmationChartViewRanges;
    [[nodiscard]] bool hasOpenPositionOnDisplayedSymbolForSelectedAccount() const;
    [[nodiscard]] int getSignedNetPositionShares(const QString& p_accountID, const QString& p_symbol) const;
    void handleManualBracketArmShortcut(StrategyBracketOverlayEntry::Side p_side);
    void clearManualArmedBracket(const QString& p_reason);
    [[nodiscard]] std::optional<double> resolveManualBracketReferencePrice(const QString& p_symbol) const;
    void maybeActivateManualArmedBracketFromOrderUpdate(const Order& p_order);
    void updateManualArmedBracketPreviewInChart();
    void applyStrategyConfirmationMuteMode(bool p_muted);
    void rejectActiveStrategyConfirmationForMute();
    void resetActiveStrategyConfirmationUiState();
    void pauseActiveStrategyConfirmationCountdown();
    void resumeActiveStrategyConfirmationCountdown();
    [[nodiscard]] QString makeConfirmationCueTextForCurrentStage() const;
    [[nodiscard]] int currentStrategyConfirmationRemainingSeconds() const;
    void refreshStrategyConfirmationCueCountdown(bool p_forceRefresh);
    [[nodiscard]] bool validateActiveStrategyConfirmationPreviewRisk(QString* p_errorText = nullptr) const;

    enum class ManualOpeningSide : quint8
    {
        None,
        Long,
        Short
    };
    [[nodiscard]] static ManualOpeningSide resolveOpeningSideForOrder(const PlaceOrderRequest& p_order,
                                                                      int p_signedNetPositionShares);
    [[nodiscard]] static bool isBuySideTradeAction(TradeAction p_tradeAction);
    [[nodiscard]] static bool isSellSideTradeAction(TradeAction p_tradeAction);

    struct ManualArmedBracketState
    {
        enum class Source : quint8
        {
            ManualShortcut,
            StrategyConfirmation
        };

        QString symbol;
        QString accountID;
        StrategyBracketOverlayEntry::Side side = StrategyBracketOverlayEntry::Side::Long;
        double stopPrice = 0.0;
        double takePrice = 0.0;
        double referenceEntryPrice = 0.0;
        int previewQuantity = 0;
        QDateTime armTimestamp;
        bool awaitingEntryFill = false;
        bool autoActivateManagedBracketOnFill = true;
        Source source = Source::ManualShortcut;
        QString confirmationID;
        QSet<QString> submittedOrderIDs;
    };
    std::optional<ManualArmedBracketState> m_manualArmedBracket;
    quint64 m_manualBracketArmRequestToken = 0;

    QVector<Account> m_accounts; // Store available accounts
    QMap<QString, Position> m_positionsById;
    QMap<QString, RiskConfig> m_riskConfigByAccount;

    QLabel* m_sessionLabel = nullptr;           // Trading session indicator
    QLabel* m_timeDisplayLabel = nullptr;       // Application time display (live or replay)
    QTimer* m_timeUpdateTimer = nullptr;        // Timer to update time display
    QTimer m_displayRefreshTimer;               // 30 Hz pull-based display refresh timer
    TradingModeBar* m_tradingModeBar = nullptr; // LIVE / SIM / REPLAY tristate mode indicator
    bool m_replayPlaybackPaused = false;

    ReplayControlsBar* m_replayControlsBar = nullptr; // Replay controls in top toolbar
    ReviewSessionBar* m_reviewSessionBar = nullptr;   // Review session selector in top toolbar
    DownloadsTab* m_downloadsTab = nullptr;           // Replay downloads tab
    WindowManager* m_windowManager = nullptr;         // Manages secondary chart windows
    RiskTab* m_riskTab = nullptr;                     // Risk configuration tab
    RiskStatusWidget* m_riskStatusWidget = nullptr;   // Compact risk status widget above Time&Sales
    bool m_riskRefreshInFlight = false;
    bool m_riskRefreshPending = false;
    QString m_riskRefreshPendingAccountId;

    // Bottom logger split: platform log (left) + strategy log (right, shown on demand)
    QWidget* m_loggerContainer = nullptr;             ///< Outer container replacing liveLogDisplay in mainSplitter
    QSplitter* m_loggerSplitter = nullptr;            ///< Horizontal splitter inside m_loggerContainer
    StrategyLogWidget* m_strategyLogWidget = nullptr; ///< Strategy log panel (right; hidden until "Display Logs")
};
