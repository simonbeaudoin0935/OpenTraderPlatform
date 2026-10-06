#include <QJsonDocument>
#include <QHeaderView>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QPalette>
#include <QApplication>
#include <QComboBox>
#include <QScreen>
#include <QWindow>
#include <QShortcut>
#include <QCoreApplication>
#include <QFont>
#include <QPointer>
#include <QTextCursor>
#include <QScrollBar>
#include <QScrollArea>
#include <QSlider>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QKeyEvent>
#include <QLineEdit>
#include <QAbstractSpinBox>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QReadLocker>
#include <QRegularExpression>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <csignal>
#include <optional>
#include <QFile>
#include "Assume.h"
#include "LTTng/LTTngTracepoints.h"

#include "TSClient.h"
#include "GUIFrontend.h"
#include "ui_GUIFrontend.h"
#include "Tabs/LoggingTab.h"
#include "Tabs/DownloadsTab.h"
#include "Tabs/ShortcutsTab.h"
#include "Tabs/ConfigTab.h"
#include "Tabs/CacheTab.h"
#include "Tabs/RiskTab.h"
#include "Widgets/StrategyQuickView/StrategyQuickView.h"
#include "Widgets/StrategyLogWidget/StrategyLogWidget.h"
#include "Widgets/ReplayControlsBar/ReplayControlsBar.h"
#include "Widgets/RiskStatus/RiskStatusWidget.h"
#include "StrategyManager.h"
#include "StockPriceChart/ChartToolbar.h"
#include "StockPriceChart/StockPriceChart.h"
#include "WindowManager/WindowManager.h"
#include "ChartWindow/ChartWindow.h"

namespace
{
    constexpr auto REPLAY_SIM_ACCOUNT_ID = "SIM123456";

    [[nodiscard]] Account makeReplaySimAccount()
    {
        QJsonObject accountJson;
        accountJson["AccountID"] = QString::fromLatin1(REPLAY_SIM_ACCOUNT_ID);
        accountJson["AccountType"] = "Margin";
        accountJson["Name"] = "Replay Simulation Account";
        accountJson["Status"] = "Active";
        return Account(accountJson);
    }
} // namespace
#include "Core/PlatformControlProtocol.h"
#include "Core/OrdersDatabase.h"
#include "Core/PositionsDatabase.h"
#include "Misc/Logging/Logging.h"
#include "Misc/Settings.h"
#include "Misc/ShortcutSettings.h"
#include "Core/MainApp.h"
#include "Assume.h"
#include "DBClient.h"
#include "BarUtils.h"
#include "CONSTANTS.h"
#include "SecureStorage.h"
#include <QInputDialog>
#include <QSet>

#define LOGGING_CATEGORY GUIFrontendLog

Q_LOGGING_CATEGORY(GUIFrontendLog, "GUIFrontend")

namespace
{
    struct ReplayEntryResolution
    {
        bool enterWithoutSymbol = false;
    };

    [[nodiscard]] ReplayEntryResolution resolveReplayEntryResolution(const QString& p_currentSymbol,
                                                                     const QDate& p_requestedDate)
    {
        Q_UNUSED(p_requestedDate);
        ReplayEntryResolution resolution{p_currentSymbol.isEmpty()};
        if (p_currentSymbol.isEmpty())
        {
            return resolution;
        }

        if (DBClient::hasReplayData(p_requestedDate, p_currentSymbol))
        {
            return resolution;
        }

        const QVector<QDate> availableDates = DBClient::listAvailableReplayDatesForSymbol(p_currentSymbol);
        resolution.enterWithoutSymbol = availableDates.isEmpty();
        return resolution;
    }

    [[nodiscard]] QString closeExecutionModeSummaryText(const ClosePositionsExecutionMode p_mode)
    {
        switch (p_mode)
        {
        case ClosePositionsExecutionMode::AggressiveMarketable:
            return "Day+ aggressive marketable limit orders";
        case ClosePositionsExecutionMode::PassiveResting:
            return "Day+ passive resting limit orders";
        }

        return "Day+ limit orders";
    }

    [[nodiscard]] QString closePositionsSessionSummary(const ClosePositionsResult& p_result)
    {
        if (!p_result.usesLimitOrders)
        {
            return QString("Session: %1 (market orders)").arg(p_result.session);
        }

        return QString("Session: %1 (%2, offset %3 c)")
            .arg(p_result.session)
            .arg(closeExecutionModeSummaryText(p_result.executionMode))
            .arg(p_result.aggressivityOffsetCents, 0, 'f', 2);
    }

    QString summarizeClosePositionsSuccess(const ClosePositionsResult& p_result)
    {
        QStringList lines;
        lines << QString("Submitted close-position orders for %1 position(s).").arg(p_result.successCount());
        lines << closePositionsSessionSummary(p_result);

        for (const ClosePositionItemResult& item: p_result.items)
        {
            if (!item.isSuccessful())
            {
                continue;
            }

            QString line = QString("- %1 x %2").arg(item.symbol).arg(item.quantity);
            if (item.limitPrice.has_value())
            {
                line += QString(" @ %1").arg(item.limitPrice.value(), 0, 'f', 2);
            }
            if (!item.orderIds.isEmpty())
            {
                line += QString(" [%1]").arg(item.orderIds.join(", "));
            }
            lines << line;
        }

        return lines.join("\n");
    }

    QString summarizeClosePositionsFailures(const ClosePositionsResult& p_result)
    {
        QStringList lines;
        lines << QString("Submitted %1 of %2 close-position order(s).")
                     .arg(p_result.successCount())
                     .arg(p_result.matchedPositionCount);
        lines << closePositionsSessionSummary(p_result);

        lines << "";
        lines << "Failures:";
        for (const ClosePositionItemResult& item: p_result.items)
        {
            if (!item.hasFailure())
            {
                continue;
            }

            lines << QString("- %1: %2").arg(item.symbol, item.failureMessage);
        }

        if (p_result.successCount() > 0)
        {
            lines << "";
            lines << "Submitted:";
            for (const ClosePositionItemResult& item: p_result.items)
            {
                if (!item.isSuccessful())
                {
                    continue;
                }

                QString line = QString("- %1 x %2").arg(item.symbol).arg(item.quantity);
                if (!item.orderIds.isEmpty())
                {
                    line += QString(" [%1]").arg(item.orderIds.join(", "));
                }
                lines << line;
            }
        }

        return lines.join("\n");
    }

    [[nodiscard]] bool isFillLikeStatus(const Order::Status p_status)
    {
        return p_status == Order::Status::FLL || p_status == Order::Status::FLP || p_status == Order::Status::FPR;
    }

    [[nodiscard]] bool isCancelLikeStatus(const Order::Status p_status)
    {
        return p_status == Order::Status::CAN || p_status == Order::Status::UCN || p_status == Order::Status::TSC ||
               p_status == Order::Status::REJ || p_status == Order::Status::OUT || p_status == Order::Status::EXP;
    }

    [[nodiscard]] bool isBuySideOrderUpdate(const Order& p_order)
    {
        return p_order.getTradeAction().toUpper().contains("BUY");
    }

    [[nodiscard]] bool isSellSideOrderUpdate(const Order& p_order)
    {
        return p_order.getTradeAction().toUpper().contains("SELL");
    }

} // namespace

