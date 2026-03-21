#include <QJsonDocument>
#include <QHeaderView>
#include <QLabel>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPalette>
#include <QApplication>
#include <QScreen>
#include <QWindow>
#include <QShortcut>
#include <QFont>
#include <QTextCursor>
#include <QScrollBar>
#include <QRegularExpression>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <csignal>
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
#include "Widgets/StrategyQuickView/StrategyQuickView.h"
#include "Widgets/StrategyLogWidget/StrategyLogWidget.h"
#include "Widgets/ReplayControlsBar/ReplayControlsBar.h"
#include "StrategyManager.h"
#include "StockPriceChart/ChartToolbar.h"
#include "StockPriceChart/StockPriceChart.h"
#include "WindowManager/WindowManager.h"
#include "ChartWindow/ChartWindow.h"
#include "Core/PlatformControlProtocol.h"
#include "Misc/Logging/Logging.h"
#include "Misc/Settings.h"
#include "Misc/ShortcutSettings.h"
#include "Core/MainApp.h"
#include "Assume.h"
#include "DBClient.h"
#include "BarUtils.h"
#include "CONSTANTS.h"
#include <QInputDialog>

#define LOGGING_CATEGORY GUIFrontendLog

Q_LOGGING_CATEGORY(GUIFrontendLog, "GUIFrontend")