GUIFrontend::GUIFrontend(MainAlgo* p_mainAlgo, QObject* parent) : QObject(parent), mainAlgo(p_mainAlgo)
{
    ui = std::make_unique<Ui::GUIFrontend>();

    // Create main window with this as parent for proper Qt ownership
    m_mainWindow = new QMainWindow();
    m_mainWindow->setAttribute(Qt::WA_DeleteOnClose, false); // We manage deletion
    ui->setupUi(m_mainWindow);
    ui->statusbar->setVisible(false);

    this->setObjectName("GUIFrontend");

    setupDarkTheme(m_mainWindow);

    // Install event filter on main window to handle close events
    m_mainWindow->installEventFilter(this);
    QCoreApplication::instance()->installEventFilter(this);

    // Restore main window geometry from last session, or show maximized on primary screen
    restoreMainWindowGeometry();

    // Initialize shortcuts from settings
    ShortcutSettings& shortcutSettings = ShortcutSettings::getInstance();

    // Add Ctrl+Q shortcut to quit the application gracefully
    m_quitShortcut = new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::QuitApplication), m_mainWindow);
    // Save state then shutdown gracefully
    auto quitConnection = connect(m_quitShortcut,
                                  &QShortcut::activated,
                                  this,
                                  [this]()
                                  {
                                      logInputEvent(u"GUIFrontend", u"quit-application-shortcut");
                                      saveMainWindowGeometry();
                                      if (m_windowManager)
                                      {
                                          m_windowManager->saveWindowState();
                                          m_windowManager->setShuttingDown();
                                      }
                                      MainApp::getInstance()->shutdown();
                                  });
    OBJ_ASSUME_TRUE(quitConnection);

    // Add "i" shortcut to focus the stock symbol input box
    m_focusShortcut = new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::FocusStockInput), m_mainWindow);
    // Note: Qt::UniqueConnection cannot be used with lambda functions
    auto focusConnection = connect(m_focusShortcut,
                                   &QShortcut::activated,
                                   [this]()
                                   {
                                       logInputEvent(u"GUIFrontend", u"focus-symbol-input-shortcut");
                                       ui->stockSymbolInput->clear();
                                       ui->stockSymbolInput->setFocus();
                                   });
    OBJ_ASSUME_TRUE(focusConnection);

    // Add Ctrl+B shortcut to execute buy order
    m_buyShortcut = new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::ExecuteBuyOrder), m_mainWindow);
    auto buyConnection =
        connect(m_buyShortcut, &QShortcut::activated, [this]() { ui->orderEntryWidget->executeBuyOrder(); });
    OBJ_ASSUME_TRUE(buyConnection);

    // Add Ctrl+S shortcut to execute sell order
    m_sellShortcut = new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::ExecuteSellOrder), m_mainWindow);
    auto sellConnection =
        connect(m_sellShortcut, &QShortcut::activated, [this]() { ui->orderEntryWidget->executeSellOrder(); });
    OBJ_ASSUME_TRUE(sellConnection);

    // Add Ctrl+Shift+B shortcut to execute buy to cover order
    m_buyToCoverShortcut =
        new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::ExecuteBuyToCoverOrder), m_mainWindow);
    auto buyToCoverConnection = connect(m_buyToCoverShortcut,
                                        &QShortcut::activated,
                                        [this]() { ui->orderEntryWidget->executeBuyToCoverOrder(); });
    OBJ_ASSUME_TRUE(buyToCoverConnection);

    // Add Ctrl+Shift+S shortcut to execute sell to cover order
    m_sellToCoverShortcut =
        new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::ExecuteSellToCoverOrder), m_mainWindow);
    auto sellToCoverConnection = connect(m_sellToCoverShortcut,
                                         &QShortcut::activated,
                                         [this]() { ui->orderEntryWidget->executeSellToCoverOrder(); });
    OBJ_ASSUME_TRUE(sellToCoverConnection);

    // Add Ctrl+X shortcut to cancel all orders
    m_cancelAllOrdersShortcut =
        new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::CancelAllOrders), m_mainWindow);
    auto cancelAllConnection =
        connect(m_cancelAllOrdersShortcut, &QShortcut::activated, [this]() { onCancelAllOrders(); });
    OBJ_ASSUME_TRUE(cancelAllConnection);

    m_closeAllPositionsShortcut =
        new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::CloseAllPositions), m_mainWindow);
    auto closeAllPositionsConnection =
        connect(m_closeAllPositionsShortcut, &QShortcut::activated, [this]() { onCloseAllPositions(); });
    OBJ_ASSUME_TRUE(closeAllPositionsConnection);

    m_closeAllPositionsPassiveShortcut =
        new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::CloseAllPositionsPassive), m_mainWindow);
    auto closeAllPositionsPassiveConnection =
        connect(m_closeAllPositionsPassiveShortcut, &QShortcut::activated, [this]() { onCloseAllPositionsPassive(); });
    OBJ_ASSUME_TRUE(closeAllPositionsPassiveConnection);

    // Add Space shortcut to toggle replay play/pause
    m_toggleReplayPlayPauseShortcut =
        new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::ToggleReplayPlayPause), m_mainWindow);
    auto toggleReplayConnection =
        connect(m_toggleReplayPlayPauseShortcut, &QShortcut::activated, [this]() { onToggleReplayPlayPause(); });
    OBJ_ASSUME_TRUE(toggleReplayConnection);

    // Add "r" shortcut to toggle replay mode on/off
    m_toggleReplayModeShortcut =
        new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::ToggleReplayMode), m_mainWindow);
    auto toggleReplayModeConnection =
        connect(m_toggleReplayModeShortcut, &QShortcut::activated, [this]() { onToggleReplayMode(); });
    OBJ_ASSUME_TRUE(toggleReplayModeConnection);

    // Timescale shortcuts (1-9 keys)
    auto createTimeFrameShortcut = [this, &shortcutSettings](ShortcutSettings::ShortcutId id, TimeFrame tf)
    {
        QShortcut* shortcut = new QShortcut(shortcutSettings.getShortcut(id), m_mainWindow);
        auto conn = connect(shortcut,
                            &QShortcut::activated,
                            [this, tf]() { ui->priceChart->toolbar()->setCurrentTimeFrame(tf); });
        OBJ_ASSUME_TRUE(conn);
        return shortcut;
    };

    m_timeFrame10sShortcut = createTimeFrameShortcut(ShortcutSettings::TimeFrame10s, TimeFrame::TEN_SECONDS);
    m_timeFrame1mShortcut = createTimeFrameShortcut(ShortcutSettings::TimeFrame1m, TimeFrame::ONE_MINUTE);
    m_timeFrame5mShortcut = createTimeFrameShortcut(ShortcutSettings::TimeFrame5m, TimeFrame::FIVE_MINUTES);
    m_timeFrame15mShortcut = createTimeFrameShortcut(ShortcutSettings::TimeFrame15m, TimeFrame::FIFTEEN_MINUTES);
    m_timeFrame30mShortcut = createTimeFrameShortcut(ShortcutSettings::TimeFrame30m, TimeFrame::THIRTY_MINUTES);
    m_timeFrame1hShortcut = createTimeFrameShortcut(ShortcutSettings::TimeFrame1h, TimeFrame::ONE_HOUR);
    m_timeFrame4hShortcut = createTimeFrameShortcut(ShortcutSettings::TimeFrame4h, TimeFrame::FOUR_HOURS);
    m_timeFrame1dShortcut = createTimeFrameShortcut(ShortcutSettings::TimeFrame1d, TimeFrame::ONE_DAY);
    m_timeFrame1wShortcut = createTimeFrameShortcut(ShortcutSettings::TimeFrame1w, TimeFrame::ONE_WEEK);
    m_timeFrame1MShortcut = createTimeFrameShortcut(ShortcutSettings::TimeFrame1M, TimeFrame::ONE_MONTH);

    // Connect to shortcut changes to update active shortcuts
    auto shortcutChangeConnection = connect(&shortcutSettings,
                                            &ShortcutSettings::shortcutChanged,
                                            this,
                                            &GUIFrontend::onShortcutChanged,
                                            Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(shortcutChangeConnection);

    // Hidden debug shortcut: Shift+Q raises SIGSEGV to test the crash handler.
    // Not configurable, not documented — for developer use only.
    auto* debugCrashShortcut = new QShortcut(QKeySequence("Shift+Q"), m_mainWindow);
    auto debugCrashConnection = connect(debugCrashShortcut, &QShortcut::activated, []() { raise(SIGSEGV); });
    OBJ_ASSUME_TRUE(debugCrashConnection);


    // Create TradeStation connection button (hosted in top controls connectivity section)
    tradeStationLoginButton = new QPushButton("Login to TradeStation", m_mainWindow);
    tradeStationLoginButton->setStyleSheet(
        "QPushButton { background-color: #00A0E9; color: #ffffff; padding: 2px 6px; border-radius: 3px; }");
    tradeStationLoginButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Connect TradeStation login button click to launch auth process
    connect(tradeStationLoginButton,
            &QPushButton::clicked,
            this,
            []()
            {
                // GUIAuthHandler is modal, so it's impossible to click the button while authentication is in progress
                ASSUME_FALSE(TSClient::getInstance()->isAuthInProgress());
                logInputEvent(u"GUIFrontend", u"launch-tradestation-auth");
                TSClient::getInstance()->launchAuthProcess();
            });

    // Create Databento connection button (hosted in top controls connectivity section)
    m_databentoButton = new QPushButton("Connect to Databento", m_mainWindow);
    m_databentoButton->setStyleSheet(
        "QPushButton { background-color: #00A0E9; color: #ffffff; padding: 2px 6px; border-radius: 3px; }");
    m_databentoButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(m_databentoButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                DEBUG << "Databento button clicked";
                logInputEvent(u"GUIFrontend", u"open-databento-api-key-dialog");
                bool ok = false;
                const QString apiKey = QInputDialog::getText(m_mainWindow,
                                                             "Databento API Key",
                                                             "Enter your Databento API key:",
                                                             QLineEdit::Password,
                                                             QString(),
                                                             &ok);
                if (ok && !apiKey.trimmed().isEmpty())
                {
                    DBClient::getInstance()->storeApiKey(apiKey.trimmed());
                }
            });

    connect(DBClient::getInstance(),
            &DBClient::connectionStateChanged,
            this,
            &GUIFrontend::onDatabentoConnectionStateChanged,
            Qt::UniqueConnection);

    connect(DBClient::getInstance(),
            &DBClient::newStatus,
            this,
            &GUIFrontend::onDatabentoStatusUpdate,
            Qt::UniqueConnection);

    connect(TSClient::getInstance(),
            &TSClient::newQuoteReceived,
            this,
            &GUIFrontend::onTradeStationQuoteReceived,
            Qt::UniqueConnection);

    connect(DBClient::getInstance(),
            &DBClient::credentialStorageFailed,
            this,
            [this](const QString& p_errorMessage)
            { QMessageBox::warning(m_mainWindow, "Credential Storage Error", p_errorMessage); });

    connect(DBClient::getInstance(),
            &DBClient::liveGatewayError,
            this,
            &GUIFrontend::onDatabentoGatewayError,
            Qt::UniqueConnection);

    // Reflect initial Databento connection state
    DBClient::getInstance()->loadApiKey();

    // Setup account info button - hide initially until accounts are loaded
    m_accountInfoButton = ui->accountInfoButton;
    m_accountInfoButton->setVisible(false);
    auto accountInfoConnection = connect(m_accountInfoButton,
                                         &QPushButton::clicked,
                                         this,
                                         &GUIFrontend::onAccountInfoButtonClicked,
                                         Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(accountInfoConnection);

    // Reorganize toolbar: Center session+clock, right-align mode labels

    // Add spacer to push session label towards center
    auto* leftSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    ui->topControlsLayout->insertSpacerItem(3, leftSpacer);

    // Create trading session indicator (centered with clock)
    m_sessionLabel = new QLabel("CLOSED", m_mainWindow);
    Q_CHECK_PTR(m_sessionLabel);
    m_sessionLabel->setStyleSheet("QLabel { background-color: #555555; color: #ffffff; padding: 4px 8px; "
                                  "border-radius: 4px; font-weight: bold; font-family: monospace; }");
    m_sessionLabel->setToolTip("Current trading session:\n"
                               "🌙 EARLY PRE-MARKET: 4:00 AM - 5:59 AM ET\n"
                               "🌅 PRE-MARKET: 6:00 AM - 9:29 AM ET\n"
                               "📈 REGULAR: 9:30 AM - 3:59 PM ET\n"
                               "🌆 AFTER-HOURS: 4:00 PM - 7:59 PM ET\n"
                               "🌚 CLOSED: 8:00 PM - 3:59 AM ET\n"
                               "🏖 WEEKEND: Saturday & Sunday\n"
                               "🎌 HOLIDAY: NYSE market holiday");
    ui->topControlsLayout->insertWidget(4, m_sessionLabel);
    updateSessionLabel();

    // Status labels (HALTED, DELAYED, HTB) live in the chart toolbar — see ChartToolbar.

    // Create time display widget (centered next to session label)
    m_timeDisplayLabel = new QLabel("00:00:00", m_mainWindow);
    Q_CHECK_PTR(m_timeDisplayLabel);
    m_timeDisplayLabel->setStyleSheet(
        "QLabel { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "                               stop:0 #1a1a1a, stop:0.5 #0d0d0d, stop:1 #1a1a1a); "
        "  color: #00ff00; " // Bright green for LIVE mode
        "  border: 2px solid #333333; "
        "  border-radius: 6px; "
        "  padding: 6px 12px; "
        "  font-family: 'Courier New', monospace; "
        "  font-size: 14px; "
        "  font-weight: bold; "
        "  letter-spacing: 1px; "
        "}");
    m_timeDisplayLabel->setToolTip("Application time (New York timezone)\n"
                                   "🟢 Green: LIVE mode - real-time clock\n"
                                   "🟠 Amber: REPLAY mode - simulated time");
    ui->topControlsLayout->insertWidget(5, m_timeDisplayLabel);

    // Replay controls bar — shown to the right of the clock, only visible in replay mode
    m_replayControlsBar = new ReplayControlsBar(m_mainWindow);
    Q_CHECK_PTR(m_replayControlsBar);
    m_replayControlsBar->setVisible(false); // hidden until replay mode is entered
    ui->topControlsLayout->insertWidget(6, m_replayControlsBar);

    m_reviewSessionBar = new ReviewSessionBar(m_mainWindow);
    Q_CHECK_PTR(m_reviewSessionBar);
    m_reviewSessionBar->setVisible(false);
    ui->topControlsLayout->insertWidget(7, m_reviewSessionBar);

    // Connect chart to replay controls bar so chart slots respond to user input
    ui->priceChart->connectReplayControls(m_replayControlsBar);
    ui->priceChart->setManualConfirmationMutedWatermarkVisible(m_strategyConfirmationAlertsMuted);

    // Add spacer to push mode labels to the right
    auto* rightSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    ui->topControlsLayout->insertSpacerItem(8, rightSpacer);

    // Create tristate trading-mode indicator (right side: LIVE / SIM / REPLAY pills)
    bool isSimMode = (MainApp::getTradingMode() == TradingMode::Sim);
    m_tradingModeBar = new TradingModeBar(m_mainWindow);
    Q_CHECK_PTR(m_tradingModeBar);
    m_tradingModeBar->setActiveMode(isSimMode ? TradingModeBar::Mode::Sim : TradingModeBar::Mode::Live);
    m_tradingModeBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->topControlsLayout->addWidget(m_tradingModeBar, 0, Qt::AlignRight);

    m_stopLossTightenOnlyLockButton = new QPushButton(QStringLiteral("SL Lock: OFF"), m_mainWindow);
    Q_CHECK_PTR(m_stopLossTightenOnlyLockButton);
    m_stopLossTightenOnlyLockButton->setToolTip("One-way safety lock for bracket stop dragging.\n"
                                                "When enabled, stop losses can only move in the risk-reducing "
                                                "direction.\n"
                                                "This lock cannot be disabled until app relaunch.");
    m_stopLossTightenOnlyLockButton->setStyleSheet(
        "QPushButton { background-color: #3f3f3f; color: #f0c35a; border: 1px solid #5a5a5a; "
        "padding: 5px 10px; border-radius: 4px; font-weight: bold; } "
        "QPushButton:hover { background-color: #4a4a4a; } "
        "QPushButton:disabled { background-color: #234f2e; color: #d8f7dd; border: 1px solid #3f8f52; }");
    ui->topControlsLayout->addWidget(m_stopLossTightenOnlyLockButton, 0, Qt::AlignRight);

    // Create connectivity section (hosted in the Credentials tab)
    m_connectivitySection = new QWidget(m_mainWindow);
    Q_CHECK_PTR(m_connectivitySection);
    m_connectivitySection->setObjectName("TopConnectivitySection");
    auto* connectivityLayout = new QHBoxLayout(m_connectivitySection);
    connectivityLayout->setContentsMargins(8, 2, 8, 2);
    connectivityLayout->setSpacing(6);

    auto* connectivityButtonsWidget = new QWidget(m_connectivitySection);
    auto* connectivityButtonsLayout = new QVBoxLayout(connectivityButtonsWidget);
    connectivityButtonsLayout->setContentsMargins(0, 0, 0, 0);
    connectivityButtonsLayout->setSpacing(2);
    connectivityButtonsLayout->addWidget(tradeStationLoginButton);
    connectivityButtonsLayout->addWidget(m_databentoButton);

    m_dataUsageLabel = new QLabel("TS: 0 B\nDB: 0 B\nMem: 0 B", m_connectivitySection);
    Q_CHECK_PTR(m_dataUsageLabel);
    m_dataUsageLabel->setStyleSheet("QLabel { color: #dddddd; font-family: monospace; font-size: 10px; }");
    m_dataUsageLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_connectivitySection->setStyleSheet("QWidget#TopConnectivitySection { background-color: #2B2B2B; "
                                         "border: 1px solid #4A4A4A; border-radius: 4px; }");
    connectivityLayout->addWidget(connectivityButtonsWidget, 0, Qt::AlignTop);
    connectivityLayout->addWidget(m_dataUsageLabel);
    updateStatusBar();

    connect(m_stopLossTightenOnlyLockButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if (m_stopLossTightenOnlyLockActive)
                {
                    return;
                }

                const QMessageBox::StandardButton reply =
                    QMessageBox::warning(nullptr,
                                         "Enable Stop-Loss Lock",
                                         "Enable stop-loss tighten-only lock for this app session?\n\n"
                                         "After activation:\n"
                                         "- Long stops cannot be dragged down.\n"
                                         "- Short stops cannot be dragged up.\n"
                                         "- This lock stays active until you relaunch the app.",
                                         QMessageBox::Yes | QMessageBox::No,
                                         QMessageBox::No);
                if (reply != QMessageBox::Yes)
                {
                    return;
                }

                m_stopLossTightenOnlyLockActive = true;
                m_stopLossTightenOnlyLockButton->setText(QStringLiteral("SL Lock: ON"));
                m_stopLossTightenOnlyLockButton->setToolTip("Stop-loss tighten-only lock is active.\n"
                                                            "Relaunch the app to clear this lock.");
                m_stopLossTightenOnlyLockButton->setEnabled(false);

                QMetaObject::invokeMethod(
                    mainAlgo,
                    [algo = mainAlgo]()
                    {
                        ASSUME_DIFF(algo, nullptr);
                        algo->activateStopLossTightenOnlyLock();
                    },
                    Qt::QueuedConnection);
            });

    // Wire mode-switch signals
    connect(m_tradingModeBar,
            &TradingModeBar::liveRequested,
            this,
            [this]()
            {
                // If in replay but brokerage is already LIVE, just exit replay — no restart needed.
                if (MainApp::isInReplayMode() && MainApp::getTradingMode() == TradingMode::Live)
                {
                    MainApp::getInstance()->exitReplayMode();
                    return;
                }
                if (MainApp::isInReviewMode() && MainApp::getTradingMode() == TradingMode::Live)
                {
                    MainApp::getInstance()->exitReviewMode();
                    return;
                }

                QString warning =
                    MainApp::isInReplayMode()
                        ? "This will exit replay mode and restart the application to connect to the LIVE API.\n\n"
                          "⚠️ WARNING: LIVE mode uses REAL MONEY!"
                        : "This will restart the application to connect to the LIVE API.\n\n"
                          "⚠️ WARNING: LIVE mode uses REAL MONEY!";

                QMessageBox::StandardButton reply =
                    QMessageBox::question(nullptr,
                                          "Switch to LIVE mode",
                                          "Switch to LIVE (real money) mode?\n\n" + warning,
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No);
                if (reply == QMessageBox::Yes)
                {
                    if (MainApp::isInReviewMode())
                    {
                        MainApp::getInstance()->exitReviewMode();
                    }
                    MainApp::setTradingMode(TradingMode::Live);
                    MainApp::restartApplication();
                }
            });

    connect(m_tradingModeBar,
            &TradingModeBar::simRequested,
            this,
            [this]()
            {
                // If in replay but brokerage is already SIM, just exit replay — no restart needed.
                if (MainApp::isInReplayMode() && MainApp::getTradingMode() == TradingMode::Sim)
                {
                    MainApp::getInstance()->exitReplayMode();
                    return;
                }
                if (MainApp::isInReviewMode() && MainApp::getTradingMode() == TradingMode::Sim)
                {
                    MainApp::getInstance()->exitReviewMode();
                    return;
                }

                QString detail =
                    MainApp::isInReplayMode()
                        ? "This will exit replay mode and restart the application to connect to the SIM API."
                        : "This will restart the application to connect to the SIM API.";

                QMessageBox::StandardButton reply =
                    QMessageBox::question(nullptr,
                                          "Switch to SIM mode",
                                          "Switch to SIM (paper trading) mode?\n\n" + detail,
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No);
                if (reply == QMessageBox::Yes)
                {
                    if (MainApp::isInReviewMode())
                    {
                        MainApp::getInstance()->exitReviewMode();
                    }
                    MainApp::setTradingMode(TradingMode::Sim);
                    MainApp::restartApplication();
                }
            });

    connect(m_tradingModeBar,
            &TradingModeBar::replayRequested,
            this,
            [this]()
            {
                if (MainApp::isInReviewMode())
                {
                    MainApp::getInstance()->exitReviewMode(false);
                }
                m_replayControlsBar->scanAndPopulateReplayDays();
                if (!m_replayControlsBar->getSelectedReplayDay().isValid())
                {
                    QMessageBox::warning(nullptr,
                                         "No Replay Data",
                                         "No recorded data found for replay.\n\n"
                                         "Download data in the Downloads tab first.");
                    return;
                }
                QDate replayDate = m_replayControlsBar->getSelectedReplayDay();
                QTime replayTime = m_replayControlsBar->getReplayStartTime();
                Playback::Speed speed = m_replayControlsBar->getReplaySpeed();
                const QString currentSymbol = ui->priceChart->getCurrentSymbol();
                const ReplayEntryResolution resolution = resolveReplayEntryResolution(currentSymbol, replayDate);
                if (!currentSymbol.isEmpty() && !resolution.enterWithoutSymbol &&
                    !DBClient::hasReplayData(replayDate, currentSymbol))
                {
                    qInfo() << "No local replay data found for" << currentSymbol << "on"
                            << replayDate.toString(Qt::ISODate)
                            << "- entering replay with the selected date and symbol";
                }
                if (resolution.enterWithoutSymbol)
                {
                    if (currentSymbol.isEmpty())
                    {
                        qInfo() << "Entering replay without a displayed symbol";
                    }
                    else
                    {
                        qInfo() << "No local replay data found for" << currentSymbol
                                << "on any date; entering replay without a displayed symbol";
                    }
                }
                m_pendingReplayEntrySymbol = resolution.enterWithoutSymbol ? QString("") : currentSymbol;
                MainApp::getInstance()->enterReplayMode(replayDate, replayTime, speed, m_pendingReplayEntrySymbol);
            });

    connect(m_tradingModeBar,
            &TradingModeBar::replayExitRequested,
            this,
            []() { MainApp::getInstance()->exitReplayMode(); });

    connect(m_tradingModeBar,
            &TradingModeBar::reviewRequested,
            this,
            [this]()
            {
                if (MainApp::isInReplayMode())
                {
                    MainApp::getInstance()->exitReplayMode();
                }

                m_reviewSessionBar->scanAndPopulateSessions();
                const QString sessionId = m_reviewSessionBar->getSelectedSessionId();
                if (sessionId.isEmpty())
                {
                    QMessageBox::warning(nullptr,
                                         "No Review Sessions",
                                         "No migrated replay ledgers were found for Review mode.");
                    return;
                }

                MainApp::getInstance()->enterReviewMode(sessionId);
            });

    connect(m_reviewSessionBar->sessionComboBox(),
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](const int index)
            {
                if (index < 0)
                {
                    return;
                }

                const QString sessionId = m_reviewSessionBar->getSelectedSessionId();
                if (sessionId.isEmpty())
                {
                    return;
                }

                saveReviewState(MainApp::isInReviewMode(), sessionId);
                if (!MainApp::isInReviewMode() || MainApp::getReviewSessionId() == sessionId)
                {
                    return;
                }

                MainApp::getInstance()->exitReviewMode(false);
                MainApp::getInstance()->enterReviewMode(sessionId);
            });

    // Set up timer to update clock every second in LIVE mode
    m_timeUpdateTimer = new QTimer(this);
    Q_CHECK_PTR(m_timeUpdateTimer);
    bool connected = connect(m_timeUpdateTimer, &QTimer::timeout, this, &GUIFrontend::updateTimeDisplay);
    ASSUME_TRUE(connected);
    m_timeUpdateTimer->start(1000); // Update every second
    updateTimeDisplay();            // Initial update

    // 30 Hz pull-based display refresh timer — GUI reads latest snapshot from SymbolContext
    connected = connect(&m_displayRefreshTimer, &QTimer::timeout, this, &GUIFrontend::onDisplayRefreshTick);
    ASSUME_TRUE(connected);
    m_displayRefreshTimer.start(ReplayConstants::GUI_THROTTLE_INTERVAL_MS);

    // In LIVE trading mode, the REPLAY pill is still shown (greyed out) — no hiding needed.
    // Connect app frontend signals and slots
    connect(this,
            &GUIFrontend::tradeStationAuthStateChanged,
            this,
            &GUIFrontend::onTradeStationAuthStateChanged,
            Qt::DirectConnection);

    connect(this,
            &GUIFrontend::tradeStationAccountsReceived,
            this,
            &GUIFrontend::onTradeStationAccountsReceived,
            Qt::DirectConnection);

    connect(this,
            &GUIFrontend::tradeStationDataUsageUpdated,
            this,
            &GUIFrontend::onTSClientDataUsageUpdate,
            Qt::DirectConnection);

    connect(this,
            &GUIFrontend::databentoDataUsageUpdated,
            this,
            &GUIFrontend::onDBClientDataUsageUpdate,
            Qt::DirectConnection);

    connect(this, &GUIFrontend::newPositionReceived, this, &GUIFrontend::onNewPositionReceived, Qt::DirectConnection);

    connect(this, &GUIFrontend::positionDeleted, this, &GUIFrontend::onPositionDeleted, Qt::DirectConnection);

    connect(this, &GUIFrontend::newOrderReceived, this, &GUIFrontend::onNewOrderReceived, Qt::DirectConnection);

    connect(this, &GUIFrontend::balanceUpdated, this, &GUIFrontend::onBalanceUpdated, Qt::DirectConnection);

    // When the chart requests missing bars, call the extracted method to handle the request
    connect(ui->priceChart, &StockPriceChart::requestMissingBars, this, &GUIFrontend::requestMissingBarsFromCache);
    connect(ui->priceChart,
            &StockPriceChart::adjustManagedBracketRequested,
            this,
            [](const QString& p_symbol,
               const bool p_adjustStop,
               const double p_stopPrice,
               const bool p_adjustTake,
               const double p_takePrice)
            {
                if (p_symbol.isEmpty())
                {
                    return;
                }

                MainAlgo* const mainAlgoInstance = MainAlgo::getInstance();
                ASSUME_DIFF(mainAlgoInstance, nullptr);

                const std::optional<double> stop = p_adjustStop ? std::optional<double>(p_stopPrice) : std::nullopt;
                const std::optional<double> take = p_adjustTake ? std::optional<double>(p_takePrice) : std::nullopt;
                if (!stop.has_value() && !take.has_value())
                {
                    return;
                }

                const QString symbol = p_symbol.trimmed().toUpper();
                QMetaObject::invokeMethod(
                    mainAlgoInstance,
                    [mainAlgoInstance, symbol, stop, take]()
                    {
                        ASSUME_DIFF(mainAlgoInstance, nullptr);
                        const QString accountID = mainAlgoInstance->getActiveAccountId().trimmed().toUpper();
                        if (accountID.isEmpty())
                        {
                            return;
                        }
                        mainAlgoInstance->processAdjustManagedBracketLevels(accountID,
                                                                            symbol,
                                                                            stop,
                                                                            take,
                                                                            QStringLiteral("chart-drag-adjust"));
                    },
                    Qt::QueuedConnection);
            });
    connect(ui->priceChart,
            &StockPriceChart::manualArmedBracketAdjusted,
            this,
            &GUIFrontend::onManualArmedBracketAdjusted,
            Qt::QueuedConnection);

    // Connect chart toolbar timescale selector
    connect(ui->priceChart->toolbar(),
            &ChartToolbar::timeFrameChanged,
            this,
            &GUIFrontend::onTimeFrameChanged,
            Qt::UniqueConnection);

    // Persist auto-timescale mode toggle
    connect(ui->priceChart->toolbar(),
            &ChartToolbar::autoTimeFrameChanged,
            this,
            [this](bool enabled)
            {
                Q_CHECK_PTR(appStateSettings);
                appStateSettings->setValue("Chart/AutoTimeFrame", enabled);
                if (!enabled)
                {
                    // Also persist the currently selected TF when switching auto off
                    appStateSettings->setValue("Chart/TimeFrame", static_cast<int>(m_currentTimeFrame));
                }
                appStateSettings->sync();
            });

    // Persist replay date/start time whenever they change so next launch restores them
    connect(m_replayControlsBar,
            &ReplayControlsBar::replayDayChanged,
            this,
            [this](const QDate& date)
            {
                if (!date.isValid())
                {
                    return;
                }
                Q_CHECK_PTR(appStateSettings);
                appStateSettings->setValue("Replay/Date", date.toString(Qt::ISODate));
                appStateSettings->sync();
            });
    connect(m_replayControlsBar,
            &ReplayControlsBar::replayStartTimeChanged,
            this,
            [this](const QTime& time)
            {
                Q_CHECK_PTR(appStateSettings);
                appStateSettings->setValue("Replay/StartTime", time.toString(Qt::ISODate));
                appStateSettings->sync();
            });
    connect(m_replayControlsBar,
            &ReplayControlsBar::replayRestartRequested,
            this,
            &GUIFrontend::onReplayRestartRequested);

    // Forward strategy log markers to the chart
    connect(MainAlgo::getInstance(),
            &MainAlgo::strategyLogEmitted,
            ui->priceChart,
            &StockPriceChart::onStrategyLogEmitted,
            Qt::QueuedConnection);
    connect(MainAlgo::getInstance(),
            &MainAlgo::strategyStatusEmitted,
            ui->priceChart,
            &StockPriceChart::onStrategyStatusEmitted,
            Qt::QueuedConnection);
    connect(
        MainAlgo::getInstance(),
        &MainAlgo::strategyDisplaySymbolRequested,
        this,
        [this](const QString& strategyID, const QString& symbol, const QString& reason)
        {
            const QString normalizedSymbol = symbol.trimmed().toUpper();
            const QString currentSymbol = currentlyDisplayedSymbol.trimmed().toUpper();
            if (normalizedSymbol.isEmpty())
            {
                qWarning(GUIFrontendLog) << "Ignoring strategy chart-switch request with empty symbol."
                                         << "strategyID=" << strategyID << "reason="
                                         << (reason.trimmed().isEmpty() ? QStringLiteral("<none>") : reason.trimmed());
                return;
            }

            if (normalizedSymbol == currentSymbol)
            {
                qCDebug(GUIFrontendLog) << "Ignoring strategy chart-switch request because symbol is already displayed."
                                        << "strategyID=" << strategyID << "symbol=" << normalizedSymbol << "reason="
                                        << (reason.trimmed().isEmpty() ? QStringLiteral("<none>") : reason.trimmed());
                return;
            }

            qInfo(GUIFrontendLog) << "Strategy requested chart symbol switch:" << "strategyID=" << strategyID
                                  << "symbol=" << normalizedSymbol << "reason=" << reason;
            displayStock(normalizedSymbol);
        },
        Qt::QueuedConnection);

    // Forward managed bracket overlay updates to the chart
    connect(MainAlgo::getInstance(),
            &MainAlgo::managedBracketOverlayEmitted,
            ui->priceChart,
            &StockPriceChart::onStrategyBracketOverlayEmitted,
            Qt::QueuedConnection);
    connect(MainAlgo::getInstance(),
            &MainAlgo::managedBracketOverlayEmitted,
            this,
            &GUIFrontend::onManagedBracketOverlayEvent,
            Qt::QueuedConnection);
    connect(MainAlgo::getInstance(),
            &MainAlgo::managedBracketProtectionDropped,
            this,
            &GUIFrontend::onManagedBracketProtectionDropped,
            Qt::QueuedConnection);
    connect(MainAlgo::getInstance(),
            &MainAlgo::riskStatusChanged,
            this,
            &GUIFrontend::onRiskStatusChanged,
            Qt::QueuedConnection);

    // Connect the stock symbol input to its slot
    connect(ui->stockSymbolInput, &QLineEdit::returnPressed, this, &GUIFrontend::onNewDisplayedStockSelection);

    connect(ui->accountSelector,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](const int)
            {
                if (m_manualArmedBracket.has_value())
                {
                    const QString selectedAccount = getSelectedAccountId().trimmed().toUpper();
                    if (selectedAccount.isEmpty() || selectedAccount != m_manualArmedBracket->accountID)
                    {
                        clearManualArmedBracket(QStringLiteral("account-changed"));
                    }
                }

                refreshRiskWidgetsForSelectedAccount();
            });

    // Make the stock symbol input convert text to uppercase
    connect(ui->stockSymbolInput,
            &QLineEdit::textChanged,
            [this](const QString& text)
            {
                QString upper = text.toUpper();
                if (upper != text)
                {
                    int pos = ui->stockSymbolInput->cursorPosition();
                    ui->stockSymbolInput->blockSignals(true);
                    ui->stockSymbolInput->setText(upper);
                    ui->stockSymbolInput->setCursorPosition(pos);
                    ui->stockSymbolInput->blockSignals(false);
                }
            });

    // Center the text in the stock symbol input
    ui->stockSymbolInput->setAlignment(Qt::AlignCenter);

    // Sync m_currentTimeFrame from the toolbar which already restored its state from AppState
    m_currentTimeFrame = ui->priceChart->toolbar()->getCurrentTimeFrame();
    updateTenSecondTimeFrameAvailability();
    // Scale candlestick widths to match the restored timescale (1m default if nothing was saved)
    ui->priceChart->setDisplayTimeFrame(m_currentTimeFrame);

    // Connect position window symbol click
    connect(ui->positionWidget,
            &PositionWidget::symbolClicked,
            this,
            [this](const QString& symbol)
            {
                ui->stockSymbolInput->setText(symbol);
                ui->stockSymbolInput->returnPressed(); // Simulate Enter key press
            });

    connect(ui->positionWidget,
            &PositionWidget::closeAllPositionsRequested,
            this,
            &GUIFrontend::onCloseAllPositions,
            Qt::UniqueConnection);
    connect(ui->positionWidget,
            &PositionWidget::closeAllPositionsPassiveRequested,
            this,
            &GUIFrontend::onCloseAllPositionsPassive,
            Qt::UniqueConnection);
    connect(ui->positionWidget,
            &PositionWidget::closePositionRequested,
            this,
            &GUIFrontend::onClosePosition,
            Qt::UniqueConnection);

    // Connect order window symbol click
    connect(ui->orderWidget,
            &OrderWidget::symbolClicked,
            this,
            [this](const QString& symbol)
            {
                ui->stockSymbolInput->setText(symbol);
                ui->stockSymbolInput->returnPressed(); // Simulate Enter key press
            });

    connect(ui->orderWidget,
            &OrderWidget::cancelAllOrdersRequested,
            this,
            &GUIFrontend::onCancelAllOrders,
            Qt::UniqueConnection);

    // Connect order window cancel order request
    connect(ui->orderWidget,
            &OrderWidget::cancelOrderRequested,
            this,
            [this](const QString& orderId)
            {
                qCDebug(GUIFrontendLog) << "Cancel order requested for order ID:" << orderId;

                // Cancel the order using TSClient
                auto cancelFuture = TSClient::getInstance()->cancelOrder(orderId);

                // Handle the result asynchronously
                cancelFuture.then(
                    this,
                    [this, orderId](std::expected<CancelOrderResult, TSClient::Error> result)
                    {
                        if (!result.has_value())
                        {
                            const QString errorMsg = QString("Failed to cancel order %1.\nError code: %2")
                                                         .arg(orderId)
                                                         .arg(static_cast<int>(result.error()));
                            qCWarning(GUIFrontendLog) << errorMsg;
                            QMessageBox::warning(nullptr, "Cancel Order Failed", errorMsg);
                            return;
                        }

                        const CancelOrderResult& cancelResult = result.value();

                        if (cancelResult.isError())
                        {
                            const QString cancelMessage = cancelResult.getMessage().isEmpty()
                                                              ? QString("Unknown cancellation error")
                                                              : cancelResult.getMessage();
                            const QString errorMsg =
                                QString("Failed to cancel order %1.\nReason: %2").arg(orderId, cancelMessage);
                            qCWarning(GUIFrontendLog) << errorMsg;
                            QMessageBox::warning(nullptr, "Cancel Order Failed", errorMsg);
                            return;
                        }

                        qCInfo(GUIFrontendLog)
                            << "Order" << orderId << "cancelled successfully:" << cancelResult.getMessage();
                    });
            });

    // Connect order entry widget
    ui->orderEntryWidget->setGUIFrontend(this);
    auto orderEntryConnection = connect(ui->orderEntryWidget,
                                        &OrderEntryWidget::orderPlaced,
                                        this,
                                        &GUIFrontend::onOrderPlaced,
                                        Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(orderEntryConnection);

    // Wire StrategyQuickView to StrategyManager signals
    StrategyManager* stratMgr = mainAlgo->getStrategyManager();
    connect(stratMgr,
            &StrategyManager::strategyLoaded,
            ui->strategyQuickView,
            &StrategyQuickView::onStrategyLoaded,
            Qt::QueuedConnection);
    connect(stratMgr,
            &StrategyManager::strategyUnloaded,
            ui->strategyQuickView,
            &StrategyQuickView::onStrategyUnloaded,
            Qt::QueuedConnection);
    connect(stratMgr,
            &StrategyManager::strategyStatusChanged,
            ui->strategyQuickView,
            &StrategyQuickView::onStrategyStatusChanged,
            Qt::QueuedConnection);
    connect(stratMgr,
            &StrategyManager::symbolsClaimed,
            ui->strategyQuickView,
            &StrategyQuickView::onSymbolsClaimed,
            Qt::QueuedConnection);
    connect(ui->strategyQuickView,
            &StrategyQuickView::symbolSelected,
            this,
            &GUIFrontend::displayStock,
            Qt::UniqueConnection);

    // Set up the downloads tab
    m_downloadsTab = new DownloadsTab();
    ui->tabWidget->addTab(m_downloadsTab, "Downloads");
    connect(m_downloadsTab,
            &DownloadsTab::downloadBatchFinished,
            this,
            [this]() { m_replayControlsBar->scanAndPopulateReplayDays(); });

    // Set up the logging tab
    LoggingTab* loggingTab = new LoggingTab();
    ui->tabWidget->addTab(loggingTab, "Logging");

    // Connect logging tab signals
    connect(loggingTab, &LoggingTab::loggerVisibilityChanged, this, &GUIFrontend::onLoggerVisibilityChanged);
    connect(loggingTab, &LoggingTab::logDepthChanged, this, &GUIFrontend::onLogDepthChanged);

    // Set initial logger visibility and log depth based on persisted settings
    // Note: visibility is handled in the logger split block below after reparenting.
    if (ui->liveLogDisplay)
    {
        Q_CHECK_PTR(appStateSettings);
        int logDepth = appStateSettings->value("Logging/LogDepth", 1000).toInt();
        maxLiveLogLines = logDepth;
    }

    // Set up the cache tab
    CacheTab* cacheTab = new CacheTab();
    ui->tabWidget->addTab(cacheTab, "Cache");

    // Set up the shortcuts tab
    ShortcutsTab* shortcutsTab = new ShortcutsTab();
    ui->tabWidget->addTab(shortcutsTab, "Shortcuts");

    // Set up the risk tab
    m_riskTab = new RiskTab();
    // Keep Risk Management immediately after Trade for quick access during execution.
    ui->tabWidget->insertTab(1, m_riskTab, "Risk Management");
    connect(m_riskTab, &RiskTab::riskConfigChanged, this, &GUIFrontend::onRiskTabConfigChanged);
    connect(m_riskTab, &RiskTab::resetRiskDayRequested, this, &GUIFrontend::onRiskTabResetDayRequested);
    connect(m_riskTab, &RiskTab::unlockRiskRequested, this, &GUIFrontend::onRiskTabUnlockRequested);

    // Set up the config tab
    ConfigTab* configTab = new ConfigTab();
    configTab->setTimeAndSalesWidget(ui->timeAndSalesWidget);
    ui->tabWidget->addTab(configTab, "Config");

    QWidget* credentialsTab = new QWidget();
    credentialsTab->setObjectName("credentialsTab");
    auto* credentialsTabLayout = new QVBoxLayout(credentialsTab);
    credentialsTabLayout->setContentsMargins(0, 0, 0, 0);
    auto* credentialsScrollArea = new QScrollArea(credentialsTab);
    credentialsScrollArea->setWidgetResizable(true);
    credentialsScrollArea->setFrameShape(QFrame::NoFrame);
    auto* credentialsContent = new QWidget(credentialsScrollArea);
    credentialsScrollArea->setWidget(credentialsContent);
    credentialsTabLayout->addWidget(credentialsScrollArea);
    auto* credentialsLayout = new QVBoxLayout(credentialsContent);
    credentialsLayout->setContentsMargins(12, 12, 12, 12);
    credentialsLayout->setSpacing(12);
    auto* backendRow = new QHBoxLayout();
    auto* backendTitle = new QLabel("Credential protection", credentialsTab);
    auto* keyringLabel = new QLabel("OS Keyring", credentialsTab);
    auto* yubiKeyLabel = new QLabel("YubiKey", credentialsTab);
    auto* backendSlider = new QSlider(Qt::Horizontal, credentialsTab);
    backendSlider->setObjectName("credentialBackendSlider");
    backendSlider->setAccessibleName("Credential storage: OS Keyring or YubiKey");
    backendSlider->setRange(0, 1);
    backendSlider->setFixedWidth(72);
    backendSlider->setValue(SecureStorage::configuredBackend() == SecureStorage::Backend::YubiKey ? 1 : 0);
    backendSlider->setToolTip(
        "Choose storage for the next launch. Stores are independent; credentials are not copied.");
    backendSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 20px; border-radius: 10px; background: #45566b; }"
        "QSlider::handle:horizontal { width: 22px; margin: -2px 0; border-radius: 11px; background: #67c4ef; }");
    backendRow->addWidget(backendTitle);
    backendRow->addSpacing(16);
    backendRow->addWidget(keyringLabel);
    backendRow->addWidget(backendSlider);
    backendRow->addWidget(yubiKeyLabel);
    backendRow->addStretch();
    credentialsLayout->addLayout(backendRow);
    auto* backendStatus = new QLabel(credentialsTab);
    backendStatus->setWordWrap(true);
    const auto updateBackendStatus = [backendStatus]()
    {
        const bool pending = SecureStorage::configuredBackend() != SecureStorage::activeBackend();
        backendStatus->setText(
            QString("Active this session: %1. %2")
                .arg(
                    SecureStorage::backendName(),
                    pending
                        ? "Restart the application to apply your selection; existing connections are unchanged."
                        : "OS Keyring and YubiKey have separate credentials; switching does not migrate or delete them."));
    };
    updateBackendStatus();
    credentialsLayout->addWidget(backendStatus);
    auto* yubiKeyHelp = new QLabel(
        "<h3>Prepare a YubiKey (one-time setup)</h3>"
        "<p>The app never programs your key. Run these commands yourself in a terminal, with only the intended "
        "YubiKey connected. You need an <b>OTP-capable YubiKey</b>; Security Key models that only support "
        "FIDO are not compatible.</p>"
        "<ol>"
        "<li>Install YubiKey Manager. On Debian/Ubuntu: <code>sudo apt install yubikey-manager</code>.</li>"
        "<li>Plug in your key and inspect its OTP slots: <code>ykman otp info</code>.<br>"
        "<b>Stop if Slot 2 is already programmed and you do not know what uses it.</b> "
        "Programming it replaces its existing credential and may break another app or lock you out of its data. "
        "If it already has a touch-required HMAC-SHA1 credential intended for this vault, skip programming.</li>"
        "<li>Only for an unused Slot 2 (or one you deliberately want to replace), run:<br>"
        "<code>ykman otp chalresp --touch --generate 2</code><br>"
        "Read the confirmation before accepting. This generates a new random HMAC-SHA1 secret and requires touch "
        "for challenge-response. <b>Do not run it again after creating your vault.</b></li>"
        "<li>The command prints the generated secret. Treat it like a password: never paste it into app fields, "
        "logs, screenshots, or support requests. If you keep a recovery copy, store it securely offline; "
        "anyone with it can reproduce the vault's encryption key without your YubiKey. "
        "The secret cannot be read back from the key.</li>"
        "<li>Select <b>YubiKey</b> above, then <b>restart OpenTraderPlatform</b>. "
        "Keep the key connected and touch it when the app prompts. If an unlock fails, use "
        "<b>Unlock / retry YubiKey</b>.</li>"
        "<li>Use <b>Login to TradeStation</b> and enter your Databento API key again as needed. "
        "The YubiKey vault starts empty: your existing OS Keyring credentials are not copied or deleted.</li>"
        "</ol>"
        "<p><b>During use:</b> one touch unlocks both services for the session, including token refreshes. "
        "Removing the key afterward does not stop trading. Losing the key or reprogramming Slot 2 can make "
        "the vault unrecoverable; retain a way to reauthenticate with your providers.</p>",
        credentialsContent);
    yubiKeyHelp->setObjectName("yubiKeySetupInstructions");
    yubiKeyHelp->setTextFormat(Qt::RichText);
    yubiKeyHelp->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    yubiKeyHelp->setWordWrap(true);
    credentialsLayout->addWidget(yubiKeyHelp);
    auto* unlockButton = new QPushButton("Unlock / retry YubiKey", credentialsTab);
    unlockButton->setObjectName("unlockYubiKeyButton");
    unlockButton->setEnabled(SecureStorage::activeBackend() == SecureStorage::Backend::YubiKey);
    credentialsLayout->addWidget(unlockButton, 0, Qt::AlignLeft);
    connect(unlockButton,
            &QPushButton::clicked,
            this,
            [this, backendStatus]()
            {
                logInputEvent(u"GUIFrontend", u"unlock-yubikey-credentials");
                QString error;
                if (!SecureStorage::unlockYubiKey(error))
                {
                    QMessageBox::warning(m_mainWindow, "YubiKey unlock failed", error);
                    return;
                }
                DBClient::getInstance()->loadApiKey();
                backendStatus->setText("YubiKey vault unlocked for this session. Use Login to TradeStation to connect. "
                                       "Removing the key will not interrupt existing connections.");
            });
    connect(
        backendSlider,
        &QSlider::valueChanged,
        this,
        [this, backendSlider, updateBackendStatus](int p_value)
        {
            const auto backend = p_value == 1 ? SecureStorage::Backend::YubiKey : SecureStorage::Backend::OSKeyring;
            logInputEvent(u"GUIFrontend",
                          u"select-credential-storage",
                          {inputDetail(u"backend", p_value == 1 ? QString("YubiKey") : QString("OS Keyring"))});
            if (backend == SecureStorage::Backend::YubiKey && QStandardPaths::findExecutable("ykman").isEmpty())
            {
                QMessageBox::warning(
                    m_mainWindow,
                    "YubiKey support unavailable",
                    "Install yubikey-manager (which provides ykman) before selecting YubiKey storage.");
                const QSignalBlocker blocker(backendSlider);
                backendSlider->setValue(SecureStorage::configuredBackend() == SecureStorage::Backend::YubiKey ? 1 : 0);
                return;
            }
            if (!SecureStorage::configureBackend(backend))
            {
                QMessageBox::warning(m_mainWindow, "Credential storage", "Could not save the storage preference.");
                const QSignalBlocker blocker(backendSlider);
                backendSlider->setValue(SecureStorage::configuredBackend() == SecureStorage::Backend::YubiKey ? 1 : 0);
            }
            updateBackendStatus();
        });
    auto* resetYubiKeyButton = new QPushButton("Reset YubiKey vault and exit...", credentialsTab);
    resetYubiKeyButton->setObjectName("resetYubiKeyVaultButton");
    resetYubiKeyButton->setToolTip(
        "Discard the local encrypted credentials after losing or reprogramming your key. "
        "OS Keyring credentials are untouched. This is not a broker logout or token revocation.");
    credentialsLayout->addWidget(resetYubiKeyButton, 0, Qt::AlignLeft);
    connect(resetYubiKeyButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                logInputEvent(u"GUIFrontend", u"request-reset-yubikey-vault");
                const auto answer = QMessageBox::warning(
                    m_mainWindow,
                    "Reset YubiKey vault?",
                    "Permanently delete ALL locally saved TradeStation credentials/tokens and the Databento API key "
                    "in the YubiKey vault?\n\n"
                    "This is useful after reprogramming Slot 2 or losing the original key. "
                    "The old vault cannot be recovered without its original secret.\n\n"
                    "The platform will close. On your next launch, unlock with your current key, log in to "
                    "TradeStation and enter your Databento key again.\n\n"
                    "OS Keyring credentials remain untouched. Broker-side tokens are not revoked, and "
                    "existing orders or positions are NOT cancelled or closed.",
                    QMessageBox::Yes | QMessageBox::Cancel,
                    QMessageBox::Cancel);
                if (answer != QMessageBox::Yes)
                {
                    return;
                }
                logInputEvent(u"GUIFrontend", u"confirm-reset-yubikey-vault");
                QString error;
                if (!SecureStorage::resetYubiKey(error))
                {
                    QMessageBox::warning(m_mainWindow, "YubiKey vault reset failed", error);
                    return;
                }
                m_mainWindow->close();
            });
    credentialsLayout->addWidget(m_connectivitySection, 0, Qt::AlignTop | Qt::AlignLeft);
    credentialsLayout->addStretch(1);
    const int configTabIndex = ui->tabWidget->indexOf(configTab);
    if (configTabIndex >= 0)
    {
        ui->tabWidget->insertTab(configTabIndex + 1, credentialsTab, "Credentials");
    }
    else
    {
        ui->tabWidget->addTab(credentialsTab, "Credentials");
    }

    // QTabWidget minimum size is influenced by all tab pages. Some pages (Risk/Config) can have
    // tall minimum-size hints, which can unintentionally clamp the whole main window height.
    // Keep pages shrinkable so smaller / mixed-DPI monitors can still show the full window frame.
    auto relaxTabPageHeightConstraint = [this](QWidget* p_tabPage)
    {
        if (!p_tabPage)
        {
            return;
        }
        p_tabPage->setMinimumSize(0, 0);
        if (p_tabPage != ui->tab)
        {
            // Non-trade tabs (notably Risk/Config) can report tall minimum-size
            // hints from dense form content. Ignore those hints so they do not
            // clamp the entire main window height on smaller monitors.
            p_tabPage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
        }
        if (QLayout* const layout = p_tabPage->layout())
        {
            layout->setSizeConstraint(QLayout::SetNoConstraint);
        }
    };
    ui->tabWidget->setMinimumSize(0, 0);
    for (int tabIndex = 0; tabIndex < ui->tabWidget->count(); ++tabIndex)
    {
        relaxTabPageHeightConstraint(ui->tabWidget->widget(tabIndex));
    }

    m_riskStatusWidget = ui->riskStatusWidget;
    Q_CHECK_PTR(m_riskStatusWidget);
    refreshRiskWidgetsForSelectedAccount();

    // Set up the live log display at the bottom
    if (ui->liveLogDisplay)
    {
        QFont font("Monospace");
        font.setPointSize(9);
        ui->liveLogDisplay->setFont(font);

        // Connect to the log broadcaster
        connect(&LogBroadcaster::instance(),
                &LogBroadcaster::logMessageReceived,
                this,
                &GUIFrontend::updateLiveLogDisplay,
                Qt::QueuedConnection);
    }

    // Split the bottom logger area: wrap liveLogDisplay in a horizontal splitter
    // so a per-strategy log panel can be shown on the right on demand.
    {
        // Build: [m_loggerContainer (QWidget)]
        //           └─ [m_loggerSplitter (QSplitter, Horizontal)]
        //                  ├─ liveLogDisplay   (left – always visible)
        //                  └─ m_strategyLogWidget (right – hidden until "Display Logs")
        m_loggerContainer = new QWidget(m_mainWindow);
        auto* containerLayout = new QVBoxLayout(m_loggerContainer);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);
        containerLayout->setSizeConstraint(QLayout::SetNoConstraint);
        m_loggerContainer->setMinimumHeight(0);

        m_loggerSplitter = new QSplitter(Qt::Horizontal, m_loggerContainer);
        m_loggerSplitter->setHandleWidth(4);
        containerLayout->addWidget(m_loggerSplitter);

        // Reparent liveLogDisplay into the horizontal splitter
        if (ui->liveLogDisplay)
        {
            // Remove from its current parent (mainSplitter), re-add to loggerSplitter
            ui->liveLogDisplay->setParent(m_loggerSplitter);
            ui->liveLogDisplay->setMinimumHeight(0);
            m_loggerSplitter->addWidget(ui->liveLogDisplay);
        }

        m_strategyLogWidget = new StrategyLogWidget(mainAlgo, m_loggerSplitter);
        m_loggerSplitter->addWidget(m_strategyLogWidget);
        m_strategyLogWidget->hide();

        // Give platform log all the width by default
        m_loggerSplitter->setStretchFactor(0, 1);
        m_loggerSplitter->setStretchFactor(1, 0);

        // Replace liveLogDisplay with the container in mainSplitter
        ui->mainSplitter->addWidget(m_loggerContainer);

        // Propagate show/hide for initial visibility state
        Q_CHECK_PTR(appStateSettings);
        bool loggerVisible = appStateSettings->value("Logging/LoggerVisible", true).toBool();
        m_loggerContainer->setVisible(loggerVisible);
    }

    // Connect strategy log display requests from StrategyQuickView
    connect(ui->strategyQuickView,
            &StrategyQuickView::displayLogsRequested,
            this,
            [this](const QString& strategyID, const QString& strategyName)
            {
                OBJ_ASSUME_DIFF(m_strategyLogWidget, nullptr);
                m_strategyLogWidget->setStrategy(strategyID, strategyName);
                m_strategyLogWidget->show();
                // Expand right panel to ~40% of the logger area if currently collapsed
                if (m_loggerSplitter && m_loggerSplitter->sizes().at(1) == 0)
                {
                    int total = m_loggerSplitter->width();
                    m_loggerSplitter->setSizes({total * 6 / 10, total * 4 / 10});
                }
            });

    // Collapse strategy log panel when its close button is pressed
    connect(m_strategyLogWidget,
            &StrategyLogWidget::closeRequested,
            this,
            [this]()
            {
                OBJ_ASSUME_DIFF(m_strategyLogWidget, nullptr);
                m_strategyLogWidget->clearStrategy();
                m_strategyLogWidget->hide();
            });

    // Provide StrategyQuickView with access to MainAlgo (for position polling + context menu)
    ui->strategyQuickView->setMainAlgo(mainAlgo);

    // Configure splitters/layout constraints to keep the main window shrinkable on smaller monitors.
    // Trade tab still prefers chart-first sizing, but both panes can collapse when vertical space is tight.
    ui->verticalLayout->setSizeConstraint(QLayout::SetNoConstraint);
    ui->verticalLayout->setStretch(0, 0); // top controls row
    ui->verticalLayout->setStretch(1, 1); // main splitter row gets remaining height
    ui->centralwidget->setMinimumSize(0, 0);
    m_mainWindow->setMinimumSize(0, 0);
    ui->tradeTabMainLayout->setContentsMargins(0, 0, 0, 0);
    ui->tradeTabMainLayout->setSpacing(0);
    ui->tradeTabSplitter->setHandleWidth(1);
    ui->tradeTabMainLayout->setSizeConstraint(QLayout::SetNoConstraint);
    // Allow collapsing when the window is resized short (e.g. smaller secondary monitors / mixed DPI setups).
    // Without this, fixed-height children (L2/risk/order entry panes) can bubble up as a hard global minimum.
    ui->tradeTabSplitter->setChildrenCollapsible(true);
    ui->tradeTabSplitter->setCollapsible(0, true);
    ui->tradeTabSplitter->setCollapsible(1, true);
    ui->tradeTopWidget->setMinimumHeight(0);
    ui->tradeTabBottomWidget->setMinimumHeight(0);
    ui->tradeTabSplitter->setStretchFactor(0,
                                           1); // tradeTopWidget gets stretch factor 1
    ui->tradeTabSplitter->setStretchFactor(1,
                                           0); // tradeTabBottomWidget gets stretch factor 0 (minimum size)
    ui->mainSplitter->setChildrenCollapsible(true);
    ui->mainSplitter->setCollapsible(0, true);
    ui->mainSplitter->setCollapsible(1, true);
    ui->mainSplitter->setStretchFactor(0, 1); // tab widget area
    ui->mainSplitter->setStretchFactor(1, 0); // logger area
    ui->mainSplitter->setMinimumHeight(0);
    if (QWidget* const topPane = ui->mainSplitter->widget(0))
    {
        topPane->setMinimumHeight(0);
    }
    if (QWidget* const bottomPane = ui->mainSplitter->widget(1))
    {
        bottomPane->setMinimumHeight(0);
    }

    // NOTE: Don't restore the last displayed stock here - wait for authentication
    // It will be restored in onTradeStationAuthStateChanged() when authenticated.
    // As a fallback, schedule a restore in case TS auth never fires (e.g. no credentials).
    QTimer::singleShot(
        500,
        this,
        [this]()
        {
            if (!m_hasRestoredLastStock)
            {
                qInfo(GUIFrontendLog) << "TS auth never fired — restoring state via fallback timer";
                m_hasRestoredLastStock = true;
                restoreLastDisplayedStock();
                restoreReviewState();
                if (!MainApp::isInReviewMode())
                {
                    restoreReplayState();
                }
                // Restore strategies AFTER replay mode is set up (enterReplayMode posts
                // stopAllStrategies via QueuedConnection; this queues behind it).
                if (!MainApp::isInReviewMode())
                {
                    QMetaObject::invokeMethod(mainAlgo, &MainAlgo::restoreStrategiesState, Qt::QueuedConnection);
                }
                QTimer::singleShot(0, m_mainWindow, [this]() { m_mainWindow->setFocus(); });
            }
        });

    // --- WindowManager: secondary chart windows ---
    m_windowManager = new WindowManager(mainAlgo, this);

    // Ctrl+T — open a new chart (panel in existing window, or new window if none)
    // ApplicationShortcut so it works even when a secondary ChartWindow has focus
    m_newChartWindowShortcut =
        new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::OpenNewChart), m_mainWindow);
    m_newChartWindowShortcut->setContext(Qt::ApplicationShortcut);
    connected = connect(m_newChartWindowShortcut,
                        &QShortcut::activated,
                        this,
                        [this]() { m_windowManager->openNewChart(currentlyDisplayedSymbol); });
    ASSUME_TRUE(connected);

    // Forward strategy log to all chart windows
    connect(MainAlgo::getInstance(),
            &MainAlgo::strategyLogEmitted,
            this,
            [this](const StrategyLogEntry& entry)
            {
                for (ChartWindow* cw: m_windowManager->chartWindows())
                    cw->onStrategyLogReceived(entry);
            });
    connect(MainAlgo::getInstance(),
            &MainAlgo::strategyStatusEmitted,
            this,
            [this](const StrategyStatusEntry& entry)
            {
                for (ChartWindow* cw: m_windowManager->chartWindows())
                {
                    cw->onStrategyStatusReceived(entry);
                }
            });

    connect(MainAlgo::getInstance(),
            &MainAlgo::managedBracketOverlayEmitted,
            this,
            [this](const StrategyBracketOverlayEntry& entry)
            {
                for (ChartWindow* cw: m_windowManager->chartWindows())
                {
                    cw->onStrategyBracketOverlayReceived(entry);
                }
            });

    // Restore chart windows from previous session (after a short delay so main window is settled)
    QTimer::singleShot(600, this, [this]() { m_windowManager->restoreWindowState(); });
}

GUIFrontend::~GUIFrontend()
{
    // Note: window state is saved before shutdown begins (in eventFilter / Ctrl+Q handler).
    // Do NOT save here — chart windows may already be destroyed by this point.
    QCoreApplication::instance()->removeEventFilter(this);

    // Delete main window explicitly (owns all child widgets via Qt parent-child)
    delete m_mainWindow;
    // ui is automatically deleted by std::unique_ptr
    // m_windowManager is deleted by Qt parent-child (parent = this)
}

void GUIFrontend::setupDarkTheme(QMainWindow* p_mainWindow)
{
    // Define the dark theme palette
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(150, 150, 150));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(150, 150, 150));

    // Apply the dark palette to the application
    p_mainWindow->setPalette(darkPalette);
    qApp->setPalette(darkPalette);

    // Set stylesheet for specific widgets and components
    QString styleSheet = R"(
        QWidget {
            background-color: #333333;
            color: #FFFFFF;
        }
        QMenuBar {
            background-color: #444444;
        }
        QMenuBar::item:selected {
            background-color: #555555;
        }
        QMenu {
            background-color: #444444;
            border: 1px solid #555555;
        }
        QMenu::item:selected {
            background-color: #555555;
        }
        QToolBar {
            background-color: #444444;
            border: none;
        }
        QToolButton {
            background-color: #444444;
            border: none;
        }
        QToolButton:hover {
            background-color: #555555;
        }
        QStatusBar {
            background-color: #333333;
            color: #CCCCCC;
        }
        QTextEdit, QLineEdit {
            background-color: #222222;
            color: #FFFFFF;
            border: 1px solid #555555;
        }
        QTabWidget::pane {
            border: 1px solid #555555;
        }
        QTabBar::tab {
            background-color: #333333;
            color: #CCCCCC;
            border: 1px solid #555555;
            padding: 5px;
        }
        QTabBar::tab:selected {
            background-color: #444444;
            color: #FFFFFF;
        }
        QScrollBar:vertical {
            background-color: #333333;
            width: 10px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background-color: #666666;
            min-height: 20px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar:horizontal {
            background-color: #333333;
            height: 10px;
            margin: 0px;
        }
        QScrollBar::handle:horizontal {
            background-color: #666666;
            min-width: 20px;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QHeaderView::section {
            background-color: #444444;
            color: #FFFFFF;
            padding: 5px;
            border: 1px solid #555555;
        }
        QTableView {
            gridline-color: #555555;
            background-color: #222222;
            color: #FFFFFF;
        }
        QTableView::item:selected {
            background-color: #3A6EA5;
        }
        QComboBox {
            background-color: #444444;
            color: #FFFFFF;
            border: 1px solid #555555;
            padding: 2px;
        }
        QComboBox::drop-down {
            background-color: #555555;
        }
        QComboBox QAbstractItemView {
            background-color: #444444;
            color: #FFFFFF;
        }
        QPushButton {
            background-color: #444444;
            color: #FFFFFF;
            border: 1px solid #555555;
            padding: 4px 8px;
        }
        QPushButton:hover {
            background-color: #555555;
        }
        QPushButton:pressed {
            background-color: #666666;
        }
        QCheckBox, QRadioButton {
            color: #FFFFFF;
        }
        QLabel {
            color: #FFFFFF;
        }
    )";

    qApp->setStyleSheet(styleSheet);
}

void GUIFrontend::updateStatusBar()
{
    QString message = "TS: " + bytesToString(TSClientDataUsage) + "\nDB: " + bytesToString(m_dbClientDataUsage) +
                      "\nMem: " + bytesToString(memoryUsage);

    if (m_dataUsageLabel != nullptr)
    {
        m_dataUsageLabel->setText(message);
    }
}

void GUIFrontend::onTSClientDataUsageUpdate(qsizetype newDataUsage)
{
    TSClientDataUsage = newDataUsage;
    updateStatusBar();
}

void GUIFrontend::onDBClientDataUsageUpdate(qsizetype newDataUsage)
{
    m_dbClientDataUsage = newDataUsage;
    updateStatusBar();
}

void GUIFrontend::applyAccountsToSelector(const QVector<Account>& p_accounts, const QString& p_preferredAccountId)
{
    m_accounts = p_accounts;

    ui->accountSelector->clear();
    for (const Account& account: p_accounts)
    {
        ui->accountSelector->addItem(
            QString("%1 (%2)").arg(account.getAccountId(),
                                   AccountType::accountTypeToString(account.getAccountType().type)),
            account.getAccountId());
    }

    int selectedIndex = -1;
    if (!p_preferredAccountId.isEmpty())
    {
        for (int i = 0; i < p_accounts.size(); ++i)
        {
            if (p_accounts[i].getAccountId() == p_preferredAccountId)
            {
                selectedIndex = i;
                break;
            }
        }
    }

    if (selectedIndex == -1 && !p_accounts.isEmpty())
    {
        selectedIndex = p_accounts.size() - 1;
    }

    if (selectedIndex != -1)
    {
        ui->accountSelector->setCurrentIndex(selectedIndex);
    }

    m_accountInfoButton->setVisible(!p_accounts.isEmpty());
    ui->orderEntryWidget->setAccounts(p_accounts);
    refreshRiskWidgetsForSelectedAccount();
}

void GUIFrontend::onTradeStationAccountsReceived(QVector<Account> results)
{
    if (MainApp::isInReplayMode())
    {
        const bool containsReplayAccount =
            std::ranges::any_of(results,
                                [](const Account& p_account)
                                { return p_account.getAccountId() == QString::fromLatin1(REPLAY_SIM_ACCOUNT_ID); });
        if (!containsReplayAccount)
        {
            qCWarning(GUIFrontendLog) << "Ignoring non-replay account list while replay mode is active";
            return;
        }
    }

    const QString currentSelection = getSelectedAccountId();
    QString preferredAccountId;

    if (MainApp::isInReplayMode())
    {
        preferredAccountId = QString::fromLatin1(REPLAY_SIM_ACCOUNT_ID);
    }
    else if (!m_preReplaySelectedAccountId.isNull())
    {
        preferredAccountId = m_preReplaySelectedAccountId;
    }
    else
    {
        preferredAccountId = currentSelection;
    }

    applyAccountsToSelector(results, preferredAccountId);

    if (!MainApp::isInReplayMode())
    {
        m_preReplaySelectedAccountId = QString();
    }
}

QString GUIFrontend::getSelectedAccountId() const
{
    if (ui->accountSelector->currentIndex() >= 0 && ui->accountSelector->currentIndex() < m_accounts.size())
    {
        return m_accounts[ui->accountSelector->currentIndex()].getAccountId();
    }
    return QString(); // Return empty string if no valid selection
}

void GUIFrontend::refreshRiskWidgetsForSelectedAccount()
{
    const QString selectedAccountId = getSelectedAccountId().trimmed().toUpper();

    if (m_riskTab != nullptr)
    {
        m_riskTab->setSelectedAccountId(selectedAccountId);
    }

    if (selectedAccountId.isEmpty())
    {
        m_riskRefreshPending = false;
        m_riskRefreshPendingAccountId.clear();

        RiskStatusSnapshot emptySnapshot;
        if (m_riskTab != nullptr)
        {
            m_riskTab->setRiskStatus(emptySnapshot);
        }
        if (m_riskStatusWidget != nullptr)
        {
            m_riskStatusWidget->setSnapshot(emptySnapshot);
        }
        return;
    }

    requestRiskWidgetsRefresh(selectedAccountId);
}