namespace
{
    QString summarizeClosePositionsSuccess(const ClosePositionsResult& p_result)
    {
        QStringList lines;
        lines << QString("Submitted close-position orders for %1 position(s).").arg(p_result.successCount());
        if (p_result.usesAggressiveLimitOrders)
        {
            lines << QString("Session: %1 (Day+ limit orders, offset %2 c)")
                         .arg(p_result.session)
                         .arg(p_result.aggressivityOffsetCents, 0, 'f', 2);
        }
        else
        {
            lines << QString("Session: %1 (market orders)").arg(p_result.session);
        }

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

        if (p_result.usesAggressiveLimitOrders)
        {
            lines << QString("Session: %1 (Day+ limit orders, offset %2 c)")
                         .arg(p_result.session)
                         .arg(p_result.aggressivityOffsetCents, 0, 'f', 2);
        }
        else
        {
            lines << QString("Session: %1 (market orders)").arg(p_result.session);
        }

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

} // namespace

GUIFrontend::GUIFrontend(MainAlgo* p_mainAlgo, QObject* parent) : FrontEnd(parent), mainAlgo(p_mainAlgo)
{
    ui = std::make_unique<Ui::GUIFrontend>();

    // Create main window with this as parent for proper Qt ownership
    m_mainWindow = new QMainWindow();
    m_mainWindow->setAttribute(Qt::WA_DeleteOnClose, false); // We manage deletion
    ui->setupUi(m_mainWindow);

    this->setObjectName("GUIFrontend");

    setupDarkTheme(m_mainWindow);

    // Install event filter on main window to handle close events
    m_mainWindow->installEventFilter(this);

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


    // Create and setup TradeStation login button
    tradeStationLoginButton = new QPushButton("Login to TradeStation", ui->statusbar);
    tradeStationLoginButton->setFlat(true); // Make it look like a status bar item
    tradeStationLoginButton->setStyleSheet(
        "QPushButton { background-color: #00A0E9; color: #ffffff; padding: 2px 6px; border-radius: 3px; }");
    ui->statusbar->addPermanentWidget(tradeStationLoginButton);

    // Connect TradeStation login button click to launch auth process
    connect(tradeStationLoginButton,
            &QPushButton::clicked,
            this,
            []()
            {
                // GUIAuthHandler is modal, so it's impossible to click the button while authentication is in progress
                ASSUME_FALSE(TSClient::getInstance()->isAuthInProgress());
                TSClient::getInstance()->launchAuthProcess();
            });

    // Create and setup Databento connection button
    m_databentoButton = new QPushButton("Connect to Databento", ui->statusbar);
    m_databentoButton->setFlat(true);
    m_databentoButton->setStyleSheet(
        "QPushButton { background-color: #00A0E9; color: #ffffff; padding: 2px 6px; border-radius: 3px; }");
    ui->statusbar->addPermanentWidget(m_databentoButton);

    connect(m_databentoButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                DEBUG << "Databento button clicked";
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
                               "🌆 AFTER-HOURS: 4:00 PM - 6:59 PM ET\n"
                               "🌚 CLOSED: 7:00 PM - 3:59 AM ET\n"
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

    // Connect chart to replay controls bar so chart slots respond to user input
    ui->priceChart->connectReplayControls(m_replayControlsBar);

    // Add spacer to push mode labels to the right
    auto* rightSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    ui->topControlsLayout->insertSpacerItem(7, rightSpacer);

    // Create tristate trading-mode indicator (right side: LIVE / SIM / REPLAY pills)
    bool isSimMode = (MainApp::getTradingMode() == TradingMode::Sim);
    m_tradingModeBar = new TradingModeBar(m_mainWindow);
    Q_CHECK_PTR(m_tradingModeBar);
    m_tradingModeBar->setActiveMode(isSimMode ? TradingModeBar::Mode::Sim : TradingModeBar::Mode::Live);
    m_tradingModeBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->topControlsLayout->addWidget(m_tradingModeBar, 0, Qt::AlignRight);

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
                    MainApp::setTradingMode(TradingMode::Sim);
                    MainApp::restartApplication();
                }
            });

    connect(m_tradingModeBar,
            &TradingModeBar::replayRequested,
            this,
            [this]()
            {
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
                QString currentSymbol = ui->priceChart->getCurrentSymbol();
                if (!currentSymbol.isEmpty() && !DBClient::getInstance()->hasReplayData(replayDate, currentSymbol))
                {
                    QMessageBox::warning(nullptr,
                                         "No Replay Data for Symbol",
                                         QString("No replay data found for %1 on %2.\n\n"
                                                 "Select a different date or download data first.")
                                             .arg(currentSymbol, replayDate.toString("yyyy-MM-dd")));
                    return;
                }
                MainApp::getInstance()->enterReplayMode(replayDate, replayTime, speed);
            });

    connect(m_tradingModeBar,
            &TradingModeBar::replayExitRequested,
            this,
            []() { MainApp::getInstance()->exitReplayMode(); });

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
            &FrontEnd::tradeStationAuthStateChanged,
            this,
            &GUIFrontend::onTradeStationAuthStateChanged,
            Qt::DirectConnection);

    connect(this,
            &FrontEnd::tradeStationAccountsReceived,
            this,
            &GUIFrontend::onTradeStationAccountsReceived,
            Qt::DirectConnection);

    connect(this,
            &FrontEnd::tradeStationDataUsageUpdated,
            this,
            &GUIFrontend::onTSClientDataUsageUpdate,
            Qt::DirectConnection);

    connect(this,
            &FrontEnd::databentoDataUsageUpdated,
            this,
            &GUIFrontend::onDBClientDataUsageUpdate,
            Qt::DirectConnection);

    connect(this, &FrontEnd::newPositionReceived, this, &GUIFrontend::onNewPositionReceived, Qt::DirectConnection);

    connect(this, &FrontEnd::positionDeleted, this, &GUIFrontend::onPositionDeleted, Qt::DirectConnection);

    connect(this, &FrontEnd::newOrderReceived, this, &GUIFrontend::onNewOrderReceived, Qt::DirectConnection);

    connect(this, &FrontEnd::balanceUpdated, this, &GUIFrontend::onBalanceUpdated, Qt::DirectConnection);

    // When the chart requests missing bars, call the extracted method to handle the request
    connect(ui->priceChart, &StockPriceChart::requestMissingBars, this, &GUIFrontend::requestMissingBarsFromCache);

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

    // Persist replay start time whenever it changes so next launch restores it
    connect(m_replayControlsBar,
            &ReplayControlsBar::replayStartTimeChanged,
            this,
            [this](const QTime& time)
            {
                Q_CHECK_PTR(appStateSettings);
                appStateSettings->setValue("Replay/StartTime", time.toString(Qt::ISODate));
                appStateSettings->sync();
            });

    // Forward strategy log markers to the chart
    connect(MainAlgo::getInstance(),
            &MainAlgo::strategyLogEmitted,
            ui->priceChart,
            &StockPriceChart::onStrategyLogEmitted,
            Qt::QueuedConnection);

    // Connect the stock symbol input to its slot
    connect(ui->stockSymbolInput, &QLineEdit::returnPressed, this, &GUIFrontend::onNewDisplayedStockSelection);

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
                cancelFuture.then(this,
                                  [this, orderId](std::expected<CancelOrderResult, TSClient::Error> result)
                                  {
                                      if (result.has_value())
                                      {
                                          CancelOrderResult& cancelResult = result.value();

                                          qCInfo(GUIFrontendLog) << "Order" << orderId << "cancelled successfully.";

                                          if (cancelResult.isError())
                                          {
                                              qCWarning(GUIFrontendLog) << "Failed to cancel order" << orderId << ":"
                                                                        << cancelResult.getMessage();
                                              // TODO: Show error message to user
                                              Q_UNREACHABLE();
                                          }
                                          else
                                          {
                                              qCInfo(GUIFrontendLog)
                                                  << "Order" << orderId
                                                  << "cancelled successfully:" << cancelResult.getMessage();
                                          }
                                      }
                                      else
                                      {
                                          qCWarning(GUIFrontendLog)
                                              << "Failed to cancel order" << orderId
                                              << "- Error code:" << static_cast<int>(result.error());
                                          Q_UNREACHABLE();
                                      }
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
    DownloadsTab* downloadsTab = new DownloadsTab();
    ui->tabWidget->addTab(downloadsTab, "Downloads");

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

    // Set up the config tab
    ConfigTab* configTab = new ConfigTab();
    configTab->setTimeAndSalesWidget(ui->timeAndSalesWidget);
    ui->tabWidget->addTab(configTab, "Config");

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

        m_loggerSplitter = new QSplitter(Qt::Horizontal, m_loggerContainer);
        m_loggerSplitter->setHandleWidth(4);
        containerLayout->addWidget(m_loggerSplitter);

        // Reparent liveLogDisplay into the horizontal splitter
        if (ui->liveLogDisplay)
        {
            // Remove from its current parent (mainSplitter), re-add to loggerSplitter
            ui->liveLogDisplay->setParent(m_loggerSplitter);
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

    // Configure the splitter to make the bottom panel (with balances, positions, orders, order entry) as compact as possible
    // Give the top widget (chart) a stretch factor of 1 and bottom widget a stretch factor of 0
    ui->tradeTabSplitter->setStretchFactor(0,
                                           1); // tradeTopWidget gets stretch factor 1
    ui->tradeTabSplitter->setStretchFactor(1,
                                           0); // tradeTabBottomWidget gets stretch factor 0 (minimum size)

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
                restoreReplayState();
                // Restore strategies AFTER replay mode is set up (enterReplayMode posts
                // stopAllStrategies via QueuedConnection; this queues behind it).
                QMetaObject::invokeMethod(mainAlgo, &MainAlgo::restoreStrategiesState, Qt::QueuedConnection);
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

    // Restore chart windows from previous session (after a short delay so main window is settled)
    QTimer::singleShot(600, this, [this]() { m_windowManager->restoreWindowState(); });
}

GUIFrontend::~GUIFrontend()
{
    // Note: window state is saved before shutdown begins (in eventFilter / Ctrl+Q handler).
    // Do NOT save here — chart windows may already be destroyed by this point.

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
    QString message = "TS: " + bytesToString(TSClientDataUsage) + " | DB: " + bytesToString(m_dbClientDataUsage) +
                      " | Mem: " + bytesToString(memoryUsage);

    ui->statusbar->showMessage(message);
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

void GUIFrontend::onTradeStationAccountsReceived(QVector<Account> results)
{
    m_accounts = results; // Store accounts

    // Populate account selector
    ui->accountSelector->clear();
    for (const Account& account: results)
    {
        ui->accountSelector->addItem(
            QString("%1 (%2)").arg(account.getAccountId(),
                                   AccountType::accountTypeToString(account.getAccountType().type)),
            account.getAccountId());
    }

    // Set default to last account (as requested)
    if (!results.isEmpty())
    {
        ui->accountSelector->setCurrentIndex(results.size() - 1);
        // Show the info button now that we have accounts
        m_accountInfoButton->setVisible(true);
    }

    // Pass accounts to order entry widget (for submit button enabling)
    ui->orderEntryWidget->setAccounts(results);
}

QString GUIFrontend::getSelectedAccountId() const
{
    if (ui->accountSelector->currentIndex() >= 0 && ui->accountSelector->currentIndex() < m_accounts.size())
    {
        return m_accounts[ui->accountSelector->currentIndex()].getAccountId();
    }
    return QString(); // Return empty string if no valid selection
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
            L2T_TP(l2trader, gui_pull_tick, dirtyFlags);

            if (snap.l2Dirty)
            {
                Level2 l2 = *snap.latestLevel2;
                snap.l2Dirty = false;
                lock.unlock();

                L2T_TP(l2trader, gui_level2_received, sc->symbol.toUtf8().constData());
                ui->level2Widget->updateData(l2.m_bids, l2.m_asks);
                ui->orderEntryWidget->onMarketDepthUpdate(sc->symbol, l2);

                lock.relock();
            }

            if (snap.tradeDirty)
            {
                QVector<Trade> trades;
                trades.swap(snap.pendingTrades);
                snap.tradeDirty = false;
                lock.unlock();

                for (const auto& t: trades)
                {
                    L2T_TP(l2trader, gui_trade_received, sc->symbol.toUtf8().constData());
                    ui->timeAndSalesWidget->onNewTrade(sc->symbol, t);
                }

                lock.relock();
            }

            if (snap.barDirty && m_currentTimeFrame == TimeFrame::ONE_MINUTE)
            {
                Bar bar = *snap.latestBar;
                snap.barDirty = false;
                lock.unlock();

                L2T_TP(l2trader, gui_bar_received, sc->symbol.toUtf8().constData(), 60);
                ui->priceChart->addLiveBar(sc->symbol, bar);

                lock.relock();
            }

            if (snap.aggregatorDirty)
            {
                auto it = snap.aggregatorBars.find(m_currentTimeFrame);
                if (it != snap.aggregatorBars.end())
                {
                    Bar bar = it.value();
                    lock.unlock();

                    L2T_TP(l2trader,
                           gui_bar_received,
                           sc->symbol.toUtf8().constData(),
                           BarUtils::secondsPerBar(m_currentTimeFrame));
                    ui->priceChart->addLiveBar(sc->symbol, bar);

                    lock.relock();
                }
                snap.aggregatorDirty = false;
            }

            if (snap.replayTimeDirty)
            {
                QDateTime replayTime = *snap.replayTime;
                snap.replayTimeDirty = false;
                lock.unlock();

                L2T_TP(l2trader, gui_replay_time_updated, replayTime.toMSecsSinceEpoch());
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
    L2T_TP(l2trader, gui_position_received, position.getSymbol().toUtf8().constData());

    ui->positionWidget->updatePosition(account, position);

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
}

void GUIFrontend::onNewOrderReceived(QString account, Order order)
{
    L2T_TP(l2trader,
           gui_order_received,
           order.getSymbol().toUtf8().constData(),
           static_cast<int>(order.getOrderStatus()));

    ui->orderWidget->updateOrder(account, order);

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
        else if (status == Order::Status::CAN || status == Order::Status::UCN || status == Order::Status::TSC)
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
            restoreReplayState();
            // Restore strategies AFTER replay mode is set up (enterReplayMode posts
            // stopAllStrategies via QueuedConnection; this queues behind it).
            QMetaObject::invokeMethod(mainAlgo, &MainAlgo::restoreStrategiesState, Qt::QueuedConnection);
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
        m_databentoButton->setText("Databento: Subscription Error");
        m_databentoButton->setStyleSheet(
            "QPushButton { background-color: #FF8C00; color: #ffffff; padding: 2px 6px; border-radius: 3px; }");
        m_databentoButton->setEnabled(true);

        // Show a prominent dialog so the user cannot miss it
        QMessageBox msgBox(m_mainWindow);
        msgBox.setWindowTitle("Databento Live Subscription Error");
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText("<b>Live data subscription is not available.</b>");
        msgBox.setInformativeText("Databento returned an error that prevents live streaming:\n\n" + errorText +
                                  "\n\nCheck that your Databento account has an active live data subscription "
                                  "for this dataset. Replay mode will still work with previously downloaded data.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    }
    else
    {
        WARNING << "Non-fatal Databento gateway message:" << errorText;
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

    // Validate the stock symbol
    if (!isValidStockSymbol(symbol))
    {
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

    // Display the stock (also updates input widget and persists)
    displayStock(symbol);

    // Clear focus from the input box after processing
    ui->stockSymbolInput->clearFocus();
}

void GUIFrontend::displayStock(const QString& symbol)
{
    if (symbol == currentlyDisplayedSymbol)
    {
        qWarning() << "Symbol " << symbol << " is already the currently displayed symbol";
        return;
    }

    currentlyDisplayedSymbol = symbol;

    // Update the stock symbol input widget to reflect the new symbol
    ui->stockSymbolInput->setText(symbol);

    // Persist so the app restores this symbol on next startup
    saveLastDisplayedStock(symbol);

    ui->priceChart->setSymbol(symbol);

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

    // Display the stock without saving again
    displayStock(lastSymbol);
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

    QDate savedDate = QDate::fromString(appStateSettings->value("Replay/Date").toString(), Qt::ISODate);

    if (!savedDate.isValid())
    {
        qWarning() << "Saved replay date is invalid, skipping replay restore";
        return;
    }

    // Populate the replay day list so setSelectedReplayDay works
    m_replayControlsBar->scanAndPopulateReplayDays();

    if (!DBClient::getInstance()->hasReplayData(savedDate, ui->priceChart->getCurrentSymbol()))
    {
        qWarning() << "Saved replay date" << savedDate << "has no data, skipping replay restore";
        return;
    }

    m_replayControlsBar->setSelectedReplayDay(savedDate);

    QTime startTime = savedTime.isValid() ? savedTime : m_replayControlsBar->getReplayStartTime();
    Playback::Speed speed = m_replayControlsBar->getReplaySpeed();

    qInfo() << "Restoring replay state: date=" << savedDate << "time=" << startTime;
    MainApp::getInstance()->enterReplayMode(savedDate, startTime, speed);
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

void GUIFrontend::onOrderPlaced(const PlaceOrderRequest& order)
{
    qInfo() << "Placing order:" << order.toJsonString();

    // Submit order to TSClient
    QFuture<std::expected<PlaceOrderResult, TSClient::Error>> future = TSClient::getInstance()->placeOrder(order);

    future.then(this,
                [this](std::expected<PlaceOrderResult, TSClient::Error> expected_result)
                {
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
                        qCritical() << "Order placement failed with error code:"
                                    << static_cast<int>(expected_result.error());
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
    if (MainApp::isInReplayMode())
        emit m_tradingModeBar->replayExitRequested();
    else
        emit m_tradingModeBar->replayRequested();
}

void GUIFrontend::onCancelAllOrders()
{
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
    submitClosePositionsRequest(request,
                                "Close All Positions",
                                QString("No open positions were found for account %1.").arg(selectedAccountId));
}

void GUIFrontend::onClosePosition(const QString& p_positionID)
{
    QVector<Position> positions;
    const bool invoked = QMetaObject::invokeMethod(
        mainAlgo,
        [this, &positions]() { positions = this->mainAlgo->getCurrentPositionsSnapshot(); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);

    for (const Position& position: positions)
    {
        if (position.getPositionID() != p_positionID)
        {
            continue;
        }

        ClosePositionsRequest request;
        request.accountId = position.getAccountID();
        request.symbols = {position.getSymbol()};
        Q_CHECK_PTR(appStateSettings);
        request.aggressivityOffsetCents =
            appStateSettings
                ->value(ClosePositionsConstants::SETTINGS_KEY_AGGRESSIVE_LIMIT_OFFSET_CENTS,
                        ClosePositionsConstants::DEFAULT_AGGRESSIVE_LIMIT_OFFSET_CENTS)
                .toDouble();

        submitClosePositionsRequest(request,
                                    "Close Position",
                                    QString("No open position was found for %1 in account %2.")
                                        .arg(position.getSymbol(), position.getAccountID()));
        return;
    }

    const QString message = QString("Position %1 is no longer available to close.").arg(p_positionID);
    QMessageBox::information(nullptr, "Close Position", message);
    qInfo() << message;
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

void GUIFrontend::requestMissingBarsFromCache(const QDateTime& from, const QDateTime& to)
{
    DEBUG << "Request missing bars from " << from << " to " << to;

    // Guard against the race where setSymbol() fires on the GUI thread but the queued
    // onSelectDisplayedStock() hasn't reached MainAlgo yet (cross-thread delivery).
    // In that case MainAlgo's m_currentDisplayedSymbolContext still points to the OLD
    // symbol, so we would paint bars from the wrong BarCache.  Retry in 50ms — by that
    // time the queued event will have been processed.
    const QString expectedSymbol = ui->priceChart->getCurrentSymbol();
    if (MainAlgo::getInstance()->getDisplayedSymbol() != expectedSymbol)
    {
        DEBUG << "Symbol mismatch (MainAlgo:" << MainAlgo::getInstance()->getDisplayedSymbol()
              << "vs chart:" << expectedSymbol << ") — retrying in 50ms";
        QTimer::singleShot(50, this, [this, from, to]() { requestMissingBarsFromCache(from, to); });
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
            ui->priceChart->onRequestedMissingBarsFailed();
        }
        else
        {
            ui->priceChart->onRequestedMissingBarsReceived(bars);
        }
    }
    else if (std::holds_alternative<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(result))
    {
        std::get<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(result).then(
            this,
            [this, from, to](std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>&& bars)
            {
                if (bars.has_value())
                {
                    if (bars.value()->isEmpty())
                    {
                        qCWarning(GUIFrontendLog) << "Historical fetch returned 0 bars";
                        ui->priceChart->onRequestedMissingBarsFailed();
                    }
                    else
                    {
                        ui->priceChart->onRequestedMissingBarsReceived(bars.value());
                    }
                }
                else
                {
                    qCritical() << "Failed to get missing bars from BarCache - Error:"
                                << QtEnum::toString(bars.error());

                    // Notify the chart that the request failed so it can release the semaphore
                    ui->priceChart->onRequestedMissingBarsFailed();

                    // Retry after 1 second using the same pattern as TSClient error handling
                    QTimer::singleShot(1000,
                                       this,
                                       [this, from, to]()
                                       {
                                           qInfo() << "Retrying missing bars request from" << from << "to" << to;
                                           requestMissingBarsFromCache(from, to);
                                       });
                }
            });
    }
    else
    {
        qCritical() << "Unexpected result type from requestMissingBarsDisplayedStock";
        ui->priceChart->onRequestedMissingBarsFailed();
    }
}

void GUIFrontend::onTimeFrameChanged(TimeFrame tf)
{
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
}

void GUIFrontend::onReplayModeEntered()
{
    L2T_TP(l2trader, gui_replay_entered);

    qCInfo(GUIFrontendLog) << "Replay mode entered";

    // Persist replay state so we can restore it on next launch
    saveReplayState(true, m_replayControlsBar->getSelectedReplayDay(), m_replayControlsBar->getReplayStartTime());

    // Update tristate mode bar to show REPLAY as active
    m_tradingModeBar->setActiveMode(TradingModeBar::Mode::Replay);

    // Clear live orders and positions from widgets (replay starts with clean slate)
    ui->orderWidget->clearAllOrders();
    ui->positionWidget->clearAllPositions();

    // Clear chart data for fresh replay (bar caches are cleared separately by MainAlgo)
    ui->priceChart->clearChart();

    // Clear market depth table — stale live data must not carry over into replay
    ui->level2Widget->clearData();
    ui->timeAndSalesWidget->clearData();

    // TODO Phase 6: probe DBClient .dbn replay file to detect available schemas (MBP-10, MBP-1, etc.)
    // For now, clear the mode indicator (no legacy TS recorded data)
    ui->level2Widget->setExpectedDataMode(false);

    // Show replay controls bar and ensure play button is in stopped state
    m_replayControlsBar->setVisible(true);
    m_replayControlsBar->setReplayPlaying(false);

    // Set controls to PreloadingPaused state (data loaded, waiting for user to press play)
    m_replayControlsBar->setReplayState(ReplayControlsBar::ReplayState::PreloadingPaused);

    // Connect replay data load error signal to chart error handler
    connect(DBClient::getInstance(),
            &DBClient::replayDataLoadFailed,
            ui->priceChart,
            &StockPriceChart::onReplayDataLoadFailed,
            Qt::UniqueConnection);

    // Update chart visual (background color and watermark)
    ui->priceChart->setReplayModeActive(true);

    // Switch all secondary chart windows to the replay symbol
    QString replaySymbol = currentlyDisplayedSymbol;
    for (ChartWindow* cw: m_windowManager->chartWindows())
        cw->enterReplayMode(replaySymbol);

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
        m_replayControlsBar->setReplayPlaying(true);
        if (MainApp::isInReplayMode())
        {
            m_replayControlsBar->setReplayState(ReplayControlsBar::ReplayState::Playing);
            m_tradingModeBar->setActiveMode(TradingModeBar::Mode::Replay);
        }
        break;

    case Playback::State::Paused:
        m_replayControlsBar->setReplayPlaying(false);
        if (MainApp::isInReplayMode())
        {
            m_replayControlsBar->setReplayState(ReplayControlsBar::ReplayState::Paused);
            m_tradingModeBar->setActiveMode(TradingModeBar::Mode::Replay);
        }
        break;

    case Playback::State::Stopped:
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

void GUIFrontend::onReplayModeExited()
{
    L2T_TP(l2trader, gui_replay_exited);

    qCInfo(GUIFrontendLog) << "Replay mode exited";

    // Clear persisted replay state
    saveReplayState(false);

    // Update tristate mode bar back to LIVE or SIM (whichever is the brokerage mode)
    bool inSimMode = (MainApp::getTradingMode() == TradingMode::Sim);
    m_tradingModeBar->setActiveMode(inSimMode ? TradingModeBar::Mode::Sim : TradingModeBar::Mode::Live);

    // Reset play button state and hide replay controls bar
    m_replayControlsBar->setReplayPlaying(false);
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

    // Restore all secondary chart windows to their pre-replay symbols
    for (ChartWindow* cw: m_windowManager->chartWindows())
        cw->exitReplayMode();

    // Update session label and time display (back to live time)
    updateSessionLabel();
    updateTimeDisplay();
}

void GUIFrontend::onTradingModeConfigured(const TradingMode p_mode)
{
    if (MainApp::isInReplayMode())
    {
        return;
    }

    m_tradingModeBar->setActiveMode(p_mode == TradingMode::Sim ? TradingModeBar::Mode::Sim
                                                               : TradingModeBar::Mode::Live);
}

bool GUIFrontend::eventFilter(QObject* p_watched, QEvent* p_event)
{
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