void GUIFrontend::requestRiskWidgetsRefresh(const QString& p_accountId)
{
    const QString accountId = p_accountId.trimmed().toUpper();
    if (accountId.isEmpty())
    {
        return;
    }

    m_riskRefreshPendingAccountId = accountId;
    if (m_riskRefreshInFlight)
    {
        m_riskRefreshPending = true;
        return;
    }

    m_riskRefreshInFlight = true;
    m_riskRefreshPending = false;

    QPointer<GUIFrontend> frontend(this);
    const bool invoked = QMetaObject::invokeMethod(
        mainAlgo,
        [algo = mainAlgo, frontend, accountId]()
        {
            ASSUME_DIFF(algo, nullptr);
            if (frontend.isNull())
            {
                return;
            }

            const RiskConfig config = algo->getRiskConfigForAccount(accountId);
            RiskStatusSnapshot snapshot = algo->getRiskStatusSnapshotForAccount(accountId);
            snapshot.accountId = accountId;

            QMetaObject::invokeMethod(
                frontend.data(),
                [frontend, accountId, config, snapshot]()
                {
                    if (frontend.isNull())
                    {
                        return;
                    }
                    frontend->applyRiskWidgetsRefreshResult(accountId, config, snapshot);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);

    if (!invoked)
    {
        m_riskRefreshInFlight = false;
        m_riskRefreshPending = false;
        m_riskRefreshPendingAccountId.clear();
        qCWarning(GUIFrontendLog) << "Failed to dispatch risk widget refresh to MainAlgo";
    }
}

void GUIFrontend::applyRiskWidgetsRefreshResult(const QString& p_accountId,
                                                const RiskConfig& p_config,
                                                const RiskStatusSnapshot& p_snapshot)
{
    m_riskRefreshInFlight = false;
    m_riskConfigByAccount.insert(p_accountId, p_config);

    const QString selectedAccountId = getSelectedAccountId().trimmed().toUpper();
    if (!selectedAccountId.isEmpty() && selectedAccountId == p_accountId)
    {
        RiskStatusSnapshot snapshot = p_snapshot;
        snapshot.accountId = selectedAccountId;

        if (m_riskTab != nullptr)
        {
            m_riskTab->setRiskConfig(p_config);
            m_riskTab->setRiskStatus(snapshot);
        }
        if (m_riskStatusWidget != nullptr)
        {
            m_riskStatusWidget->setSnapshot(snapshot);
        }
    }

    if (!m_riskRefreshPending)
    {
        return;
    }

    const QString pendingAccountId = m_riskRefreshPendingAccountId;
    m_riskRefreshPending = false;
    m_riskRefreshPendingAccountId.clear();
    if (!pendingAccountId.isEmpty())
    {
        requestRiskWidgetsRefresh(pendingAccountId);
    }
}

void GUIFrontend::onRiskStatusChanged(const QString& p_accountId)
{
    const QString selectedAccountId = getSelectedAccountId().trimmed().toUpper();
    if (selectedAccountId.isEmpty())
    {
        return;
    }

    if (p_accountId.trimmed().toUpper() == selectedAccountId)
    {
        refreshRiskWidgetsForSelectedAccount();
    }
}

void GUIFrontend::onRiskTabConfigChanged(const QString& p_accountId)
{
    if (m_riskTab == nullptr)
    {
        return;
    }

    const QString accountId = p_accountId.trimmed().toUpper();
    if (accountId.isEmpty())
    {
        return;
    }

    const RiskConfig config = m_riskTab->currentConfig();
    m_riskConfigByAccount.insert(accountId, config);
    const bool invoked = QMetaObject::invokeMethod(
        mainAlgo,
        [algo = mainAlgo, accountId, config]()
        {
            ASSUME_DIFF(algo, nullptr);
            algo->setRiskConfigForAccount(accountId, config);
        },
        Qt::QueuedConnection);
    ASSUME_TRUE(invoked);
    refreshRiskWidgetsForSelectedAccount();
}

void GUIFrontend::onRiskTabResetDayRequested(const QString& p_accountId)
{
    const QString accountId = p_accountId.trimmed().toUpper();
    if (accountId.isEmpty())
    {
        return;
    }

    const bool invoked = QMetaObject::invokeMethod(
        mainAlgo,
        [algo = mainAlgo, accountId]()
        {
            ASSUME_DIFF(algo, nullptr);
            algo->resetRiskDayForAccount(accountId);
        },
        Qt::QueuedConnection);
    ASSUME_TRUE(invoked);
    refreshRiskWidgetsForSelectedAccount();
}

void GUIFrontend::onRiskTabUnlockRequested(const QString& p_accountId)
{
    const QString accountId = p_accountId.trimmed().toUpper();
    if (accountId.isEmpty())
    {
        return;
    }

    const bool invoked = QMetaObject::invokeMethod(
        mainAlgo,
        [algo = mainAlgo, accountId]()
        {
            ASSUME_DIFF(algo, nullptr);
            algo->unlockRiskForAccount(accountId);
        },
        Qt::QueuedConnection);
    ASSUME_TRUE(invoked);
    refreshRiskWidgetsForSelectedAccount();
}

std::expected<DBClient::ReplayDownloadBatchResult, QString>
GUIFrontend::startReplayDownloadBatch(const QDate& p_date, const QStringList& p_symbols)
{
    if (m_downloadsTab == nullptr)
    {
        return std::unexpected("Downloads tab is not available");
    }

    return m_downloadsTab->startExternalDownloadBatch(p_date, p_symbols);
}

void GUIFrontend::onMemoryUsageUpdate(qsizetype newDataUsage)
{
    memoryUsage = newDataUsage;
    updateStatusBar();
}

// ---------------------------------------------------------------------------
// Pull-based 30 Hz display refresh (reads DisplaySnapshot from SymbolContext)
// ---------------------------------------------------------------------------
void GUIFrontend::onDisplayRefreshTick()
{
    // --- Main window snapshot refresh ---
    QPointer<SymbolContext> sc = mainAlgo->getDisplayedSymbolContext();
    if (sc)
    {
        DisplaySnapshot& snap = sc->m_displaySnapshot;
        QWriteLocker lock(&snap.lock);

        int dirtyFlags = (snap.l2Dirty ? 1 : 0) | (snap.tradeDirty ? 2 : 0) | (snap.barDirty ? 4 : 0) |
                         (snap.aggregatorDirty ? 8 : 0) | (snap.replayTimeDirty ? 16 : 0);

        if (dirtyFlags != 0)
        {
            LTTnG_TP(opentraderplatform, gui_pull_tick, dirtyFlags);

            const auto isStaleDisplayedPayload = [this, &sc]() -> bool
            { return sc->symbol != ui->priceChart->getCurrentSymbol(); };

            if (snap.l2Dirty)
            {
                Level2 l2 = *snap.latestLevel2;
                snap.l2Dirty = false;
                lock.unlock();

                if (!isStaleDisplayedPayload())
                {
                    LTTnG_TP(opentraderplatform, gui_level2_received, sc->symbol.toUtf8().constData());
                    ui->level2Widget->updateData(l2.m_bids, l2.m_asks);
                    ui->orderEntryWidget->onMarketDepthUpdate(sc->symbol, l2);
                    ui->priceChart->onLevel2Update(sc->symbol, l2);
                }

                lock.relock();
            }

            if (snap.tradeDirty)
            {
                QVector<Trade> trades;
                trades.swap(snap.pendingTrades);
                snap.tradeDirty = false;
                lock.unlock();

                if (!isStaleDisplayedPayload())
                {
                    for (const auto& t: trades)
                    {
                        LTTnG_TP(opentraderplatform, gui_trade_received, sc->symbol.toUtf8().constData());
                        ui->timeAndSalesWidget->onNewTrade(sc->symbol, t);
                    }
                }

                lock.relock();
            }

            if (snap.barDirty && m_currentTimeFrame == TimeFrame::ONE_MINUTE)
            {
                Bar bar = *snap.latestBar;
                snap.barDirty = false;
                lock.unlock();

                if (!isStaleDisplayedPayload())
                {
                    LTTnG_TP(opentraderplatform, gui_bar_received, sc->symbol.toUtf8().constData(), 60);
                    ui->priceChart->addLiveBar(sc->symbol, bar);
                }

                lock.relock();
            }

            if (snap.aggregatorDirty)
            {
                auto it = snap.aggregatorBars.find(m_currentTimeFrame);
                if (it != snap.aggregatorBars.end())
                {
                    Bar bar = it.value();
                    lock.unlock();

                    if (!isStaleDisplayedPayload())
                    {
                        LTTnG_TP(opentraderplatform,
                                 gui_bar_received,
                                 sc->symbol.toUtf8().constData(),
                                 BarUtils::secondsPerBar(m_currentTimeFrame));
                        ui->priceChart->addLiveBar(sc->symbol, bar);
                    }

                    lock.relock();
                }
                snap.aggregatorDirty = false;
            }

            if (snap.replayTimeDirty)
            {
                QDateTime replayTime = *snap.replayTime;
                snap.replayTimeDirty = false;
                lock.unlock();

                LTTnG_TP(opentraderplatform, gui_replay_time_updated, replayTime.toMSecsSinceEpoch());
                updateSessionLabel();
                updateTimeDisplay();
            }
        }
    } // end main window snapshot

    // --- Secondary chart windows (each reads from its own SymbolContext snapshot) ---
    for (ChartWindow* cw: m_windowManager->chartWindows())
        cw->refreshAllPanels();
}

void GUIFrontend::onNewPositionReceived(QString account, Position position)
{
    LTTnG_TP(opentraderplatform, gui_position_received, position.getSymbol().toUtf8().constData());

    ui->positionWidget->updatePosition(account, position);
    const QString positionID = position.getPositionID().trimmed();
    if (!positionID.isEmpty())
    {
        m_positionsById.insert(positionID, position);
    }

    // Forward position to chart for visualization
    // Only process positions for the currently displayed symbol
    if (position.getSymbol() == ui->priceChart->getCurrentSymbol())
    {
        int quantity = position.getQuantity().toInt();

        if (quantity == 0)
        {
            // Position closed (round-trip complete)
            ui->priceChart->onPositionClosed(position);
        }
        else
        {
            // Open or updated position
            ui->priceChart->onPositionUpdated(position);
        }
    }

    // Forward to secondary chart windows (each filters by symbol internally)
    for (ChartWindow* cw: m_windowManager->chartWindows())
        cw->onPositionReceived(position);
}

void GUIFrontend::onPositionDeleted(QString account, QString positionID)
{
    ui->positionWidget->onPositionDeleted(account, positionID);
    m_positionsById.remove(positionID.trimmed());
}

void GUIFrontend::onNewOrderReceived(QString account, Order order)
{
    LTTnG_TP(opentraderplatform,
             gui_order_received,
             order.getSymbol().toUtf8().constData(),
             static_cast<int>(order.getOrderStatus()));

    ui->orderWidget->updateOrder(account, order);
    maybeActivateManualArmedBracketFromOrderUpdate(order);

    // Forward order to chart for visualization
    // Only process orders for the currently displayed symbol
    if (order.getSymbol() == ui->priceChart->getCurrentSymbol())
    {
        Order::Status status = order.getOrderStatus();

        if (status == Order::Status::OPN || status == Order::Status::ACK)
        {
            // Pending order (sent or acknowledged)
            ui->priceChart->onOrderPlaced(order);
        }
        else if (status == Order::Status::FLL || status == Order::Status::FLP || status == Order::Status::FPR)
        {
            // Filled or partial fill
            ui->priceChart->onOrderFilled(order);
        }
        else if (isCancelLikeStatus(status))
        {
            // Cancelled
            ui->priceChart->onOrderCancelled(order);
        }
        else if (status == Order::Status::UCH || status == Order::Status::RSN)
        {
            // Amended (replaced)
            ui->priceChart->onOrderAmended(order);
        }
    }

    // Forward to secondary chart windows (each filters by symbol internally)
    for (ChartWindow* cw: m_windowManager->chartWindows())
        cw->onOrderReceived(order);
}

void GUIFrontend::onBalanceUpdated(Balance balance)
{
    ui->balanceWidget->updateBalance(balance);
}

void GUIFrontend::onTradeStationAuthStateChanged(bool isAuthenticated,
                                                 TSClient::AuthStateReason reason,
                                                 QString message)
{
    static bool isFirstTime = true;

    if (isAuthenticated)
    {
        tradeStationLoginButton->setText("TradeStation Connected");
        tradeStationLoginButton->setStyleSheet(
            "QPushButton { background-color: #E6FFE6; color: #4CAF50; padding: 2px 6px; border-radius: 3px; }");
        tradeStationLoginButton->setEnabled(true);

        // Restore the last displayed stock now that we're authenticated
        // Only do this once on the first successful authentication
        if (!m_hasRestoredLastStock)
        {
            m_hasRestoredLastStock = true;
            restoreLastDisplayedStock();
            restoreReviewState();
            if (!MainApp::isInReviewMode())
            {
                restoreReplayState();
            }
            // Restore strategies AFTER replay mode is set up (enterReplayMode posts
            // stopAllStrategies via QueuedConnection; this queues behind it).
            if (!MainApp::isInReviewMode())
            {
                QMetaObject::invokeMethod(mainAlgo, &MainAlgo::restoreStrategiesState, Qt::QueuedConnection);
            }
            // Clear focus from the stock input after restore — it should not
            // have keyboard focus at startup (press 'i' to focus it explicitly)
            QTimer::singleShot(0, m_mainWindow, [this]() { m_mainWindow->setFocus(); });
        }

        // Clear first-time flag on successful authentication
        isFirstTime = false;
    }
    else
    {
        // Check if we're in a "connecting" state using the enum
        if (reason == TSClient::AuthStateReason::Connecting)
        {
            // Show connecting state with orange/yellow color and disable button
            tradeStationLoginButton->setText("Connecting to TradeStation...");
            tradeStationLoginButton->setStyleSheet(
                "QPushButton { background-color: #FFF4E6; color: #FF9800; padding: 2px 6px; border-radius: 3px; }");
            tradeStationLoginButton->setEnabled(false);
            // Don't change isFirstTime - let connection result determine final state
        }
        else if (reason == TSClient::AuthStateReason::TokenExpired)
        {
            tradeStationLoginButton->setText("TradeStation session expired");
            tradeStationLoginButton->setStyleSheet(
                "QPushButton { background-color: #FFE6E6; color: #f44336; padding: 2px 6px; border-radius: 3px; }");
            tradeStationLoginButton->setEnabled(false);

            // Trigger the standard GUI re-authentication flow immediately.
            // Guard to avoid re-opening modal auth windows if retries report repeated auth failures.
            if (!TSClient::getInstance()->isAuthInProgress())
            {
                TSClient::getInstance()->launchAuthProcess();
            }
        }
        else if (isFirstTime)
        {
            // If its the first time we receive this signal and its negative state, it just
            // means that at startup we are not authenticated, not that there was an error.
            // Present the normal blue button to login
            tradeStationLoginButton->setText("Login to TradeStation");
            tradeStationLoginButton->setStyleSheet(
                "QPushButton { background-color: #00A0E9; color: #ffffff; padding: 2px 6px; border-radius: 3px; }");
            tradeStationLoginButton->setEnabled(true);
            // Clear first-time flag after showing initial state
            isFirstTime = false;
        }
        else
        {
            tradeStationLoginButton->setText("Login Failed: " + message);
            tradeStationLoginButton->setStyleSheet(
                "QPushButton { background-color: #FFE6E6; color: #f44336; padding: 2px 6px; border-radius: 3px; }");
            tradeStationLoginButton->setEnabled(true);
        }
    }
}

void GUIFrontend::onTradeStationQuoteReceived(const QString& symbol, const Quote& quote)
{
    if (symbol != currentlyDisplayedSymbol)
    {
        return;
    }

    const MarketFlags& flags = quote.getMarketFlags();
    const QString haltedReason = flags.isHalted() ? QStringLiteral("TradeStation status") : QString();
    ui->priceChart->toolbar()->setHalted(flags.isHalted(), haltedReason);
    ui->priceChart->toolbar()->setDelayed(flags.isDelayed());
    ui->priceChart->toolbar()->setHardToBorrow(flags.isHardToBorrow());
}

void GUIFrontend::onDatabentoConnectionStateChanged(bool isConnected)
{
    if (isConnected)
    {
        m_databentoButton->setText("Databento Connected");
        m_databentoButton->setStyleSheet(
            "QPushButton { background-color: #E6FFE6; color: #4CAF50; padding: 2px 6px; border-radius: 3px; }");
    }
    else
    {
        m_databentoButton->setText("Connect to Databento");
        m_databentoButton->setStyleSheet(
            "QPushButton { background-color: #00A0E9; color: #ffffff; padding: 2px 6px; border-radius: 3px; }");
    }
    m_databentoButton->setEnabled(true);
}

void GUIFrontend::onDatabentoStatusUpdate(const QString& symbol, bool isHalted, const QString& haltReason, bool isSsr)
{
    if (symbol != currentlyDisplayedSymbol)
        return;

    ui->priceChart->toolbar()->setHalted(isHalted, haltReason);
    ui->priceChart->toolbar()->setHardToBorrow(isSsr);
}

void GUIFrontend::onDatabentoGatewayError(const QString& errorText, bool isFatal)
{
    if (isFatal)
    {
        // Update the button to a warning/error state
        m_databentoButton->setText("Databento: Live Data Error");
        m_databentoButton->setStyleSheet(
            "QPushButton { background-color: #FF8C00; color: #ffffff; padding: 2px 6px; border-radius: 3px; }");
        m_databentoButton->setEnabled(true);

        // Show a prominent dialog so the user cannot miss it
        m_hasShownDatabentoLiveFailureDialog = true;
        QMessageBox msgBox(m_mainWindow);
        msgBox.setWindowTitle("Databento Live Data Error");
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText("<b>Databento live streaming is unavailable.</b>");
        msgBox.setInformativeText(
            "Databento returned an error during startup that prevents the live streaming session from opening:\n\n" +
            errorText +
            "\n\nYou may still be able to load on-demand historical bars for opened symbols if Databento historical "
            "access is available, but continuous live streaming, full depth, and status data are not active.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    }
    else
    {
        WARNING << "Non-fatal Databento gateway message:" << errorText;

        if (!errorText.startsWith("Falling back from "))
        {
            return;
        }

        if (m_hasShownDatabentoLiveFailureDialog)
        {
            return;
        }

        m_databentoButton->setText("Databento: Basic Mode");
        m_databentoButton->setStyleSheet(
            "QPushButton { background-color: #FFD166; color: #1f1f1f; padding: 2px 6px; border-radius: 3px; }");
        m_databentoButton->setEnabled(true);

        if (m_hasShownDatabentoFallbackWarning)
        {
            return;
        }

        m_hasShownDatabentoFallbackWarning = true;
        INFO << "Databento full-depth feed unavailable at startup; basic mode fallback is active.";
    }
}

QString GUIFrontend::bytesToString(qint64 bytes)
{
    if (bytes >= 1024 * 1024)
    {
        double megabytes = static_cast<double>(bytes) / (1024 * 1024);
        return QString("%1 MB").arg(megabytes, 0, 'f', 2);
    }
    else if (bytes >= 1024)
    {
        double kilobytes = static_cast<double>(bytes) / 1024;
        return QString("%1 KB").arg(kilobytes, 0, 'f', 2);
    }
    else
    {
        return QString("%1 bytes").arg(bytes);
    }
}

bool GUIFrontend::isValidStockSymbol(const QString& symbol) const
{
    // Check if symbol is empty
    if (symbol.isEmpty())
    {
        return false;
    }

    // Check for leading or trailing whitespace
    if (symbol != symbol.trimmed())
    {
        return false;
    }

    // Check length (typical stock symbols are 1-10 characters)
    if (symbol.length() > 10 || symbol.length() < 1)
    {
        return false;
    }

    // Check for valid characters: alphanumeric, dots, hyphens, slashes
    QRegularExpression validSymbolRegex("^[A-Z0-9.\\-/]+$");
    if (!validSymbolRegex.match(symbol).hasMatch())
    {
        return false;
    }

    return true;
}

void GUIFrontend::onNewDisplayedStockSelection()
{
    QString symbol = ui->stockSymbolInput->text().toUpper();
    logInputEvent(u"GUIFrontend", u"submit-symbol-selection", {inputDetail(u"symbol", symbol)});

    // Validate the stock symbol
    if (!isValidStockSymbol(symbol))
    {
        logInputEvent(u"GUIFrontend", u"reject-symbol-selection", {inputDetail(u"symbol", symbol)});
        QMessageBox::warning(nullptr,
                             "Invalid Symbol",
                             "Please enter a valid stock symbol.\n\n"
                             "Valid symbols:\n"
                             "- Must not be empty\n"
                             "- Must not contain leading or trailing spaces\n"
                             "- Must be 1-10 characters long\n"
                             "- Can only contain letters, numbers, dots (.), hyphens (-), and slashes (/)");
        ui->stockSymbolInput->setFocus();
        ui->stockSymbolInput->selectAll();
        return;
    }

    if (symbol == currentlyDisplayedSymbol)
    {
        ui->stockSymbolInput->clearFocus();
        return;
    }

    const bool shouldValidateOnServer = !MainApp::isInReplayMode() && !MainApp::isInReviewMode();
    if (!shouldValidateOnServer)
    {
        displayStock(symbol);
        ui->stockSymbolInput->clearFocus();
        return;
    }

    const uint64_t validationToken = ++m_symbolSelectionValidationToken;
    const QString previousSymbol = currentlyDisplayedSymbol;
    TSClient::getInstance()
        ->getBars(symbol, 1, TSClient::BarUnit::Minute, 1, TSClient::BarSessionTemplate::USEQ24Hour)
        .then(this,
              [this, symbol, previousSymbol, validationToken](
                  std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error> result)
              {
                  if (validationToken != m_symbolSelectionValidationToken)
                  {
                      return;
                  }

                  if (!result.has_value() && result.error() == TSClient::Error::RejectedByValidator)
                  {
                      logInputEvent(u"GUIFrontend", u"reject-symbol-selection", {inputDetail(u"symbol", symbol)});
                      QMessageBox::warning(nullptr,
                                           "Unknown Symbol",
                                           QString("Symbol '%1' was rejected by TradeStation.\n\n"
                                                   "Keeping current symbol: %2")
                                               .arg(symbol, previousSymbol.isEmpty() ? "<none>" : previousSymbol));
                      if (!previousSymbol.isEmpty())
                      {
                          ui->stockSymbolInput->setText(previousSymbol);
                      }
                      ui->stockSymbolInput->setFocus();
                      ui->stockSymbolInput->selectAll();
                      return;
                  }

                  // Non-validation failures (timeouts/transient API issues) should not block symbol switch.
                  displayStock(symbol);
                  ui->stockSymbolInput->clearFocus();
              });
}

void GUIFrontend::displayStock(const QString& symbol)
{
    if (symbol == currentlyDisplayedSymbol)
    {
        qWarning() << "Symbol " << symbol << " is already the currently displayed symbol";
        return;
    }

    if (m_manualArmedBracket.has_value())
    {
        clearManualArmedBracket(QStringLiteral("symbol-changed"));
    }

    currentlyDisplayedSymbol = symbol;

    // Update the stock symbol input widget to reflect the new symbol
    ui->stockSymbolInput->setText(symbol);

    // Persist so the app restores this symbol on next startup
    saveLastDisplayedStock(symbol);

    ui->priceChart->setSymbol(symbol);

    // Reset quote/trade widgets immediately on symbol switch so stale data from the
    // previously displayed symbol does not linger while the new symbol initializes.
    ui->level2Widget->clearData();
    ui->timeAndSalesWidget->clearData();

    // Reset status indicators for the new symbol (they will be updated by live status stream)
    ui->priceChart->toolbar()->setHalted(false);
    ui->priceChart->toolbar()->setDelayed(false);
    ui->priceChart->toolbar()->setHardToBorrow(false);

    // Update the order entry widget with the new symbol
    ui->orderEntryWidget->setSymbol(symbol);

    QMetaObject::invokeMethod(mainAlgo,
                              "onSelectDisplayedStock",
                              Qt::QueuedConnection,
                              Q_ARG(QString,
                                    symbol)); // Pass the symbol parameter
}

void GUIFrontend::updateLiveLogDisplay(const QString& message)
{
    if (!ui->liveLogDisplay)
    {
        return;
    }

    // Check if the user is currently at the bottom of the log
    bool wasAtBottom =
        ui->liveLogDisplay->verticalScrollBar()->value() == ui->liveLogDisplay->verticalScrollBar()->maximum();

    ui->liveLogDisplay->append(message);

    // Enforce max log lines
    QTextDocument* doc = ui->liveLogDisplay->document();
    int lineCount = doc->lineCount();

    if (lineCount > maxLiveLogLines)
    {
        QTextCursor cursor(doc);
        cursor.movePosition(QTextCursor::Start);

        // Calculate how many lines to remove
        int linesToRemove = lineCount - maxLiveLogLines;

        // Select and delete the excess lines
        for (int i = 0; i < linesToRemove; ++i)
        {
            cursor.select(QTextCursor::LineUnderCursor);
            cursor.removeSelectedText();
            cursor.deleteChar(); // Remove the newline
        }
    }

    // Only auto-scroll to bottom if the user was already at the bottom
    if (wasAtBottom)
    {
        QTextCursor cursor = ui->liveLogDisplay->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui->liveLogDisplay->setTextCursor(cursor);
    }
}

void GUIFrontend::onLoggerVisibilityChanged(bool visible)
{
    if (m_loggerContainer)
    {
        m_loggerContainer->setVisible(visible);
    }
    else if (ui->liveLogDisplay)
    {
        ui->liveLogDisplay->setVisible(visible);
    }
}

void GUIFrontend::onLogDepthChanged(int maxLines)
{
    maxLiveLogLines = maxLines;

    // Trim current log display if needed
    if (ui->liveLogDisplay)
    {
        QTextDocument* doc = ui->liveLogDisplay->document();
        int lineCount = doc->lineCount();

        if (lineCount > maxLiveLogLines)
        {
            QTextCursor cursor(doc);
            cursor.movePosition(QTextCursor::Start);

            int linesToRemove = lineCount - maxLiveLogLines;

            for (int i = 0; i < linesToRemove; ++i)
            {
                cursor.select(QTextCursor::LineUnderCursor);
                cursor.removeSelectedText();
                cursor.deleteChar();
            }
        }
    }
}

void GUIFrontend::saveLastDisplayedStock(const QString& symbol)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("UI/LastDisplayedStock", symbol);
    appStateSettings->sync();
    qInfo() << "Saved last displayed stock:" << symbol;
}

void GUIFrontend::restoreLastDisplayedStock()
{
    Q_CHECK_PTR(appStateSettings);
    QString lastSymbol = appStateSettings->value("UI/LastDisplayedStock").toString().toUpper();

    if (lastSymbol.isEmpty())
    {
        qInfo() << "No previously displayed stock to restore";
        return;
    }

    if (!isValidStockSymbol(lastSymbol))
    {
        qWarning() << "Previously saved stock symbol is invalid:" << lastSymbol;
        return;
    }

    qInfo() << "Restoring last displayed stock:" << lastSymbol;

    // Set the symbol in the input box (uppercase)
    ui->stockSymbolInput->setText(lastSymbol);

    // Reuse the same validated flow as manual symbol submission so a stale/invalid
    // persisted symbol cannot enter live-mode retry loops.
    onNewDisplayedStockSelection();
}

void GUIFrontend::saveReplayState(bool active, const QDate& date, const QTime& startTime)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("Replay/Active", active);
    if (active && date.isValid())
    {
        appStateSettings->setValue("Replay/Date", date.toString(Qt::ISODate));
    }
    // Always persist the start time so the widget restores to it on next launch,
    // regardless of whether replay is currently active.
    if (startTime.isValid())
    {
        appStateSettings->setValue("Replay/StartTime", startTime.toString(Qt::ISODate));
    }
    appStateSettings->sync();
}

void GUIFrontend::restoreReplayState()
{
    Q_CHECK_PTR(appStateSettings);

    QDate savedDate = QDate::fromString(appStateSettings->value("Replay/Date").toString(), Qt::ISODate);

    // Always refresh and restore replay date selection even if replay is currently inactive.
    m_replayControlsBar->scanAndPopulateReplayDays();
    const QDate latestAvailableDate = m_replayControlsBar->getSelectedReplayDay();
    if (latestAvailableDate.isValid())
    {
        if (!savedDate.isValid())
        {
            savedDate = latestAvailableDate;
        }
        else
        {
            m_replayControlsBar->setSelectedReplayDay(savedDate);
            if (m_replayControlsBar->getSelectedReplayDay() != savedDate)
            {
                const QDate fallbackDate = m_replayControlsBar->getSelectedReplayDay();
                qInfo() << "Saved replay date" << savedDate.toString(Qt::ISODate)
                        << "is no longer available on disk - restoring replay with latest locally available date"
                        << fallbackDate.toString(Qt::ISODate);
                savedDate = fallbackDate;
            }
        }
    }
    else
    {
        qWarning() << "No replay data is available on disk";
    }

    // Always restore the start time widget, even if replay isn't active,
    // so the user's last-used time is shown on next launch.
    QTime savedTime = QTime::fromString(appStateSettings->value("Replay/StartTime").toString(), Qt::ISODate);
    if (savedTime.isValid())
        m_replayControlsBar->setReplayStartTime(savedTime);

    const int savedReplaySpeedValue =
        appStateSettings->value("Replay/Speed", static_cast<int>(Playback::Speed::Normal)).toInt();
    const auto savedReplaySpeed =
        PlatformControlProtocol::replaySpeedFromString(QString::number(savedReplaySpeedValue));
    if (savedReplaySpeed.has_value())
    {
        m_replayControlsBar->setReplaySpeed(savedReplaySpeed.value());
    }

    if (!appStateSettings->value("Replay/Active", false).toBool())
    {
        return;
    }
    if (!latestAvailableDate.isValid())
    {
        qWarning() << "No replay data is available on disk, skipping replay restore";
        return;
    }

    if (!savedDate.isValid())
    {
        qWarning() << "Saved replay date is invalid - restoring replay with latest locally available date"
                   << latestAvailableDate.toString(Qt::ISODate);
        savedDate = latestAvailableDate;
    }
    else
    {
        m_replayControlsBar->setSelectedReplayDay(savedDate);
        if (m_replayControlsBar->getSelectedReplayDay() != savedDate)
        {
            const QDate fallbackDate = m_replayControlsBar->getSelectedReplayDay();
            qInfo() << "Saved replay date" << savedDate.toString(Qt::ISODate)
                    << "is no longer available on disk - restoring replay with latest locally available date"
                    << fallbackDate.toString(Qt::ISODate);
            savedDate = fallbackDate;
        }
    }

    if (!savedDate.isValid())
    {
        qWarning() << "No valid replay date could be restored, skipping replay restore";
        return;
    }

    const QString currentSymbol = ui->priceChart->getCurrentSymbol();
    const ReplayEntryResolution resolution = resolveReplayEntryResolution(currentSymbol, savedDate);
    m_replayControlsBar->setSelectedReplayDay(savedDate);

    QTime startTime = savedTime.isValid() ? savedTime : m_replayControlsBar->getReplayStartTime();
    Playback::Speed speed = m_replayControlsBar->getReplaySpeed();

    qInfo() << "Restoring replay state: date=" << savedDate << "time=" << startTime;
    if (resolution.enterWithoutSymbol)
    {
        if (currentSymbol.isEmpty())
        {
            qInfo() << "Restoring replay without a displayed symbol";
        }
        else
        {
            qInfo() << "Saved symbol" << currentSymbol
                    << "has no local replay data on any date; restoring replay without a displayed symbol";
        }
    }
    m_pendingReplayEntrySymbol = resolution.enterWithoutSymbol ? QString("") : currentSymbol;
    MainApp::getInstance()->enterReplayMode(savedDate, startTime, speed, m_pendingReplayEntrySymbol);
}

void GUIFrontend::saveReviewState(const bool active, const QString& sessionId)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("Review/Active", active);
    if (!sessionId.isEmpty())
    {
        appStateSettings->setValue("Review/SessionId", sessionId);
    }
    appStateSettings->sync();
}

void GUIFrontend::restoreReviewState()
{
    Q_CHECK_PTR(appStateSettings);

    m_reviewSessionBar->scanAndPopulateSessions();
    const QString savedSessionId = appStateSettings->value("Review/SessionId").toString();
    if (!savedSessionId.isEmpty())
    {
        m_reviewSessionBar->setSelectedSessionId(savedSessionId);
    }

    if (!appStateSettings->value("Review/Active", false).toBool())
    {
        return;
    }

    const QString sessionId = m_reviewSessionBar->getSelectedSessionId();
    if (sessionId.isEmpty())
    {
        qWarning() << "Saved review session is unavailable, skipping review restore";
        return;
    }

    MainApp::getInstance()->enterReviewMode(sessionId);
}

void GUIFrontend::loadReviewSessionIntoWidgets()
{
    ui->orderWidget->clearAllOrders();
    ui->positionWidget->clearAllPositions();
    m_positionsById.clear();
    ui->level2Widget->clearData();
    ui->timeAndSalesWidget->clearData();

    QSet<QString> reviewSymbols;

    OrdersDatabase* ordersDb = OrdersDatabase::getInstance();
    if (ordersDb->isOpen())
    {
        const auto orders = ordersDb->loadAllOrders();
        for (auto it = orders.constBegin(); it != orders.constEnd(); ++it)
        {
            const Order& order = std::get<0>(it.value());
            ui->orderWidget->updateOrder(order.getAccountID(), order);
            if (!order.getSymbol().isEmpty())
            {
                reviewSymbols.insert(order.getSymbol());
            }
        }
    }

    PositionsDatabase* positionsDb = PositionsDatabase::getInstance();
    if (positionsDb->isOpen())
    {
        const QMap<QString, Position> positions = positionsDb->loadAllPositions();
        for (auto it = positions.constBegin(); it != positions.constEnd(); ++it)
        {
            const Position& position = it.value();
            ui->positionWidget->updatePosition(position.getAccountID(), position);
            const QString positionID = position.getPositionID().trimmed();
            if (!positionID.isEmpty())
            {
                m_positionsById.insert(positionID, position);
            }
            if (!position.getSymbol().isEmpty())
            {
                reviewSymbols.insert(position.getSymbol());
            }
        }
    }

    QStringList symbolList = reviewSymbols.values();
    std::sort(symbolList.begin(), symbolList.end());
    ui->strategyQuickView->setReviewSymbols(symbolList);

    if (symbolList.isEmpty())
    {
        currentlyDisplayedSymbol.clear();
        ui->stockSymbolInput->clear();
        ui->priceChart->clearSymbol();
        ui->orderEntryWidget->setSymbol(QString());
        return;
    }

    QString symbolToDisplay = currentlyDisplayedSymbol;
    if (!reviewSymbols.contains(symbolToDisplay))
    {
        symbolToDisplay = symbolList.first();
    }

    currentlyDisplayedSymbol.clear();
    displayStock(symbolToDisplay);
}

void GUIFrontend::saveMainWindowGeometry()
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->beginGroup("MainWindow");
    appStateSettings->setValue("geometry", m_mainWindow->saveGeometry());
    appStateSettings->setValue("wasMaximized", m_mainWindow->isMaximized());
    if (m_mainWindow->windowHandle() && m_mainWindow->windowHandle()->screen())
        appStateSettings->setValue("screenName", m_mainWindow->windowHandle()->screen()->name());
    appStateSettings->endGroup();
    appStateSettings->sync();
}

void GUIFrontend::restoreMainWindowGeometry()
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->beginGroup("MainWindow");
    QByteArray geometry = appStateSettings->value("geometry").toByteArray();
    bool wasMaximized = appStateSettings->value("wasMaximized", true).toBool();
    QString screenName = appStateSettings->value("screenName").toString();
    appStateSettings->endGroup();

    if (geometry.isEmpty())
    {
        m_mainWindow->showMaximized();
        return;
    }

    // restoreGeometry handles window show, but we need to ensure native handle
    // exists first for screen targeting
    m_mainWindow->restoreGeometry(geometry);
    m_mainWindow->show(); // creates native window handle

    // Move to saved screen if still connected
    if (!screenName.isEmpty() && m_mainWindow->windowHandle())
    {
        for (QScreen* screen: QApplication::screens())
        {
            if (screen->name() == screenName)
            {
                m_mainWindow->windowHandle()->setScreen(screen);
                m_mainWindow->restoreGeometry(geometry); // re-apply after screen change
                break;
            }
        }
    }

    if (wasMaximized)
        m_mainWindow->showMaximized();
}

void GUIFrontend::onStrategyOrderConfirmationRequested(QString confirmationID,
                                                       QString symbol,
                                                       QString promptText,
                                                       int timeoutSec,
                                                       QString accountID,
                                                       int quantity,
                                                       bool isLongSide,
                                                       double referencePrice,
                                                       double stopPrice)
{
    confirmationID = confirmationID.trimmed();
    symbol = symbol.trimmed().toUpper();
    promptText = promptText.trimmed();
    accountID = accountID.trimmed().toUpper();
    if (confirmationID.isEmpty())
    {
        return;
    }

    if (m_strategyConfirmationAlertsMuted)
    {
        qCInfo(GUIFrontendLog) << "Manual-confirm mute active; auto-rejecting strategy confirmation" << confirmationID;
        QMetaObject::invokeMethod(
            mainAlgo,
            [algo = mainAlgo, confirmationID]()
            {
                ASSUME_DIFF(algo, nullptr);
                algo->onStrategyOrderConfirmationDecision(confirmationID, false, 0.0);
            },
            Qt::QueuedConnection);
        return;
    }

    m_activeStrategyConfirmationID = confirmationID;
    m_activeStrategyConfirmationSymbol = symbol;
    m_activeStrategyConfirmationPrompt =
        promptText.isEmpty() ? QString("REVIEW %1 FOR ENTRY").arg(symbol.isEmpty() ? currentlyDisplayedSymbol : symbol)
                             : promptText;
    m_activeStrategyConfirmationTimeoutSec = std::max(0, timeoutSec);
    m_activeStrategyConfirmationRemainingMs = static_cast<qint64>(m_activeStrategyConfirmationTimeoutSec) * 1000LL;
    m_activeStrategyConfirmationCountdownPaused = MainApp::isInReplayMode() && m_replayPlaybackPaused;
    m_activeStrategyConfirmationDeadlineUtc =
        m_activeStrategyConfirmationCountdownPaused || m_activeStrategyConfirmationRemainingMs <= 0
            ? QDateTime()
            : QDateTime::currentDateTimeUtc().addMSecs(m_activeStrategyConfirmationRemainingMs);
    m_lastStrategyConfirmationCueText.clear();
    m_strategyConfirmationInputStage = StrategyConfirmationInputStage::AwaitingDecision;
    m_replayControlsBar->setReplaySpeedControlLocked(MainApp::isInReplayMode());
    m_preConfirmationDisplayedSymbol.clear();
    m_preConfirmationChartViewRanges.reset();

    const bool hasEventSymbolToSwitch = !symbol.isEmpty() && symbol != currentlyDisplayedSymbol;
    if (hasEventSymbolToSwitch && hasOpenPositionOnDisplayedSymbolForSelectedAccount())
    {
        m_strategyConfirmationInputStage = StrategyConfirmationInputStage::AwaitingSwitchAuthorization;
    }
    else if (hasEventSymbolToSwitch)
    {
        ChartViewRangesSnapshot viewRangesSnapshot;
        if (ui->priceChart->snapshotCurrentViewRanges(viewRangesSnapshot.xLower,
                                                      viewRangesSnapshot.xUpper,
                                                      viewRangesSnapshot.yLower,
                                                      viewRangesSnapshot.yUpper))
        {
            m_preConfirmationChartViewRanges = viewRangesSnapshot;
        }
        m_preConfirmationDisplayedSymbol = currentlyDisplayedSymbol;
        displayStock(symbol);
        ui->priceChart->recenterToCurrentPriceAction(true);
    }
    else
    {
        ui->priceChart->recenterToCurrentPriceAction(true);
    }

    if (stopPrice > 0.0 && !symbol.isEmpty() && !accountID.isEmpty() && quantity > 0)
    {
        const double entryReferencePrice = referencePrice > 0.0 ? referencePrice : stopPrice;
        const double normalizedStopPrice = qMax(0.01, stopPrice);
        const double riskDistance = qMax(0.01, std::abs(entryReferencePrice - normalizedStopPrice));

        ManualArmedBracketState state;
        state.symbol = symbol;
        state.accountID = accountID;
        state.side = isLongSide ? StrategyBracketOverlayEntry::Side::Long : StrategyBracketOverlayEntry::Side::Short;
        state.stopPrice = normalizedStopPrice;
        if (state.side == StrategyBracketOverlayEntry::Side::Long)
        {
            state.takePrice = qMax(0.01, entryReferencePrice + riskDistance);
            if (state.takePrice <= state.stopPrice)
            {
                state.takePrice = state.stopPrice + 0.01;
            }
        }
        else
        {
            state.takePrice = qMax(0.01, entryReferencePrice - riskDistance);
            if (state.takePrice >= state.stopPrice)
            {
                state.takePrice = qMax(0.01, state.stopPrice - 0.01);
            }
        }
        state.referenceEntryPrice = qMax(0.01, entryReferencePrice);
        state.previewQuantity = qMax(1, quantity);
        state.armTimestamp = MainApp::getCurrentAppTime();
        state.awaitingEntryFill = false;
        state.autoActivateManagedBracketOnFill = false;
        state.source = ManualArmedBracketState::Source::StrategyConfirmation;
        state.confirmationID = confirmationID;
        state.submittedOrderIDs.clear();
        m_manualArmedBracket = state;
        updateManualArmedBracketPreviewInChart();
    }
    else if (m_manualArmedBracket.has_value() &&
             m_manualArmedBracket->source == ManualArmedBracketState::Source::StrategyConfirmation)
    {
        clearManualArmedBracket(QStringLiteral("strategy-confirmation-preview-unavailable"));
    }

    if (!m_strategyConfirmationAlertsMuted)
    {
        m_mainWindow->raise();
        m_mainWindow->activateWindow();
        ui->priceChart->setFocus();
        QApplication::beep();
    }
    else
    {
        qCInfo(GUIFrontendLog) << "Strategy confirmation alerts muted; suppressing alert cue for confirmation"
                               << confirmationID;
    }

    refreshStrategyConfirmationCueCountdown(true);
}

void GUIFrontend::onStrategyOrderConfirmationResolved(QString confirmationID)
{
    confirmationID = confirmationID.trimmed();
    if (!m_activeStrategyConfirmationID.isEmpty() && confirmationID != m_activeStrategyConfirmationID)
    {
        return;
    }

    if (m_manualArmedBracket.has_value() &&
        m_manualArmedBracket->source == ManualArmedBracketState::Source::StrategyConfirmation &&
        (confirmationID.isEmpty() || m_manualArmedBracket->confirmationID == confirmationID))
    {
        clearManualArmedBracket(QStringLiteral("strategy-confirmation-resolved"));
    }

    resetActiveStrategyConfirmationUiState();
    ui->priceChart->clearManualConfirmationCue();
    QTimer::singleShot(0,
                       this,
                       [this]()
                       {
                           if (m_activeStrategyConfirmationID.isEmpty())
                           {
                               m_replayControlsBar->setReplaySpeedControlLocked(false);
                           }
                       });
}

bool GUIFrontend::hasOpenPositionOnDisplayedSymbolForSelectedAccount() const
{
    if (currentlyDisplayedSymbol.isEmpty())
    {
        return false;
    }

    const QString selectedAccountId = getSelectedAccountId();
    if (selectedAccountId.isEmpty())
    {
        return false;
    }

    for (auto it = m_positionsById.constBegin(); it != m_positionsById.constEnd(); ++it)
    {
        const Position& position = it.value();
        if (position.getAccountID() != selectedAccountId || position.getSymbol() != currentlyDisplayedSymbol)
        {
            continue;
        }
        if (position.isDeleted())
        {
            continue;
        }

        bool quantityOk = false;
        const int quantity = position.getQuantity().toInt(&quantityOk);
        if (quantityOk && quantity != 0)
        {
            return true;
        }
    }

    return false;
}

void GUIFrontend::applyStrategyConfirmationMuteMode(const bool p_muted)
{
    m_strategyConfirmationAlertsMuted = p_muted;
    ui->priceChart->setManualConfirmationMutedWatermarkVisible(m_strategyConfirmationAlertsMuted);

    if (m_strategyConfirmationAlertsMuted)
    {
        rejectActiveStrategyConfirmationForMute();
    }
    else if (!m_activeStrategyConfirmationID.isEmpty())
    {
        refreshStrategyConfirmationCueCountdown(true);
    }

    QMetaObject::invokeMethod(
        mainAlgo,
        [algo = mainAlgo, p_muted]()
        {
            ASSUME_DIFF(algo, nullptr);
            algo->setManualOrderConfirmationsMuted(p_muted);
        },
        Qt::QueuedConnection);
}

void GUIFrontend::rejectActiveStrategyConfirmationForMute()
{
    if (m_activeStrategyConfirmationID.isEmpty())
    {
        return;
    }

    const QString confirmationID = m_activeStrategyConfirmationID;
    const QString symbolToRestore = m_preConfirmationDisplayedSymbol;
    const std::optional<ChartViewRangesSnapshot> rangesToRestore = m_preConfirmationChartViewRanges;
    const bool clearStrategyPreview =
        m_manualArmedBracket.has_value() &&
        m_manualArmedBracket->source == ManualArmedBracketState::Source::StrategyConfirmation &&
        m_manualArmedBracket->confirmationID == confirmationID;

    resetActiveStrategyConfirmationUiState();
    ui->priceChart->clearManualConfirmationCue();
    if (clearStrategyPreview)
    {
        clearManualArmedBracket(QStringLiteral("strategy-confirmation-rejected-by-mute"));
    }
    if (!symbolToRestore.isEmpty() && symbolToRestore != currentlyDisplayedSymbol)
    {
        displayStock(symbolToRestore);
        if (rangesToRestore.has_value())
        {
            ui->priceChart->applyViewRanges(rangesToRestore->xLower,
                                            rangesToRestore->xUpper,
                                            rangesToRestore->yLower,
                                            rangesToRestore->yUpper,
                                            true);
        }
    }
}

void GUIFrontend::resetActiveStrategyConfirmationUiState()
{
    m_activeStrategyConfirmationID.clear();
    m_activeStrategyConfirmationSymbol.clear();
    m_activeStrategyConfirmationPrompt.clear();
    m_activeStrategyConfirmationTimeoutSec = 0;
    m_activeStrategyConfirmationDeadlineUtc = QDateTime();
    m_activeStrategyConfirmationRemainingMs = 0;
    m_activeStrategyConfirmationCountdownPaused = false;
    m_lastStrategyConfirmationCueText.clear();
    m_strategyConfirmationInputStage = StrategyConfirmationInputStage::Idle;
    m_preConfirmationDisplayedSymbol.clear();
    m_preConfirmationChartViewRanges.reset();
}

void GUIFrontend::pauseActiveStrategyConfirmationCountdown()
{
    if (m_activeStrategyConfirmationID.isEmpty() || m_activeStrategyConfirmationCountdownPaused)
    {
        return;
    }

    if (m_activeStrategyConfirmationDeadlineUtc.isValid())
    {
        m_activeStrategyConfirmationRemainingMs =
            std::max<qint64>(0, QDateTime::currentDateTimeUtc().msecsTo(m_activeStrategyConfirmationDeadlineUtc));
    }

    m_activeStrategyConfirmationDeadlineUtc = QDateTime();
    m_activeStrategyConfirmationCountdownPaused = true;
    refreshStrategyConfirmationCueCountdown(true);
}

void GUIFrontend::resumeActiveStrategyConfirmationCountdown()
{
    if (m_activeStrategyConfirmationID.isEmpty() || !m_activeStrategyConfirmationCountdownPaused)
    {
        return;
    }

    m_activeStrategyConfirmationCountdownPaused = false;
    if (m_activeStrategyConfirmationRemainingMs > 0)
    {
        m_activeStrategyConfirmationDeadlineUtc =
            QDateTime::currentDateTimeUtc().addMSecs(m_activeStrategyConfirmationRemainingMs);
    }
    else
    {
        m_activeStrategyConfirmationDeadlineUtc = QDateTime();
    }

    refreshStrategyConfirmationCueCountdown(true);
}

QString GUIFrontend::makeConfirmationCueTextForCurrentStage() const
{
    const QString prompt =
        m_activeStrategyConfirmationPrompt.isEmpty()
            ? QString("REVIEW %1 FOR ENTRY")
                  .arg(m_activeStrategyConfirmationSymbol.isEmpty() ? currentlyDisplayedSymbol
                                                                    : m_activeStrategyConfirmationSymbol)
            : m_activeStrategyConfirmationPrompt;
    const int timeoutSec = currentStrategyConfirmationRemainingSeconds();
    const QString mutedSuffix = m_strategyConfirmationAlertsMuted
                                    ? QStringLiteral("\nManual-confirm mute active (press M to unmute).")
                                    : QString();

    if (m_strategyConfirmationInputStage == StrategyConfirmationInputStage::AwaitingSwitchAuthorization)
    {
        return QString("%1\nPress W to switch focus / N to reject / Shift+N to reject+block (%2s)%3")
            .arg(prompt)
            .arg(timeoutSec)
            .arg(mutedSuffix);
    }

    return QString("%1\nPress Y to accept / N to reject / Shift+N to reject+block (%2s)%3")
        .arg(prompt)
        .arg(timeoutSec)
        .arg(mutedSuffix);
}

int GUIFrontend::currentStrategyConfirmationRemainingSeconds() const
{
    if (m_activeStrategyConfirmationID.isEmpty())
    {
        return 0;
    }

    if (m_activeStrategyConfirmationCountdownPaused)
    {
        const qint64 remainingMs = std::max<qint64>(0, m_activeStrategyConfirmationRemainingMs);
        return static_cast<int>((remainingMs + 999) / 1000);
    }

    if (!m_activeStrategyConfirmationDeadlineUtc.isValid())
    {
        if (m_activeStrategyConfirmationRemainingMs > 0)
        {
            return static_cast<int>((m_activeStrategyConfirmationRemainingMs + 999) / 1000);
        }
        return std::max(0, m_activeStrategyConfirmationTimeoutSec);
    }

    const qint64 remainingMs = QDateTime::currentDateTimeUtc().msecsTo(m_activeStrategyConfirmationDeadlineUtc);
    if (remainingMs <= 0)
    {
        return 0;
    }

    return static_cast<int>((remainingMs + 999) / 1000);
}

void GUIFrontend::refreshStrategyConfirmationCueCountdown(const bool p_forceRefresh)
{
    if (m_activeStrategyConfirmationID.isEmpty())
    {
        return;
    }

    const QString cueText = makeConfirmationCueTextForCurrentStage();
    if (!p_forceRefresh && cueText == m_lastStrategyConfirmationCueText)
    {
        return;
    }

    m_lastStrategyConfirmationCueText = cueText;
    ui->priceChart->showManualConfirmationCue(cueText, !m_strategyConfirmationAlertsMuted);
}

bool GUIFrontend::validateActiveStrategyConfirmationPreviewRisk(QString* p_errorText) const
{
    if (!m_manualArmedBracket.has_value() ||
        m_manualArmedBracket->source != ManualArmedBracketState::Source::StrategyConfirmation ||
        m_activeStrategyConfirmationID.isEmpty() ||
        m_manualArmedBracket->confirmationID != m_activeStrategyConfirmationID)
    {
        return true;
    }

    const ManualArmedBracketState& preview = m_manualArmedBracket.value();
    if (preview.accountID.isEmpty() || preview.previewQuantity <= 0 || preview.referenceEntryPrice <= 0.0 ||
        preview.stopPrice <= 0.0)
    {
        if (p_errorText != nullptr)
        {
            *p_errorText = QStringLiteral("Confirmation preview has invalid risk inputs (account/qty/price/stop).");
        }
        return false;
    }

    const QString accountID = preview.accountID.trimmed().toUpper();
    RiskConfig riskConfig = m_riskConfigByAccount.value(accountID);
    if (m_riskTab != nullptr && accountID == getSelectedAccountId().trimmed().toUpper())
    {
        riskConfig = m_riskTab->currentConfig();
    }
    if (!riskConfig.enabled || riskConfig.maxPlannedLossPerTradeUsd <= 0.0)
    {
        return true;
    }

    const double plannedLoss =
        std::abs(preview.referenceEntryPrice - preview.stopPrice) * static_cast<double>(preview.previewQuantity);
    if (plannedLoss > riskConfig.maxPlannedLossPerTradeUsd)
    {
        if (p_errorText != nullptr)
        {
            *p_errorText = QString("Planned loss exceeds per-trade risk limit.\n\n"
                                   "Planned: $%1\n"
                                   "Limit: $%2\n\n"
                                   "Adjust the stop closer to entry before pressing Y.")
                               .arg(QString::number(plannedLoss, 'f', 2),
                                    QString::number(riskConfig.maxPlannedLossPerTradeUsd, 'f', 2));
        }
        return false;
    }

    return true;
}

bool GUIFrontend::isBuySideTradeAction(const TradeAction p_tradeAction)
{
    return p_tradeAction == TradeAction::Buy || p_tradeAction == TradeAction::BuyToCover ||
           p_tradeAction == TradeAction::BuyToOpen || p_tradeAction == TradeAction::BuyToClose;
}

bool GUIFrontend::isSellSideTradeAction(const TradeAction p_tradeAction)
{
    return p_tradeAction == TradeAction::Sell || p_tradeAction == TradeAction::SellShort ||
           p_tradeAction == TradeAction::SellToOpen || p_tradeAction == TradeAction::SellToClose;
}

GUIFrontend::ManualOpeningSide GUIFrontend::resolveOpeningSideForOrder(const PlaceOrderRequest& p_order,
                                                                       const int p_signedNetPositionShares)
{
    const int quantity = p_order.getQuantity();
    if (quantity <= 0)
    {
        return ManualOpeningSide::None;
    }

    const TradeAction action = p_order.getTradeAction();
    switch (action)
    {
    case TradeAction::BuyToOpen:
        return ManualOpeningSide::Long;
    case TradeAction::SellShort:
    case TradeAction::SellToOpen:
        return ManualOpeningSide::Short;
    case TradeAction::BuyToCover:
    case TradeAction::BuyToClose:
    case TradeAction::SellToClose:
        return ManualOpeningSide::None;
    case TradeAction::Buy:
        if (p_signedNetPositionShares < 0 && quantity <= std::abs(p_signedNetPositionShares))
        {
            return ManualOpeningSide::None;
        }
        return ManualOpeningSide::Long;
    case TradeAction::Sell:
        if (p_signedNetPositionShares > 0 && quantity <= p_signedNetPositionShares)
        {
            return ManualOpeningSide::None;
        }
        return ManualOpeningSide::Short;
    }

    return ManualOpeningSide::None;
}

int GUIFrontend::getSignedNetPositionShares(const QString& p_accountID, const QString& p_symbol) const
{
    const QString accountID = p_accountID.trimmed().toUpper();
    const QString symbol = p_symbol.trimmed().toUpper();
    if (accountID.isEmpty() || symbol.isEmpty())
    {
        return 0;
    }

    int netShares = 0;
    for (auto it = m_positionsById.constBegin(); it != m_positionsById.constEnd(); ++it)
    {
        const Position& position = it.value();
        if (position.getAccountID().trimmed().toUpper() != accountID ||
            position.getSymbol().trimmed().toUpper() != symbol)
        {
            continue;
        }
        if (position.isDeleted())
        {
            continue;
        }

        bool quantityOk = false;
        int quantity = position.getQuantity().toInt(&quantityOk);
        if (!quantityOk)
        {
            continue;
        }

        quantity = std::abs(quantity);
        if (position.getLongShort().compare(QStringLiteral("Short"), Qt::CaseInsensitive) == 0)
        {
            netShares -= quantity;
        }
        else
        {
            netShares += quantity;
        }
    }

    return netShares;
}

std::optional<double> GUIFrontend::resolveManualBracketReferencePrice(const QString& p_symbol) const
{
    const QString symbol = p_symbol.trimmed().toUpper();
    if (symbol.isEmpty())
    {
        return std::nullopt;
    }

    if (symbol == currentlyDisplayedSymbol.trimmed().toUpper())
    {
        QPointer<SymbolContext> sc = mainAlgo->getDisplayedSymbolContext();
        if (!sc.isNull())
        {
            QReadLocker lock(&sc->m_displaySnapshot.lock);
            if (!sc->m_displaySnapshot.recentTrades.isEmpty())
            {
                const double lastTrade = sc->m_displaySnapshot.recentTrades.last().m_price;
                if (lastTrade > 0.0)
                {
                    return lastTrade;
                }
            }

            if (sc->m_displaySnapshot.latestLevel2.has_value())
            {
                const Level2& level2 = sc->m_displaySnapshot.latestLevel2.value();
                if (!level2.m_bids.empty() && !level2.m_asks.empty())
                {
                    const double bestBid = level2.m_bids[0].m_price;
                    const double bestAsk = level2.m_asks[0].m_price;
                    if (bestBid > 0.0 && bestAsk > 0.0)
                    {
                        return (bestBid + bestAsk) * 0.5;
                    }
                }
            }
        }
    }

    return ui->priceChart->getLatestClosePrice();
}

void GUIFrontend::updateManualArmedBracketPreviewInChart()
{
    if (!m_manualArmedBracket.has_value())
    {
        ui->priceChart->clearManualArmedBracketOverlay();
        return;
    }

    ui->priceChart->upsertManualArmedBracketOverlay(m_manualArmedBracket->symbol,
                                                    m_manualArmedBracket->side,
                                                    m_manualArmedBracket->stopPrice,
                                                    m_manualArmedBracket->takePrice,
                                                    m_manualArmedBracket->armTimestamp,
                                                    m_manualArmedBracket->referenceEntryPrice,
                                                    m_manualArmedBracket->previewQuantity);
}

void GUIFrontend::clearManualArmedBracket(const QString& p_reason)
{
    if (!m_manualArmedBracket.has_value())
    {
        return;
    }

    if (!p_reason.isEmpty())
    {
        qCDebug(GUIFrontendLog) << "Clearing manual armed bracket for" << m_manualArmedBracket->symbol
                                << "reason:" << p_reason;
    }

    m_manualArmedBracket.reset();
    ui->priceChart->clearManualArmedBracketOverlay();
}

void GUIFrontend::handleManualBracketArmShortcut(const StrategyBracketOverlayEntry::Side p_side)
{
    if (!m_activeStrategyConfirmationID.isEmpty())
    {
        return;
    }

    const QString symbol = currentlyDisplayedSymbol.trimmed().toUpper();
    if (symbol.isEmpty())
    {
        QMessageBox::warning(nullptr, "No Symbol", "Select a symbol before arming a bracket.");
        return;
    }

    const QString accountID = getSelectedAccountId().trimmed().toUpper();
    if (accountID.isEmpty())
    {
        QMessageBox::warning(nullptr, "No Account", "Select an account before arming a bracket.");
        return;
    }

    if (m_manualArmedBracket.has_value())
    {
        if (m_manualArmedBracket->symbol == symbol && m_manualArmedBracket->accountID == accountID &&
            m_manualArmedBracket->side == p_side)
        {
            clearManualArmedBracket(QStringLiteral("shortcut-toggle-off"));
            return;
        }
        clearManualArmedBracket(QStringLiteral("shortcut-switch-side"));
    }

    const quint64 requestToken = ++m_manualBracketArmRequestToken;
    QPointer<GUIFrontend> frontend(this);
    const bool invoked = QMetaObject::invokeMethod(
        mainAlgo,
        [algo = mainAlgo, frontend, accountID, symbol, p_side, requestToken]()
        {
            ASSUME_DIFF(algo, nullptr);
            if (frontend.isNull())
            {
                return;
            }

            const bool hasManagedBracket = algo->hasManagedBracketForAccountSymbol(accountID, symbol);
            QMetaObject::invokeMethod(
                frontend.data(),
                [frontend, accountID, symbol, p_side, hasManagedBracket, requestToken]()
                {
                    if (frontend.isNull())
                    {
                        return;
                    }

                    if (frontend->m_manualBracketArmRequestToken != requestToken)
                    {
                        return;
                    }

                    if (frontend->currentlyDisplayedSymbol.trimmed().toUpper() != symbol ||
                        frontend->getSelectedAccountId().trimmed().toUpper() != accountID)
                    {
                        return;
                    }

                    if (hasManagedBracket)
                    {
                        QMessageBox::warning(nullptr,
                                             "Managed Bracket Active",
                                             "A managed bracket is already active for this symbol/account.\n\n"
                                             "Cancel or finish it before arming a manual preview bracket.");
                        return;
                    }

                    const std::optional<double> referencePrice = frontend->resolveManualBracketReferencePrice(symbol);
                    if (!referencePrice.has_value() || referencePrice.value() <= 0.0)
                    {
                        QMessageBox::warning(nullptr,
                                             "No Market Reference",
                                             "Unable to resolve a valid reference price for this symbol.\n\n"
                                             "Need last trade, best bid/ask, or a chart close to arm a bracket.");
                        return;
                    }

                    const double stopLossPercent =
                        qMax(0.0, frontend->ui->orderEntryWidget->getManualBracketStopLossPercent());
                    const double takeProfitPercent =
                        qMax(0.0, frontend->ui->orderEntryWidget->getManualBracketTakeProfitPercent());

                    double stopPrice = 0.0;
                    double takePrice = 0.0;
                    if (p_side == StrategyBracketOverlayEntry::Side::Long)
                    {
                        stopPrice = qMax(0.01, referencePrice.value() * (1.0 - (stopLossPercent / 100.0)));
                        takePrice = qMax(0.01, referencePrice.value() * (1.0 + (takeProfitPercent / 100.0)));
                        if (stopPrice >= takePrice)
                        {
                            takePrice = stopPrice + 0.01;
                        }
                    }
                    else
                    {
                        stopPrice = qMax(0.01, referencePrice.value() * (1.0 + (stopLossPercent / 100.0)));
                        takePrice = qMax(0.01, referencePrice.value() * (1.0 - (takeProfitPercent / 100.0)));
                        if (stopPrice <= takePrice)
                        {
                            takePrice = qMax(0.01, stopPrice - 0.01);
                        }
                    }

                    ManualArmedBracketState state;
                    state.symbol = symbol;
                    state.accountID = accountID;
                    state.side = p_side;
                    state.stopPrice = stopPrice;
                    state.takePrice = takePrice;
                    state.referenceEntryPrice = referencePrice.value();
                    state.previewQuantity = qMax(1, frontend->ui->orderEntryWidget->getConfiguredQuantity());
                    state.armTimestamp = MainApp::getCurrentAppTime();
                    state.awaitingEntryFill = false;
                    state.autoActivateManagedBracketOnFill = true;
                    state.source = ManualArmedBracketState::Source::ManualShortcut;
                    state.confirmationID.clear();
                    frontend->m_manualArmedBracket = state;
                    frontend->updateManualArmedBracketPreviewInChart();
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
    ASSUME_TRUE(invoked);
}

void GUIFrontend::onManualArmedBracketAdjusted(const QString& p_symbol,
                                               const double p_stopPrice,
                                               const double p_takePrice)
{
    constexpr double kPriceEpsilon = 0.000001;
    if (!m_manualArmedBracket.has_value())
    {
        return;
    }

    if (m_manualArmedBracket->symbol != p_symbol.trimmed().toUpper())
    {
        return;
    }

    const double requestedStopPrice = qMax(0.01, p_stopPrice);
    const double currentStopPrice = m_manualArmedBracket->stopPrice;
    const bool stopLoosensRisk = (m_manualArmedBracket->side == StrategyBracketOverlayEntry::Side::Long &&
                                  requestedStopPrice < currentStopPrice) ||
                                 (m_manualArmedBracket->side == StrategyBracketOverlayEntry::Side::Short &&
                                  requestedStopPrice > currentStopPrice);
    const bool stopBlockedByLock = m_stopLossTightenOnlyLockActive && stopLoosensRisk &&
                                   std::abs(requestedStopPrice - currentStopPrice) > kPriceEpsilon;

    if (!stopBlockedByLock)
    {
        m_manualArmedBracket->stopPrice = requestedStopPrice;
    }
    m_manualArmedBracket->takePrice = qMax(0.01, p_takePrice);
    if (m_manualArmedBracket->side == StrategyBracketOverlayEntry::Side::Long &&
        m_manualArmedBracket->stopPrice >= m_manualArmedBracket->takePrice)
    {
        m_manualArmedBracket->takePrice = m_manualArmedBracket->stopPrice + 0.01;
    }
    if (m_manualArmedBracket->side == StrategyBracketOverlayEntry::Side::Short &&
        m_manualArmedBracket->stopPrice <= m_manualArmedBracket->takePrice)
    {
        m_manualArmedBracket->takePrice = qMax(0.01, m_manualArmedBracket->stopPrice - 0.01);
    }

    if (stopBlockedByLock)
    {
        qCDebug(GUIFrontendLog) << "Blocked manual-armed stop adjustment for" << m_manualArmedBracket->symbol
                                << "requestedStop=" << requestedStopPrice
                                << "currentStop=" << m_manualArmedBracket->stopPrice;
    }

    updateManualArmedBracketPreviewInChart();
}

void GUIFrontend::onManagedBracketOverlayEvent(const StrategyBracketOverlayEntry& p_entry)
{
    if (!m_manualArmedBracket.has_value())
    {
        return;
    }

    if (p_entry.symbol.trimmed().toUpper() != m_manualArmedBracket->symbol)
    {
        return;
    }

    if (p_entry.action == StrategyBracketOverlayEntry::Action::Upsert)
    {
        clearManualArmedBracket(QStringLiteral("managed-bracket-upserted"));
    }
}

void GUIFrontend::onManagedBracketProtectionDropped(const QString& p_accountID,
                                                    const QString& p_symbol,
                                                    const QString& p_reason)
{
    const QString symbol = p_symbol.trimmed().toUpper();
    const QString accountID = p_accountID.trimmed().toUpper();
    const QString reason =
        p_reason.trimmed().isEmpty() ? QStringLiteral("Native bracket placement failed.") : p_reason.trimmed();

    qCWarning(GUIFrontendLog) << "Managed bracket protection dropped:" << "symbol=" << symbol << "account=" << accountID
                              << "reason=" << reason;

    QMessageBox::warning(nullptr,
                         "Managed Bracket Dropped",
                         QString("Managed bracket protection was dropped for %1 (%2).\n\n"
                                 "Reason: %3\n\n"
                                 "No stop/take protection is active for this position.")
                             .arg(symbol, accountID, reason));
}

void GUIFrontend::maybeActivateManualArmedBracketFromOrderUpdate(const Order& p_order)
{
    if (!m_manualArmedBracket.has_value() || !m_manualArmedBracket->awaitingEntryFill)
    {
        return;
    }

    if (!m_manualArmedBracket->autoActivateManagedBracketOnFill)
    {
        return;
    }

    if (p_order.getSymbol().trimmed().toUpper() != m_manualArmedBracket->symbol ||
        p_order.getAccountID().trimmed().toUpper() != m_manualArmedBracket->accountID)
    {
        return;
    }

    const QString orderID = p_order.getOrderID().trimmed();
    const Order::Status status = p_order.getOrderStatus();
    if (isCancelLikeStatus(status))
    {
        if (!orderID.isEmpty())
        {
            m_manualArmedBracket->submittedOrderIDs.remove(orderID);
        }
        if (m_manualArmedBracket->submittedOrderIDs.isEmpty())
        {
            m_manualArmedBracket->awaitingEntryFill = false;
        }
        return;
    }

    if (!isFillLikeStatus(status))
    {
        return;
    }

    if (!m_manualArmedBracket->submittedOrderIDs.isEmpty() && !orderID.isEmpty() &&
        !m_manualArmedBracket->submittedOrderIDs.contains(orderID))
    {
        return;
    }

    const bool wantsLong = m_manualArmedBracket->side == StrategyBracketOverlayEntry::Side::Long;
    if ((wantsLong && !isBuySideOrderUpdate(p_order)) || (!wantsLong && !isSellSideOrderUpdate(p_order)))
    {
        return;
    }

    const QString symbol = m_manualArmedBracket->symbol;
    const QString accountID = m_manualArmedBracket->accountID;
    const double stopPrice = m_manualArmedBracket->stopPrice;
    const double takePrice = m_manualArmedBracket->takePrice;
    const double fallbackReferenceEntryPrice = m_manualArmedBracket->referenceEntryPrice;
    const StrategyBracketSide side = wantsLong ? StrategyBracketSide::Long : StrategyBracketSide::Short;
    const double fillReferenceEntryPrice = p_order.getFilledPrice();
    const std::optional<double> referenceEntryPrice =
        fillReferenceEntryPrice > 0.0
            ? std::optional<double>(fillReferenceEntryPrice)
            : (fallbackReferenceEntryPrice > 0.0 ? std::optional<double>(fallbackReferenceEntryPrice) : std::nullopt);
    clearManualArmedBracket(QStringLiteral("entry-fill-activated"));

    QMetaObject::invokeMethod(
        mainAlgo,
        [this, symbol, accountID, side, stopPrice, takePrice, referenceEntryPrice]()
        {
            this->mainAlgo->processUpsertManagedBracket(QStringLiteral("GUI-MANUAL-ARM"),
                                                        accountID,
                                                        symbol,
                                                        side,
                                                        stopPrice,
                                                        takePrice,
                                                        StrategyBracketExecutionPolicy::Auto,
                                                        referenceEntryPrice);
        },
        Qt::QueuedConnection);
}

void GUIFrontend::onOrderPlaced(const PlaceOrderRequest& order)
{
    if (MainApp::isInReviewMode())
    {
        QMessageBox::information(nullptr, "Review Mode", "Order placement is disabled while Review mode is active.");
        return;
    }

    if (!order.isValid())
    {
        qWarning() << "Rejected invalid GUI order request:" << order.toJsonString();
        QMessageBox::warning(nullptr,
                             "Invalid Order",
                             "The order request is missing required fields.\n\n"
                             "Please reselect a symbol and try again.");
        return;
    }

    const QString normalizedAccountID = order.getAccountID().trimmed().toUpper();
    const QString normalizedSymbol = order.getSymbol().trimmed().toUpper();
    const int signedNetPositionShares = getSignedNetPositionShares(normalizedAccountID, normalizedSymbol);
    const ManualOpeningSide openingSide = resolveOpeningSideForOrder(order, signedNetPositionShares);
    const bool isOpeningOrder = openingSide != ManualOpeningSide::None;

    bool orderUsesManualArming = false;
    QDateTime manualArmTimestamp;
    std::optional<double> manualArmStopPrice;
    if (isOpeningOrder && m_manualArmedBracket.has_value() && m_manualArmedBracket->symbol == normalizedSymbol &&
        m_manualArmedBracket->accountID == normalizedAccountID)
    {
        const bool openingLong = openingSide == ManualOpeningSide::Long;
        const bool armedLong = m_manualArmedBracket->side == StrategyBracketOverlayEntry::Side::Long;
        if (openingLong != armedLong)
        {
            const QString armedSideText = armedLong ? QStringLiteral("LONG") : QStringLiteral("SHORT");
            QMessageBox::warning(nullptr,
                                 "Bracket Direction Mismatch",
                                 QString("This order direction does not match the armed %1 bracket.\n\n"
                                         "Press B or S to switch/cancel the armed bracket, then submit again.")
                                     .arg(armedSideText));
            return;
        }

        orderUsesManualArming = true;
        manualArmTimestamp = m_manualArmedBracket->armTimestamp;
        if (m_manualArmedBracket->stopPrice > 0.0)
        {
            manualArmStopPrice = m_manualArmedBracket->stopPrice;
        }
        m_manualArmedBracket->awaitingEntryFill = true;
    }

    PlaceOrderRequest orderToSubmit = order;
    if (orderUsesManualArming && manualArmStopPrice.has_value() &&
        (!orderToSubmit.getStopPrice().has_value() || orderToSubmit.getStopPrice().value() <= 0.0))
    {
        // Preserve the preview stop as risk metadata for opening orders so planned-loss checks
        // use the same stop the user just armed on chart.
        orderToSubmit.setStopPrice(manualArmStopPrice.value());
    }

    qInfo() << "Placing order:" << orderToSubmit.toJsonString();

    // Submit order to MainAlgo so all order sources share the same risk gate.
    QFuture<std::expected<PlaceOrderResult, TSClient::Error>> future = mainAlgo->placeOrder(orderToSubmit);

    future.then(
        this,
        [this, orderUsesManualArming, manualArmTimestamp, normalizedAccountID, normalizedSymbol](
            std::expected<PlaceOrderResult, TSClient::Error> expected_result)
        {
            if (orderUsesManualArming && m_manualArmedBracket.has_value() &&
                m_manualArmedBracket->armTimestamp == manualArmTimestamp &&
                m_manualArmedBracket->accountID == normalizedAccountID &&
                m_manualArmedBracket->symbol == normalizedSymbol)
            {
                if (!expected_result.has_value())
                {
                    if (m_manualArmedBracket->submittedOrderIDs.isEmpty())
                    {
                        m_manualArmedBracket->awaitingEntryFill = false;
                    }
                }
                else
                {
                    bool addedOrderID = false;
                    const PlaceOrderResult& result = expected_result.value();
                    for (const auto& orderItem: result.getOrders())
                    {
                        const QString orderID = orderItem.getOrderID().trimmed();
                        if (!orderID.isEmpty())
                        {
                            m_manualArmedBracket->submittedOrderIDs.insert(orderID);
                            addedOrderID = true;
                        }
                    }
                    if (!addedOrderID && result.hasErrors() && m_manualArmedBracket->submittedOrderIDs.isEmpty())
                    {
                        m_manualArmedBracket->awaitingEntryFill = false;
                    }
                }
            }

            // Check if result popups are enabled
            bool showPopup = ui->orderEntryWidget->isResultPopupEnabled();

            if (!expected_result.has_value())
            {
                QString errorMsg = "Order placement failed with error code: " +
                                   QString::number(static_cast<int>(expected_result.error()));
                if (showPopup)
                {
                    QMessageBox::critical(nullptr, "Order Error", errorMsg);
                }
                qCritical() << "Order placement failed with error code:" << static_cast<int>(expected_result.error());
                return;
            }

            PlaceOrderResult& result = expected_result.value();

            if (result.hasErrors())
            {
                QString errorMsg = "Order failed:\n";
                for (const auto& error: result.getErrors())
                {
                    errorMsg += error.getMessage() + "\n";
                    if (error.getError().has_value())
                    {
                        errorMsg += "Error: " + error.getError().value() + "\n";
                    }
                }
                if (showPopup)
                {
                    QMessageBox::critical(nullptr, "Order Error", errorMsg);
                }
                qCritical() << "Order placement failed:" << errorMsg;
            }
            else
            {
                QString successMsg = "Order(s) placed successfully:\n";
                for (const auto& orderItem: result.getOrders())
                {
                    successMsg += "Order ID: " + orderItem.getOrderID() + "\n";
                    successMsg += orderItem.getMessage() + "\n";
                }
                if (showPopup)
                {
                    QMessageBox::information(nullptr, "Order Success", successMsg);
                }
                qInfo() << "Order placement successful:" << successMsg;
            }
        });
}

void GUIFrontend::onShortcutChanged(ShortcutSettings::ShortcutId p_id, const QKeySequence& p_newSequence)
{
    // Update the appropriate shortcut
    switch (p_id)
    {
    case ShortcutSettings::QuitApplication:
        Q_CHECK_PTR(m_quitShortcut);
        m_quitShortcut->setKey(p_newSequence);
        qInfo() << "Updated quit application shortcut to:" << p_newSequence.toString();
        break;

    case ShortcutSettings::FocusStockInput:
        Q_CHECK_PTR(m_focusShortcut);
        m_focusShortcut->setKey(p_newSequence);
        qInfo() << "Updated focus stock input shortcut to:" << p_newSequence.toString();
        break;

    case ShortcutSettings::ExecuteBuyOrder:
        Q_CHECK_PTR(m_buyShortcut);
        m_buyShortcut->setKey(p_newSequence);
        qInfo() << "Updated execute buy order shortcut to:" << p_newSequence.toString();
        break;

    case ShortcutSettings::ExecuteSellOrder:
        Q_CHECK_PTR(m_sellShortcut);
        m_sellShortcut->setKey(p_newSequence);
        qInfo() << "Updated execute sell order shortcut to:" << p_newSequence.toString();
        break;

    case ShortcutSettings::ExecuteBuyToCoverOrder:
        Q_CHECK_PTR(m_buyToCoverShortcut);
        m_buyToCoverShortcut->setKey(p_newSequence);
        qInfo() << "Updated execute buy to cover order shortcut to:" << p_newSequence.toString();
        break;

    case ShortcutSettings::ExecuteSellToCoverOrder:
        Q_CHECK_PTR(m_sellToCoverShortcut);
        m_sellToCoverShortcut->setKey(p_newSequence);
        qInfo() << "Updated execute sell to cover order shortcut to:" << p_newSequence.toString();
        break;

    case ShortcutSettings::CancelAllOrders:
        Q_CHECK_PTR(m_cancelAllOrdersShortcut);
        m_cancelAllOrdersShortcut->setKey(p_newSequence);
        qInfo() << "Updated cancel all orders shortcut to:" << p_newSequence.toString();
        break;

    case ShortcutSettings::CloseAllPositions:
        Q_CHECK_PTR(m_closeAllPositionsShortcut);
        m_closeAllPositionsShortcut->setKey(p_newSequence);
        qInfo() << "Updated close all positions shortcut to:" << p_newSequence.toString();
        break;

    case ShortcutSettings::CloseAllPositionsPassive:
        Q_CHECK_PTR(m_closeAllPositionsPassiveShortcut);
        m_closeAllPositionsPassiveShortcut->setKey(p_newSequence);
        qInfo() << "Updated passive close all positions shortcut to:" << p_newSequence.toString();
        break;

    case ShortcutSettings::ToggleReplayPlayPause:
        Q_CHECK_PTR(m_toggleReplayPlayPauseShortcut);
        m_toggleReplayPlayPauseShortcut->setKey(p_newSequence);
        qInfo() << "Updated toggle replay play/pause shortcut to:" << p_newSequence.toString();
        break;

    case ShortcutSettings::ToggleReplayMode:
        Q_CHECK_PTR(m_toggleReplayModeShortcut);
        m_toggleReplayModeShortcut->setKey(p_newSequence);
        qInfo() << "Updated toggle replay mode shortcut to:" << p_newSequence.toString();
        break;

    case ShortcutSettings::TimeFrame10s:
        Q_CHECK_PTR(m_timeFrame10sShortcut);
        m_timeFrame10sShortcut->setKey(p_newSequence);
        break;

    case ShortcutSettings::TimeFrame1m:
        Q_CHECK_PTR(m_timeFrame1mShortcut);
        m_timeFrame1mShortcut->setKey(p_newSequence);
        break;
    case ShortcutSettings::TimeFrame5m:
        Q_CHECK_PTR(m_timeFrame5mShortcut);
        m_timeFrame5mShortcut->setKey(p_newSequence);
        break;
    case ShortcutSettings::TimeFrame15m:
        Q_CHECK_PTR(m_timeFrame15mShortcut);
        m_timeFrame15mShortcut->setKey(p_newSequence);
        break;
    case ShortcutSettings::TimeFrame30m:
        Q_CHECK_PTR(m_timeFrame30mShortcut);
        m_timeFrame30mShortcut->setKey(p_newSequence);
        break;
    case ShortcutSettings::TimeFrame1h:
        Q_CHECK_PTR(m_timeFrame1hShortcut);
        m_timeFrame1hShortcut->setKey(p_newSequence);
        break;
    case ShortcutSettings::TimeFrame4h:
        Q_CHECK_PTR(m_timeFrame4hShortcut);
        m_timeFrame4hShortcut->setKey(p_newSequence);
        break;
    case ShortcutSettings::TimeFrame1d:
        Q_CHECK_PTR(m_timeFrame1dShortcut);
        m_timeFrame1dShortcut->setKey(p_newSequence);
        break;
    case ShortcutSettings::TimeFrame1w:
        Q_CHECK_PTR(m_timeFrame1wShortcut);
        m_timeFrame1wShortcut->setKey(p_newSequence);
        break;
    case ShortcutSettings::TimeFrame1M:
        Q_CHECK_PTR(m_timeFrame1MShortcut);
        m_timeFrame1MShortcut->setKey(p_newSequence);
        break;

    case ShortcutSettings::OpenNewChart:
        Q_CHECK_PTR(m_newChartWindowShortcut);
        m_newChartWindowShortcut->setKey(p_newSequence);
        qInfo() << "Updated open new chart shortcut to:" << p_newSequence.toString();
        break;

    case ShortcutSettings::CloseChartWindow:
        // CloseChartWindow shortcut lives on each ChartWindow, not on main window.
        // ChartWindows read the shortcut at creation time; live update not supported.
        break;
    }
}

void GUIFrontend::onToggleReplayPlayPause()
{
    logInputEvent(u"GUIFrontend", u"toggle-replay-play-pause-shortcut");
    // Only works in replay mode
    if (MainApp::getDataSourceMode() != DataSourceMode::Replay)
    {
        return;
    }

    // Toggle via the replay controls bar method which clicks the button and emits the signal
    m_replayControlsBar->togglePlayPause();
}

void GUIFrontend::onToggleReplayMode()
{
    logInputEvent(u"GUIFrontend",
                  u"toggle-replay-mode-shortcut",
                  {inputDetail(u"inReplayMode", MainApp::isInReplayMode())});
    if (MainApp::isInReplayMode())
        emit m_tradingModeBar->replayExitRequested();
    else
        emit m_tradingModeBar->replayRequested();
}

void GUIFrontend::onCancelAllOrders()
{
    logInputEvent(u"GUIFrontend", u"cancel-all-orders-clicked");
    if (MainApp::isInReviewMode())
    {
        QMessageBox::information(nullptr, "Review Mode", "Order cancellation is disabled while Review mode is active.");
        return;
    }

    // Get only cancellable order IDs from the order window (filters by status)
    QStringList orderIds = ui->orderWidget->getCancellableOrderIds();

    if (orderIds.isEmpty())
    {
        qInfo() << "No cancellable orders found";
        QMessageBox::information(
            nullptr,
            "Cancel All Orders",
            "No orders available to cancel. All orders are either filled, cancelled, rejected, or in a non-cancellable state.");
        return;
    }

    logInputEvent(u"GUIFrontend", u"cancel-all-orders-ready", {inputDetail(u"count", orderIds.count())});

    // Check if confirmation is enabled (controlled by OrderEntryWidget settings)
    if (ui->orderEntryWidget->isCancelAllConfirmationEnabled())
    {
        // Confirm with user
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr,
            "Cancel All Orders",
            QString(
                "Are you sure you want to cancel %1 cancellable order(s)?\n\nOnly orders that are queued, received, or sent will be cancelled.\nFilled, cancelled, and rejected orders will be skipped.")
                .arg(orderIds.count()),
            QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes)
        {
            return;
        }
    }

    logInputEvent(u"GUIFrontend", u"cancel-all-orders-confirmed", {inputDetail(u"count", orderIds.count())});
    qInfo() << "Cancelling" << orderIds.count() << "cancellable orders";

    // Cancel each order
    for (const QString& orderId: orderIds)
    {
        auto cancelFuture = TSClient::getInstance()->cancelOrder(orderId);
        cancelFuture.then(
            [this, orderId](std::expected<CancelOrderResult, TSClient::Error> result)
            {
                if (!result.has_value())
                {
                    qCritical() << "Failed to cancel order" << orderId
                                << "with error code:" << static_cast<int>(result.error());
                }
                else
                {
                    CancelOrderResult& cancelResult = result.value();
                    qInfo() << "Order" << orderId << "cancelled successfully:" << cancelResult.toJsonString();
                }
            });
    }
}

void GUIFrontend::onCloseAllPositions()
{
    logInputEvent(u"GUIFrontend",
                  u"close-all-positions-clicked",
                  {inputDetail(u"accountId", getSelectedAccountId()), inputDetail(u"mode", QString("aggressive"))});
    if (MainApp::isInReviewMode())
    {
        QMessageBox::information(nullptr, "Review Mode", "Position closing is disabled while Review mode is active.");
        return;
    }

    const QString selectedAccountId = getSelectedAccountId();
    if (selectedAccountId.isEmpty())
    {
        QMessageBox::critical(nullptr,
                              "Close All Positions",
                              "No account is currently selected, so positions cannot be closed.");
        qCritical() << "Close all positions rejected: no selected account";
        return;
    }

    Q_CHECK_PTR(appStateSettings);
    ClosePositionsRequest request;
    request.accountId = selectedAccountId;
    request.aggressivityOffsetCents = appStateSettings
                                          ->value(ClosePositionsConstants::SETTINGS_KEY_AGGRESSIVE_LIMIT_OFFSET_CENTS,
                                                  ClosePositionsConstants::DEFAULT_AGGRESSIVE_LIMIT_OFFSET_CENTS)
                                          .toDouble();
    request.executionMode = ClosePositionsExecutionMode::AggressiveMarketable;
    submitClosePositionsRequest(request,
                                "Close All Positions",
                                QString("No open positions were found for account %1.").arg(selectedAccountId));
}

void GUIFrontend::onCloseAllPositionsPassive()
{
    logInputEvent(u"GUIFrontend",
                  u"close-all-positions-passive-clicked",
                  {inputDetail(u"accountId", getSelectedAccountId()), inputDetail(u"mode", QString("passive"))});
    if (MainApp::isInReviewMode())
    {
        QMessageBox::information(nullptr, "Review Mode", "Position closing is disabled while Review mode is active.");
        return;
    }

    const QString selectedAccountId = getSelectedAccountId();
    if (selectedAccountId.isEmpty())
    {
        QMessageBox::critical(nullptr,
                              "Close All Positions (Passive)",
                              "No account is currently selected, so positions cannot be closed.");
        qCritical() << "Passive close all positions rejected: no selected account";
        return;
    }

    Q_CHECK_PTR(appStateSettings);
    ClosePositionsRequest request;
    request.accountId = selectedAccountId;
    request.aggressivityOffsetCents = ClosePositionsConstants::MIN_AGGRESSIVE_LIMIT_OFFSET_CENTS;
    request.executionMode = ClosePositionsExecutionMode::PassiveResting;
    submitClosePositionsRequest(request,
                                "Close All Positions (Passive)",
                                QString("No open positions were found for account %1.").arg(selectedAccountId));
}

void GUIFrontend::onClosePosition(const QString& p_positionID)
{
    logInputEvent(u"GUIFrontend",
                  u"close-position-clicked",
                  {inputDetail(u"positionId", p_positionID), inputDetail(u"mode", QString("aggressive"))});
    if (MainApp::isInReviewMode())
    {
        QMessageBox::information(nullptr, "Review Mode", "Position closing is disabled while Review mode is active.");
        return;
    }

    const auto submitCloseForPosition = [this](const Position& position)
    {
        ClosePositionsRequest request;
        request.accountId = position.getAccountID();
        request.symbols = {position.getSymbol()};
        Q_CHECK_PTR(appStateSettings);
        request.aggressivityOffsetCents =
            appStateSettings
                ->value(ClosePositionsConstants::SETTINGS_KEY_AGGRESSIVE_LIMIT_OFFSET_CENTS,
                        ClosePositionsConstants::DEFAULT_AGGRESSIVE_LIMIT_OFFSET_CENTS)
                .toDouble();
        request.executionMode = ClosePositionsExecutionMode::AggressiveMarketable;

        submitClosePositionsRequest(request,
                                    "Close Position",
                                    QString("No open position was found for %1 in account %2.")
                                        .arg(position.getSymbol(), position.getAccountID()));
    };

    const QString positionID = p_positionID.trimmed();
    const auto cachedIt = m_positionsById.constFind(positionID);
    if (cachedIt != m_positionsById.cend())
    {
        submitCloseForPosition(cachedIt.value());
        return;
    }

    QPointer<GUIFrontend> frontend(this);
    const bool invoked = QMetaObject::invokeMethod(
        mainAlgo,
        [algo = mainAlgo, frontend, positionID]()
        {
            ASSUME_DIFF(algo, nullptr);
            if (frontend.isNull())
            {
                return;
            }

            std::optional<Position> matchedPosition;
            const QVector<Position> positions = algo->getCurrentPositionsSnapshot();
            for (const Position& position: positions)
            {
                if (position.getPositionID().trimmed() == positionID)
                {
                    matchedPosition = position;
                    break;
                }
            }

            QMetaObject::invokeMethod(
                frontend.data(),
                [frontend, positionID, matchedPosition]()
                {
                    if (frontend.isNull())
                    {
                        return;
                    }

                    if (!matchedPosition.has_value())
                    {
                        const QString message = QString("Position %1 is no longer available to close.").arg(positionID);
                        QMessageBox::information(nullptr, "Close Position", message);
                        qInfo() << message;
                        return;
                    }

                    const Position& position = matchedPosition.value();
                    frontend->m_positionsById.insert(position.getPositionID().trimmed(), position);

                    ClosePositionsRequest request;
                    request.accountId = position.getAccountID();
                    request.symbols = {position.getSymbol()};
                    Q_CHECK_PTR(appStateSettings);
                    request.aggressivityOffsetCents =
                        appStateSettings
                            ->value(ClosePositionsConstants::SETTINGS_KEY_AGGRESSIVE_LIMIT_OFFSET_CENTS,
                                    ClosePositionsConstants::DEFAULT_AGGRESSIVE_LIMIT_OFFSET_CENTS)
                            .toDouble();
                    request.executionMode = ClosePositionsExecutionMode::AggressiveMarketable;

                    frontend->submitClosePositionsRequest(request,
                                                          "Close Position",
                                                          QString("No open position was found for %1 in account %2.")
                                                              .arg(position.getSymbol(), position.getAccountID()));
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
    ASSUME_TRUE(invoked);

    qInfo() << "Close position lookup queued for" << positionID;
}

void GUIFrontend::submitClosePositionsRequest(const ClosePositionsRequest& p_request,
                                              const QString& p_dialogTitle,
                                              const QString& p_noMatchesMessage)
{
    mainAlgo->closePositions(p_request).then(
        this,
        [this, p_dialogTitle, p_noMatchesMessage](std::expected<ClosePositionsResult, QString> result)
        {
            if (!result.has_value())
            {
                QMessageBox::critical(nullptr, p_dialogTitle, result.error());
                qCritical() << p_dialogTitle << "failed:" << result.error();
                return;
            }

            const ClosePositionsResult& closeResult = result.value();
            if (closeResult.matchedPositionCount == 0)
            {
                QMessageBox::information(nullptr, p_dialogTitle, p_noMatchesMessage);
                qInfo() << p_noMatchesMessage;
                return;
            }

            if (closeResult.hasFailures())
            {
                const QString message = summarizeClosePositionsFailures(closeResult);
                if (closeResult.successCount() == 0)
                {
                    QMessageBox::critical(nullptr, p_dialogTitle, message);
                }
                else
                {
                    QMessageBox::warning(nullptr, p_dialogTitle, message);
                }
                qWarning() << p_dialogTitle << "completed with errors:" << message;
                return;
            }

            const QString message = summarizeClosePositionsSuccess(closeResult);
            if (ui->orderEntryWidget->isResultPopupEnabled())
            {
                QMessageBox::information(nullptr, p_dialogTitle, message);
            }
            qInfo() << p_dialogTitle << "succeeded:" << message;
        });
}

void GUIFrontend::requestMissingBarsFromCache(const QString& p_symbol,
                                              const QDateTime& from,
                                              const QDateTime& to,
                                              uint64_t p_requestToken)
{
    DEBUG << "Request missing bars from " << from << " to " << to;

    if (!ui->priceChart->isExpectedMissingBarsRequest(p_symbol, p_requestToken))
    {
        DEBUG << "Ignoring stale requestMissingBarsFromCache dispatch for" << p_symbol << "token" << p_requestToken;
        return;
    }

    // Guard against the race where setSymbol() fires on the GUI thread but the queued
    // onSelectDisplayedStock() hasn't reached MainAlgo yet (cross-thread delivery).
    // In that case MainAlgo's m_currentDisplayedSymbolContext still points to the OLD
    // symbol, so we would paint bars from the wrong BarCache.  Retry in 50ms — by that
    // time the queued event will have been processed.
    if (MainAlgo::getInstance()->getDisplayedSymbol() != p_symbol)
    {
        DEBUG << "Symbol mismatch (MainAlgo:" << MainAlgo::getInstance()->getDisplayedSymbol()
              << "vs chart:" << p_symbol << ") — retrying in 50ms";
        QTimer::singleShot(50,
                           this,
                           [this, p_symbol, from, to, p_requestToken]()
                           { requestMissingBarsFromCache(p_symbol, from, to, p_requestToken); });
        return;
    }

    BarCache::GetBarsResult_t result = MainAlgo::getInstance()->requestMissingBarsDisplayedStock(from.date(),
                                                                                                 from.time(),
                                                                                                 to.time(),
                                                                                                 m_currentTimeFrame);

    if (std::holds_alternative<std::shared_ptr<QVector<Bar>>>(result))
    {
        auto bars = std::get<std::shared_ptr<QVector<Bar>>>(result);
        if (bars->isEmpty())
        {
            // No bars available yet (instrument not ready or no cached data)
            ui->priceChart->onRequestedMissingBarsFailed(p_symbol, p_requestToken);
        }
        else
        {
            ui->priceChart->onRequestedMissingBarsReceived(p_symbol, p_requestToken, bars);
        }
    }
    else if (std::holds_alternative<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(result))
    {
        std::get<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(result).then(
            [frontend = QPointer<GUIFrontend>(this), p_symbol, from, to, p_requestToken](
                std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>&& bars) mutable
            {
                if (frontend.isNull())
                {
                    return;
                }

                QMetaObject::invokeMethod(
                    frontend,
                    [frontend, p_symbol, from, to, p_requestToken, bars = std::move(bars)]() mutable
                    {
                        if (frontend.isNull())
                        {
                            return;
                        }

                        if (!frontend->ui->priceChart->isExpectedMissingBarsRequest(p_symbol, p_requestToken))
                        {
                            qCDebug(GUIFrontendLog)
                                << "Ignoring stale missing bars callback for" << p_symbol << "token" << p_requestToken;
                            return;
                        }

                        if (bars.has_value())
                        {
                            if (bars.value()->isEmpty())
                            {
                                qCWarning(GUIFrontendLog) << "Historical fetch returned 0 bars";
                                frontend->ui->priceChart->onRequestedMissingBarsFailed(p_symbol, p_requestToken);
                            }
                            else
                            {
                                frontend->ui->priceChart->onRequestedMissingBarsReceived(p_symbol,
                                                                                         p_requestToken,
                                                                                         bars.value());
                            }
                        }
                        else
                        {
                            qCritical() << "Failed to get missing bars from BarCache - Error:"
                                        << QtEnum::toString(bars.error());

                            // Notify the chart that the request failed so it can release the semaphore
                            frontend->ui->priceChart->onRequestedMissingBarsFailed(p_symbol, p_requestToken);

                            // Retry after 1 second using the same pattern as TSClient error handling
                            QTimer::singleShot(
                                1000,
                                frontend,
                                [frontend, p_symbol, from, to, p_requestToken]()
                                {
                                    if (frontend.isNull())
                                    {
                                        return;
                                    }

                                    if (!frontend->ui->priceChart->isExpectedMissingBarsRequest(p_symbol,
                                                                                                p_requestToken))
                                    {
                                        qCDebug(GUIFrontendLog) << "Skipping stale missing bars retry for" << p_symbol
                                                                << "token" << p_requestToken;
                                        return;
                                    }

                                    qInfo() << "Retrying missing bars request from" << from << "to" << to;
                                    frontend->requestMissingBarsFromCache(p_symbol, from, to, p_requestToken);
                                });
                        }
                    },
                    Qt::QueuedConnection);
            });
    }
    else
    {
        qCritical() << "Unexpected result type from requestMissingBarsDisplayedStock";
        ui->priceChart->onRequestedMissingBarsFailed(p_symbol, p_requestToken);
    }
}

void GUIFrontend::onTimeFrameChanged(TimeFrame tf)
{
    if (tf == TimeFrame::TEN_SECONDS && !MainApp::isInReplayMode())
    {
        ui->priceChart->toolbar()->setCurrentTimeFrame(TimeFrame::ONE_MINUTE);
        return;
    }

    if (tf == m_currentTimeFrame)
        return;

    DEBUG << "Timescale changed to" << static_cast<int>(tf);
    m_currentTimeFrame = tf;

    // Persist when auto is off (Phase 9)
    if (!ui->priceChart->toolbar()->isAutoTimeFrameEnabled())
    {
        Q_CHECK_PTR(appStateSettings);
        appStateSettings->setValue("Chart/TimeFrame", static_cast<int>(tf));
        appStateSettings->sync();
    }

    // Sync the replay time-edit step granularity to the new timeframe
    m_replayControlsBar->setCurrentTimeFrame(tf);

    // Clear the chart and let it re-request bars with the new timescale.
    // Preserve X and Y ranges so the view doesn't jump after the reload.
    ui->priceChart->setDisplayTimeFrame(tf);
    ui->priceChart->preserveCurrentRanges();
    ui->priceChart->clearChart();
}

void GUIFrontend::updateTenSecondTimeFrameAvailability()
{
    const bool allowTenSecond = MainApp::isInReplayMode();
    ui->priceChart->toolbar()->setTenSecondTimeFrameEnabled(allowTenSecond);
    if (m_timeFrame10sShortcut != nullptr)
    {
        m_timeFrame10sShortcut->setEnabled(allowTenSecond);
    }
}

QString GUIFrontend::formatAccountInfo(const Account& account) const
{
    QString info;
    info += "<b>Account Information</b><br><br>";
    info += "<b>ID:</b> " + account.getAccountId() + "<br>";
    info += "<b>Type:</b> " + AccountType::accountTypeToString(account.getAccountType().type) + "<br>";
    info += "<b>Status:</b> " + account.getStatus() + "<br>";
    info += "<b>Currency:</b> " + account.getCurrency() + "<br>";

    // Check AccountDetail if it exists
    const auto& detail = account.getAccountDetail();
    if (detail.has_value())
    {
        info += "<br><b>Account Details:</b><br>";
        info += "  Stock Locate Eligible: " + QString(detail->isStockLocateEligible ? "Yes" : "No") + "<br>";
        info += "  Enrolled in RegT Program: " + QString(detail->enrolledInRegTProgram ? "Yes" : "No") + "<br>";
        info +=
            "  Requires Buying Power Warning: " + QString(detail->requiresBuyingPowerWarning ? "Yes" : "No") + "<br>";
        info += "  Day Trading Qualified: " + QString(detail->dayTradingQualified ? "Yes" : "No") + "<br>";
        info += "  Option Approval Level: " + QString::number(detail->optionApprovalLevel) + "<br>";
        info += "  Pattern Day Trader: " + QString(detail->patternDayTrader ? "Yes" : "No") + "<br>";
    }

    return info;
}

void GUIFrontend::onAccountInfoButtonClicked()
{
    int currentIndex = ui->accountSelector->currentIndex();
    if (currentIndex >= 0 && currentIndex < m_accounts.size())
    {
        const Account& account = m_accounts[currentIndex];
        logInputEvent(u"GUIFrontend", u"show-account-info", {inputDetail(u"accountId", account.getAccountId())});
        QString info = formatAccountInfo(account);

        // Show information in a message box
        QMessageBox msgBox;
        msgBox.setWindowTitle("Account Information");
        msgBox.setTextFormat(Qt::RichText);
        msgBox.setText(info);
        msgBox.setIcon(QMessageBox::Information);
        msgBox.exec();
    }
}

void GUIFrontend::updateSessionLabel()
{
    if (m_sessionLabel == nullptr)
    {
        return;
    }

    TradingSession session = MainApp::getCurrentSession();
    QString sessionText;
    QString backgroundColor;

    switch (session)
    {
    case TradingSession::EarlyPreMarket:
        sessionText = "🌙 EARLY PRE";
        backgroundColor = "#2a1a4a"; // Dark purple
        break;
    case TradingSession::PreMarket:
        sessionText = "🌅 PRE-MARKET";
        backgroundColor = "#4a3a2a"; // Dark orange-brown
        break;
    case TradingSession::Regular:
        sessionText = "📈 REGULAR";
        backgroundColor = "#1a3a1a"; // Dark green
        break;
    case TradingSession::AfterHours:
        sessionText = "🌆 AFTER-HOURS";
        backgroundColor = "#4a3a1a"; // Dark orange-brown
        break;
    case TradingSession::Closed:
        sessionText = "🌚 CLOSED";
        backgroundColor = "#1a1a2a"; // Dark blue-gray
        break;
    case TradingSession::Weekend:
        sessionText = "🏖 WEEKEND";
        backgroundColor = "#2a2a2a"; // Dark gray
        break;
    case TradingSession::Holiday:
        sessionText = "🎌 HOLIDAY";
        backgroundColor = "#1a2a1a"; // Dark green
        break;
    default:
        sessionText = "🌚 CLOSED";
        backgroundColor = "#1a1a2a";
        break;
    }

    m_sessionLabel->setText(sessionText);
    m_sessionLabel->setStyleSheet(QString("QLabel { background-color: %1; color: #ffffff; padding: 4px 8px; "
                                          "border-radius: 4px; font-weight: bold; font-family: monospace; }")
                                      .arg(backgroundColor));
}

void GUIFrontend::updateTimeDisplay()
{

    OBJ_ASSUME_TRUE(m_timeDisplayLabel != nullptr);

    // Get current application time (live or replay)
    QDateTime currentTime = MainApp::getCurrentAppTime();

    // Format time with date: "Wed 02/05  03:34:47 PM"
    QString timeStr = currentTime.toString("ddd MM/dd  hh:mm:ss AP");

    // Check if we're in replay mode
    bool isReplayMode = MainApp::isInReplayMode();

    // Update display with appropriate styling
    if (isReplayMode)
    {
        // REPLAY mode: Amber/orange color
        m_timeDisplayLabel->setText("⏱️ " + timeStr);
        m_timeDisplayLabel->setStyleSheet(
            "QLabel { "
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
            "                               stop:0 #2a1a0a, stop:0.5 #1a0f05, stop:1 #2a1a0a); "
            "  color: #ff9900; " // Amber for REPLAY
            "  border: 2px solid #443322; "
            "  border-radius: 6px; "
            "  padding: 6px 12px; "
            "  font-family: 'Courier New', monospace; "
            "  font-size: 14px; "
            "  font-weight: bold; "
            "  letter-spacing: 1px; "
            "}");
    }
    else
    {
        // LIVE mode: Green color
        m_timeDisplayLabel->setText("🕐 " + timeStr);
        m_timeDisplayLabel->setStyleSheet(
            "QLabel { "
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
            "                               stop:0 #0a1a0a, stop:0.5 #050f05, stop:1 #0a1a0a); "
            "  color: #00ff00; " // Bright green for LIVE
            "  border: 2px solid #224422; "
            "  border-radius: 6px; "
            "  padding: 6px 12px; "
            "  font-family: 'Courier New', monospace; "
            "  font-size: 14px; "
            "  font-weight: bold; "
            "  letter-spacing: 1px; "
            "}");
    }

    refreshStrategyConfirmationCueCountdown(false);
}

void GUIFrontend::onReplayModeEntered()
{
    LTTnG_TP(opentraderplatform, gui_replay_entered);
    clearManualArmedBracket(QStringLiteral("replay-mode-entered"));
    m_replayPlaybackPaused = true;

    qCInfo(GUIFrontendLog) << "Replay mode entered";
    if (m_preReplayDisplayedSymbol.isNull())
    {
        m_preReplayDisplayedSymbol = currentlyDisplayedSymbol;
    }
    if (m_preReplaySelectedAccountId.isNull())
    {
        m_preReplaySelectedAccountId = getSelectedAccountId();
    }

    const QVector<Account> replayAccounts{makeReplaySimAccount()};
    applyAccountsToSelector(replayAccounts, QString::fromLatin1(REPLAY_SIM_ACCOUNT_ID));

    // Persist replay state so we can restore it on next launch
    saveReplayState(true, m_replayControlsBar->getSelectedReplayDay(), m_replayControlsBar->getReplayStartTime());

    // Update tristate mode bar to show REPLAY as active
    m_tradingModeBar->setActiveMode(TradingModeBar::Mode::Replay);

    // Clear live orders and positions from widgets (replay starts with clean slate)
    ui->orderWidget->clearAllOrders();
    ui->positionWidget->clearAllPositions();
    m_positionsById.clear();
    ui->strategyQuickView->clearStrategies();
    if (m_strategyLogWidget)
    {
        m_strategyLogWidget->clearStrategy();
        m_strategyLogWidget->hide();
    }

    // Clear chart data for fresh replay (bar caches are cleared separately by MainAlgo)
    ui->priceChart->clearChart();

    // Clear market depth table — stale live data must not carry over into replay
    ui->level2Widget->clearData();
    ui->timeAndSalesWidget->clearData();

    // TODO Phase 6: probe DBClient .dbn replay file to detect available schemas (MBP-10, MBP-1, etc.)
    // For now, clear the mode indicator (no legacy TS recorded data)
    ui->level2Widget->setExpectedDataMode(false);

    // Refresh replay-day availability and show replay controls.
    m_replayControlsBar->scanAndPopulateReplayDays();

    // Show replay controls bar and ensure play button is in stopped state
    m_replayControlsBar->setVisible(true);
    m_replayControlsBar->setReplayPlaying(false);

    // Set controls to PreloadingPaused state (data loaded, waiting for user to press play)
    m_replayControlsBar->setReplayState(ReplayControlsBar::ReplayState::PreloadingPaused);
    m_replayControlsBar->setReplaySpeedControlLocked(false);

    // Connect replay data load error signal to chart error handler
    connect(DBClient::getInstance(),
            &DBClient::replayDataLoadFailed,
            ui->priceChart,
            &StockPriceChart::onReplayDataLoadFailed,
            Qt::UniqueConnection);

    // Update chart visual (background color and watermark)
    ui->priceChart->setReplayModeActive(true);
    updateTenSecondTimeFrameAvailability();

    const bool explicitEmptyReplaySymbol = !m_pendingReplayEntrySymbol.isNull() && m_pendingReplayEntrySymbol.isEmpty();
    if (explicitEmptyReplaySymbol)
    {
        currentlyDisplayedSymbol.clear();
        ui->stockSymbolInput->clear();
        ui->priceChart->setSymbol(QString());
    }

    ui->orderEntryWidget->setSymbol(ui->priceChart->getCurrentSymbol());

    // Switch all secondary chart windows to the replay symbol
    const QString replaySymbol =
        m_pendingReplayEntrySymbol.isNull() ? currentlyDisplayedSymbol : m_pendingReplayEntrySymbol;
    for (ChartWindow* cw: m_windowManager->chartWindows())
        cw->enterReplayMode(replaySymbol);
    m_pendingReplayEntrySymbol = QString();

    // Update session label and time display (replay time may have changed)
    updateSessionLabel();
    updateTimeDisplay();
}

void GUIFrontend::onReplayConfigurationChanged(const QDate& p_date,
                                               const QTime& p_startTime,
                                               const Playback::Speed p_speed)
{
    if (p_date.isValid() && m_replayControlsBar->getSelectedReplayDay() != p_date)
    {
        m_replayControlsBar->scanAndPopulateReplayDays();
        m_replayControlsBar->setSelectedReplayDay(p_date);
    }

    if (p_startTime.isValid() && m_replayControlsBar->getReplayStartTime() != p_startTime)
    {
        m_replayControlsBar->setReplayStartTime(p_startTime);
    }

    if (m_replayControlsBar->getReplaySpeed() != p_speed)
    {
        m_replayControlsBar->setReplaySpeed(p_speed);
    }
}

void GUIFrontend::onReplayPlaybackStateChanged(const Playback::State p_state)
{
    switch (p_state)
    {
    case Playback::State::Playing:
        m_replayPlaybackPaused = false;
        resumeActiveStrategyConfirmationCountdown();
        m_replayControlsBar->setReplayPlaying(true);
        if (MainApp::isInReplayMode())
        {
            m_replayControlsBar->setReplayState(ReplayControlsBar::ReplayState::Playing);
            m_tradingModeBar->setActiveMode(TradingModeBar::Mode::Replay);
        }
        break;

    case Playback::State::Paused:
        m_replayPlaybackPaused = true;
        pauseActiveStrategyConfirmationCountdown();
        m_replayControlsBar->setReplayPlaying(false);
        if (MainApp::isInReplayMode())
        {
            const bool shouldTreatAsPostStartPause =
                (m_replayControlsBar->getReplayState() == ReplayControlsBar::ReplayState::Playing) ||
                m_replayControlsBar->hasStartedPlayback();
            m_replayControlsBar->setReplayState(shouldTreatAsPostStartPause
                                                    ? ReplayControlsBar::ReplayState::Paused
                                                    : ReplayControlsBar::ReplayState::PreloadingPaused);
            m_tradingModeBar->setActiveMode(TradingModeBar::Mode::Replay);
        }
        break;

    case Playback::State::Stopped:
        m_replayPlaybackPaused = MainApp::isInReplayMode();
        if (m_replayPlaybackPaused)
        {
            pauseActiveStrategyConfirmationCountdown();
        }
        else
        {
            resumeActiveStrategyConfirmationCountdown();
        }
        m_replayControlsBar->setReplayPlaying(false);
        if (MainApp::isInReplayMode())
        {
            m_replayControlsBar->setReplayState(ReplayControlsBar::ReplayState::PreloadingPaused);
            m_tradingModeBar->setActiveMode(TradingModeBar::Mode::Replay);
        }
        else
        {
            m_replayControlsBar->setReplayState(ReplayControlsBar::ReplayState::Inactive);
        }
        break;
    }
}

void GUIFrontend::onReplayRestartRequested()
{
    if (!MainApp::isInReplayMode())
    {
        return;
    }
    clearManualArmedBracket(QStringLiteral("replay-restart"));

    const QDate date = m_replayControlsBar->getSelectedReplayDay();
    const QTime startTime = m_replayControlsBar->getReplayStartTime();
    const Playback::Speed speed = m_replayControlsBar->getReplaySpeed();

    MainApp::currentAppReplayTime = QDateTime(date, startTime, TradingHours::MARKET_TIMEZONE);
    ui->priceChart->clearChart();
    ui->orderWidget->clearAllOrders();
    ui->positionWidget->clearAllPositions();
    m_positionsById.clear();
    ui->strategyQuickView->clearStrategies();
    if (m_strategyLogWidget)
    {
        m_strategyLogWidget->clearStrategy();
        m_strategyLogWidget->hide();
    }
    ui->level2Widget->clearData();
    ui->timeAndSalesWidget->clearData();

    m_replayControlsBar->setReplayPlaying(false);
    m_replayControlsBar->setReplayState(ReplayControlsBar::ReplayState::PreloadingPaused);
    m_tradingModeBar->setActiveMode(TradingModeBar::Mode::Replay);

    MainApp::getInstance()->restartReplaySession(date, startTime, speed);
}

void GUIFrontend::onReplayModeExited()
{
    LTTnG_TP(opentraderplatform, gui_replay_exited);
    clearManualArmedBracket(QStringLiteral("replay-mode-exited"));
    m_positionsById.clear();
    m_replayPlaybackPaused = false;
    resumeActiveStrategyConfirmationCountdown();

    qCInfo(GUIFrontendLog) << "Replay mode exited";

    // Clear persisted replay state
    saveReplayState(false);

    // Update tristate mode bar back to LIVE or SIM (whichever is the brokerage mode)
    bool inSimMode = (MainApp::getTradingMode() == TradingMode::Sim);
    m_tradingModeBar->setActiveMode(inSimMode ? TradingModeBar::Mode::Sim : TradingModeBar::Mode::Live);

    // Reset play button state and hide replay controls bar
    m_replayControlsBar->setReplayPlaying(false);
    m_replayControlsBar->setReplaySpeedControlLocked(false);
    m_replayControlsBar->setVisible(false);

    // Set controls state back to Inactive
    m_replayControlsBar->setReplayState(ReplayControlsBar::ReplayState::Inactive);

    // Clear chart data (MainAlgo will clear caches and restart live stream)
    ui->priceChart->clearChart();

    // Clear market depth table — replay data must not carry over into live mode
    ui->level2Widget->clearData();
    ui->timeAndSalesWidget->clearData();

    // Restore chart visual
    ui->priceChart->setReplayModeActive(false);
    updateTenSecondTimeFrameAvailability();

    if (ui->priceChart->getCurrentSymbol().isEmpty() && !m_preReplayDisplayedSymbol.isEmpty())
    {
        currentlyDisplayedSymbol = m_preReplayDisplayedSymbol;
        ui->stockSymbolInput->setText(currentlyDisplayedSymbol);
        ui->priceChart->setSymbol(currentlyDisplayedSymbol);
        ui->orderEntryWidget->setSymbol(currentlyDisplayedSymbol);
    }
    else if (ui->priceChart->getCurrentSymbol().isEmpty())
    {
        currentlyDisplayedSymbol.clear();
        ui->stockSymbolInput->clear();
        ui->orderEntryWidget->setSymbol(QString());
    }

    // Restore all secondary chart windows to their pre-replay symbols
    for (ChartWindow* cw: m_windowManager->chartWindows())
        cw->exitReplayMode();
    m_preReplayDisplayedSymbol = QString();
    m_pendingReplayEntrySymbol = QString();

    // Update session label and time display (back to live time)
    updateSessionLabel();
    updateTimeDisplay();
}

void GUIFrontend::onReviewModeEntered()
{
    clearManualArmedBracket(QStringLiteral("review-mode-entered"));
    qCInfo(GUIFrontendLog) << "Review mode entered";

    saveReviewState(true, MainApp::getReviewSessionId());

    m_tradingModeBar->setActiveMode(TradingModeBar::Mode::Review);
    m_replayControlsBar->setVisible(false);
    m_reviewSessionBar->scanAndPopulateSessions();
    m_reviewSessionBar->setSelectedSessionId(MainApp::getReviewSessionId());
    m_reviewSessionBar->setVisible(true);

    ui->orderEntryWidget->setReviewModeEnabled(true);
    ui->orderWidget->setReviewModeEnabled(true);
    ui->positionWidget->setReviewModeEnabled(true);
    ui->strategyQuickView->setReviewModeEnabled(true);

    loadReviewSessionIntoWidgets();
    updateTenSecondTimeFrameAvailability();
    updateSessionLabel();
    updateTimeDisplay();
}

void GUIFrontend::onReviewModeExited()
{
    clearManualArmedBracket(QStringLiteral("review-mode-exited"));
    m_positionsById.clear();
    qCInfo(GUIFrontendLog) << "Review mode exited";

    saveReviewState(false);

    const bool inSimMode = (MainApp::getTradingMode() == TradingMode::Sim);
    m_tradingModeBar->setActiveMode(inSimMode ? TradingModeBar::Mode::Sim : TradingModeBar::Mode::Live);
    m_reviewSessionBar->setVisible(false);

    ui->orderEntryWidget->setReviewModeEnabled(false);
    ui->orderWidget->setReviewModeEnabled(false);
    ui->positionWidget->setReviewModeEnabled(false);
    ui->strategyQuickView->setReviewModeEnabled(false);
    ui->strategyQuickView->setReviewSymbols({});

    ui->level2Widget->clearData();
    ui->timeAndSalesWidget->clearData();

    updateTenSecondTimeFrameAvailability();
    updateSessionLabel();
    updateTimeDisplay();
}

void GUIFrontend::onTradingModeConfigured(const TradingMode p_mode)
{
    if (MainApp::isInReplayMode() || MainApp::isInReviewMode())
    {
        return;
    }

    m_tradingModeBar->setActiveMode(p_mode == TradingMode::Sim ? TradingModeBar::Mode::Sim
                                                               : TradingModeBar::Mode::Live);
    updateTenSecondTimeFrameAvailability();
}

bool GUIFrontend::eventFilter(QObject* p_watched, QEvent* p_event)
{
    if (p_watched == m_mainWindow && p_event->type() == QEvent::Paint && !m_platformWindowPainted)
    {
        m_platformWindowPainted = true;
        // Defer delivery until the paint event has finished.
        QTimer::singleShot(0, this, [this]() { emit platformWindowPainted(); });
    }
    if (p_event->type() == QEvent::KeyPress && m_mainWindow->isVisible() && m_mainWindow->isActiveWindow())
    {
        auto* keyEvent = static_cast<QKeyEvent*>(p_event);
        const int key = keyEvent->key();
        const Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
        const bool noModifier = modifiers == Qt::NoModifier;

        if (!keyEvent->isAutoRepeat() && noModifier && key == Qt::Key_M)
        {
            QWidget* const focusedWidget = QApplication::focusWidget();
            const bool typingInputFocused = qobject_cast<QLineEdit*>(focusedWidget) != nullptr ||
                                            qobject_cast<QAbstractSpinBox*>(focusedWidget) != nullptr ||
                                            qobject_cast<QTextEdit*>(focusedWidget) != nullptr ||
                                            qobject_cast<QPlainTextEdit*>(focusedWidget) != nullptr;
            if (!typingInputFocused)
            {
                const bool wasMuted = m_strategyConfirmationAlertsMuted;
                const bool nextMuted = !m_strategyConfirmationAlertsMuted;
                logInputEvent(u"GUIFrontend",
                              u"toggle-strategy-confirm-alert-mute-shortcut",
                              {inputDetail(u"muted", nextMuted),
                               inputDetail(u"hadActiveConfirmation", !m_activeStrategyConfirmationID.isEmpty())});
                applyStrategyConfirmationMuteMode(nextMuted);
                qCInfo(GUIFrontendLog) << "Strategy manual-confirm mute mode changed from" << wasMuted << "to"
                                       << nextMuted;
                p_event->accept();
                return true;
            }
        }

        if (!keyEvent->isAutoRepeat() && noModifier && (key == Qt::Key_B || key == Qt::Key_S || key == Qt::Key_Escape))
        {
            QWidget* const focusedWidget = QApplication::focusWidget();
            const bool typingInputFocused = qobject_cast<QLineEdit*>(focusedWidget) != nullptr ||
                                            qobject_cast<QAbstractSpinBox*>(focusedWidget) != nullptr ||
                                            qobject_cast<QTextEdit*>(focusedWidget) != nullptr ||
                                            qobject_cast<QPlainTextEdit*>(focusedWidget) != nullptr;

            if (!typingInputFocused && m_activeStrategyConfirmationID.isEmpty())
            {
                if (key == Qt::Key_B)
                {
                    logInputEvent(u"GUIFrontend", u"manual-bracket-arm-long-shortcut");
                    handleManualBracketArmShortcut(StrategyBracketOverlayEntry::Side::Long);
                    p_event->accept();
                    return true;
                }
                if (key == Qt::Key_S)
                {
                    logInputEvent(u"GUIFrontend", u"manual-bracket-arm-short-shortcut");
                    handleManualBracketArmShortcut(StrategyBracketOverlayEntry::Side::Short);
                    p_event->accept();
                    return true;
                }
                if (key == Qt::Key_Escape && m_manualArmedBracket.has_value())
                {
                    logInputEvent(u"GUIFrontend", u"manual-bracket-arm-cancel-shortcut");
                    clearManualArmedBracket(QStringLiteral("escape-shortcut"));
                    p_event->accept();
                    return true;
                }
            }
        }
    }

    if (p_event->type() == QEvent::KeyPress && !m_activeStrategyConfirmationID.isEmpty() && m_mainWindow->isVisible() &&
        m_mainWindow->isActiveWindow())
    {
        auto* keyEvent = static_cast<QKeyEvent*>(p_event);
        const int key = keyEvent->key();
        const Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
        const bool noModifier = modifiers == Qt::NoModifier;
        const bool shiftOnly = modifiers == Qt::ShiftModifier;
        const bool acceptShortcut = key == Qt::Key_Y && (noModifier || shiftOnly);
        const bool rejectShortcut = key == Qt::Key_N && noModifier;
        const bool rejectAndBlockSymbolShortcut = key == Qt::Key_N && shiftOnly;
        const bool switchShortcut = key == Qt::Key_W && (noModifier || shiftOnly);
        if (!keyEvent->isAutoRepeat() &&
            (acceptShortcut || rejectShortcut || rejectAndBlockSymbolShortcut || switchShortcut))
        {
            const QString confirmationID = m_activeStrategyConfirmationID;
            const StrategyConfirmationInputStage stage =
                (m_strategyConfirmationInputStage == StrategyConfirmationInputStage::Idle)
                    ? StrategyConfirmationInputStage::AwaitingDecision
                    : m_strategyConfirmationInputStage;
            const auto forwardDecision = [this, confirmationID](const bool accepted, const double overrideStopPrice)
            {
                QMetaObject::invokeMethod(
                    mainAlgo,
                    [algo = mainAlgo, confirmationID, accepted, overrideStopPrice]()
                    {
                        ASSUME_DIFF(algo, nullptr);
                        algo->onStrategyOrderConfirmationDecision(confirmationID, accepted, overrideStopPrice);
                    },
                    Qt::QueuedConnection);
            };
            const auto forwardRejectAndBlockSymbol = [this, confirmationID]()
            {
                QMetaObject::invokeMethod(
                    mainAlgo,
                    [algo = mainAlgo, confirmationID]()
                    {
                        ASSUME_DIFF(algo, nullptr);
                        algo->onStrategyOrderConfirmationRejectAndBlockSymbol(confirmationID);
                    },
                    Qt::QueuedConnection);
            };

            if (stage == StrategyConfirmationInputStage::AwaitingSwitchAuthorization)
            {
                if (switchShortcut)
                {
                    logInputEvent(u"GUIFrontend",
                                  u"strategy-manual-confirm-switch-authorized",
                                  {inputDetail(u"confirmationId", confirmationID),
                                   inputDetail(u"symbol", m_activeStrategyConfirmationSymbol)});
                    if (!m_activeStrategyConfirmationSymbol.isEmpty() &&
                        m_activeStrategyConfirmationSymbol != currentlyDisplayedSymbol)
                    {
                        ChartViewRangesSnapshot viewRangesSnapshot;
                        if (ui->priceChart->snapshotCurrentViewRanges(viewRangesSnapshot.xLower,
                                                                      viewRangesSnapshot.xUpper,
                                                                      viewRangesSnapshot.yLower,
                                                                      viewRangesSnapshot.yUpper))
                        {
                            m_preConfirmationChartViewRanges = viewRangesSnapshot;
                        }
                        m_preConfirmationDisplayedSymbol = currentlyDisplayedSymbol;
                        displayStock(m_activeStrategyConfirmationSymbol);
                    }
                    ui->priceChart->recenterToCurrentPriceAction(true);
                    m_strategyConfirmationInputStage = StrategyConfirmationInputStage::AwaitingDecision;
                    refreshStrategyConfirmationCueCountdown(true);
                    p_event->accept();
                    return true;
                }

                if (acceptShortcut)
                {
                    logInputEvent(u"GUIFrontend",
                                  u"strategy-manual-confirm-accept-ignored-awaiting-switch",
                                  {inputDetail(u"confirmationId", confirmationID)});
                    p_event->accept();
                    return true;
                }

                if (rejectAndBlockSymbolShortcut)
                {
                    logInputEvent(u"GUIFrontend",
                                  u"strategy-manual-confirm-reject-and-block-before-switch",
                                  {inputDetail(u"confirmationId", confirmationID)});
                    const bool clearStrategyPreview =
                        m_manualArmedBracket.has_value() &&
                        m_manualArmedBracket->source == ManualArmedBracketState::Source::StrategyConfirmation &&
                        m_manualArmedBracket->confirmationID == confirmationID;
                    resetActiveStrategyConfirmationUiState();
                    ui->priceChart->clearManualConfirmationCue();
                    if (clearStrategyPreview)
                    {
                        clearManualArmedBracket(QStringLiteral("strategy-confirmation-reject-block-before-switch"));
                    }
                    forwardRejectAndBlockSymbol();
                    p_event->accept();
                    return true;
                }

                logInputEvent(u"GUIFrontend",
                              u"strategy-manual-confirm-reject-before-switch",
                              {inputDetail(u"confirmationId", confirmationID)});
                resetActiveStrategyConfirmationUiState();
                ui->priceChart->clearManualConfirmationCue();
                if (m_manualArmedBracket.has_value() &&
                    m_manualArmedBracket->source == ManualArmedBracketState::Source::StrategyConfirmation &&
                    m_manualArmedBracket->confirmationID == confirmationID)
                {
                    clearManualArmedBracket(QStringLiteral("strategy-confirmation-rejected-before-switch"));
                }
                forwardDecision(false, 0.0);
                p_event->accept();
                return true;
            }

            if (switchShortcut)
            {
                logInputEvent(u"GUIFrontend",
                              u"strategy-manual-confirm-switch-noop",
                              {inputDetail(u"confirmationId", confirmationID)});
                p_event->accept();
                return true;
            }

            const bool rejectAndBlockSymbol = rejectAndBlockSymbolShortcut;
            const bool accepted = acceptShortcut;
            const QString symbolToRestore = accepted ? QString() : m_preConfirmationDisplayedSymbol;
            const std::optional<ChartViewRangesSnapshot> rangesToRestore =
                accepted ? std::nullopt : m_preConfirmationChartViewRanges;
            double overrideStopPrice = 0.0;
            if (accepted && m_manualArmedBracket.has_value() &&
                m_manualArmedBracket->source == ManualArmedBracketState::Source::StrategyConfirmation &&
                m_manualArmedBracket->confirmationID == confirmationID)
            {
                QString validationError;
                if (!validateActiveStrategyConfirmationPreviewRisk(&validationError))
                {
                    logInputEvent(
                        u"GUIFrontend",
                        u"strategy-manual-confirm-accept-blocked-risk",
                        {inputDetail(u"confirmationId", confirmationID), inputDetail(u"reason", validationError)});
                    QMessageBox::warning(nullptr, "Risk Limit", validationError);
                    refreshStrategyConfirmationCueCountdown(true);
                    p_event->accept();
                    return true;
                }
                overrideStopPrice = m_manualArmedBracket->stopPrice;
            }
            logInputEvent(u"GUIFrontend",
                          accepted ? u"strategy-manual-confirm-accept"
                                   : (rejectAndBlockSymbol ? u"strategy-manual-confirm-reject-and-block-symbol"
                                                           : u"strategy-manual-confirm-reject"),
                          {inputDetail(u"confirmationId", confirmationID)});
            const bool clearStrategyPreview =
                m_manualArmedBracket.has_value() &&
                m_manualArmedBracket->source == ManualArmedBracketState::Source::StrategyConfirmation &&
                m_manualArmedBracket->confirmationID == confirmationID;
            resetActiveStrategyConfirmationUiState();
            ui->priceChart->clearManualConfirmationCue();
            if (clearStrategyPreview)
            {
                clearManualArmedBracket(QStringLiteral("strategy-confirmation-decision"));
            }
            if (!accepted && !symbolToRestore.isEmpty() && symbolToRestore != currentlyDisplayedSymbol)
            {
                displayStock(symbolToRestore);
                if (rangesToRestore.has_value())
                {
                    ui->priceChart->applyViewRanges(rangesToRestore->xLower,
                                                    rangesToRestore->xUpper,
                                                    rangesToRestore->yLower,
                                                    rangesToRestore->yUpper,
                                                    true);
                }
            }
            if (rejectAndBlockSymbol)
            {
                forwardRejectAndBlockSymbol();
            }
            else
            {
                forwardDecision(accepted, overrideStopPrice);
            }
            p_event->accept();
            return true;
        }
    }

    // Handle main window close event
    if (p_watched == m_mainWindow && p_event->type() == QEvent::Close)
    {
        // Save chart window state NOW, before shutdown closes them
        saveMainWindowGeometry();
        if (m_windowManager)
        {
            m_windowManager->saveWindowState();
            m_windowManager->setShuttingDown();
        }

        // Call shutdown for graceful cleanup (same as Ctrl+Q)
        MainApp::getInstance()->shutdown();
        p_event->accept();
        return true;
    }

    return QObject::eventFilter(p_watched, p_event);
}
