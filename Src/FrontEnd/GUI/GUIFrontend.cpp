#include <QJsonDocument>
#include <QHeaderView>
#include <QLabel>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPalette>
#include <QApplication>
#include <QShortcut>
#include <QFont>
#include <QTextCursor>
#include <QScrollBar>
#include <QRegularExpression>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QFile>
#include "Assume.h"

#include "TSClient.h"
#include "GUIFrontend.h"
#include "ui_GUIFrontend.h"
#include "Tabs/LoggingTab.h"
#include "Tabs/RecordsInfoTab.h"
#include "Tabs/ShortcutsTab.h"
#include "Tabs/ConfigTab.h"
#include "Tabs/CacheTab.h"
#include "Tabs/StrategiesTab/StrategiesTab.h"
#include "StockPriceChart/ChartToolbar.h"
#include "StockPriceChart/StockPriceChart.h"
#include "Misc/Logging.h"
#include "Misc/Settings.h"
#include "Misc/ShortcutSettings.h"
#include "Core/MainApp.h"
#include "Assume.h"
#include "DBClient.h"
#include <QInputDialog>

#define LOGGING_CATEGORY GUIFrontendLog

Q_LOGGING_CATEGORY(GUIFrontendLog, "GUIFrontend")

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

    m_mainWindow->showMaximized();

    // Initialize shortcuts from settings
    ShortcutSettings& shortcutSettings = ShortcutSettings::getInstance();

    // Add Ctrl+Q shortcut to quit the application gracefully
    m_quitShortcut = new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::QuitApplication), m_mainWindow);
    // Connect to MainApp::shutdown() for graceful shutdown instead of abrupt quit
    auto quitConnection = connect(m_quitShortcut, &QShortcut::activated, []() { MainApp::getInstance()->shutdown(); });
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

    // Connect to shortcut changes to update active shortcuts
    auto shortcutChangeConnection = connect(&shortcutSettings,
                                            &ShortcutSettings::shortcutChanged,
                                            this,
                                            &GUIFrontend::onShortcutChanged,
                                            Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(shortcutChangeConnection);


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
                               "🌙 EARLY PRE-MARKET: 4:01 AM - 6:00 AM ET\n"
                               "🌅 PRE-MARKET: 6:01 AM - 9:30 AM ET\n"
                               "📈 REGULAR: 9:31 AM - 4:00 PM ET\n"
                               "🌆 AFTER-HOURS: 4:01 PM - 8:00 PM ET\n"
                               "🌙 CLOSED: 8:01 PM - 4:00 AM ET");
    ui->topControlsLayout->insertWidget(4, m_sessionLabel);
    updateSessionLabel();

    // Create MarketFlags status labels (start in inactive/grey state)
    // Inactive: dark grey background, muted text
    // Active: bright colored background matching the alert level
    static const QString inactiveStyle = "QLabel { background-color: #3a3a3a; color: #808080; padding: 4px 8px; "
                                         "border-radius: 4px; font-weight: bold; }";

    m_haltedLabel = new QLabel("HALTED", m_mainWindow);
    Q_CHECK_PTR(m_haltedLabel);
    m_haltedLabel->setStyleSheet(inactiveStyle);
    m_haltedLabel->setToolTip("Trading is halted for this symbol");
    ui->topControlsLayout->insertWidget(5, m_haltedLabel);

    m_delayedLabel = new QLabel("DELAYED", m_mainWindow);
    Q_CHECK_PTR(m_delayedLabel);
    m_delayedLabel->setStyleSheet(inactiveStyle);
    m_delayedLabel->setToolTip("Data is delayed (not real-time)");
    ui->topControlsLayout->insertWidget(6, m_delayedLabel);

    m_hardToBorrowLabel = new QLabel("HTB", m_mainWindow);
    Q_CHECK_PTR(m_hardToBorrowLabel);
    m_hardToBorrowLabel->setStyleSheet(inactiveStyle);
    m_hardToBorrowLabel->setToolTip("Hard to borrow - short selling may be restricted");
    ui->topControlsLayout->insertWidget(7, m_hardToBorrowLabel);

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
    ui->topControlsLayout->insertWidget(8, m_timeDisplayLabel);

    // Add spacer to push mode labels to the right
    auto* rightSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    ui->topControlsLayout->insertSpacerItem(9, rightSpacer);

    // Create trading mode indicator (right side: SIM/LIVE) - clickable to toggle
    bool isSimMode = (MainApp::getTradingMode() == TradingMode::Sim);
    m_tradingModeLabel = new QLabel(isSimMode ? "🔵 SIM" : "🟠 LIVE", m_mainWindow);
    Q_CHECK_PTR(m_tradingModeLabel);
    m_tradingModeLabel->setAlignment(Qt::AlignCenter);
    m_tradingModeLabel->setMinimumWidth(65);
    m_tradingModeLabel->setStyleSheet(isSimMode
                                          ? "QLabel { background-color: #1E90FF; color: #ffffff; padding: 4px 8px; "
                                            "border-radius: 4px; font-weight: bold; }"
                                          : "QLabel { background-color: #FF8C00; color: #ffffff; padding: 4px 8px; "
                                            "border-radius: 4px; font-weight: bold; }");
    m_tradingModeLabel->setToolTip("Click to toggle between SIM and LIVE trading mode (requires restart)");
    m_tradingModeLabel->setCursor(Qt::PointingHandCursor);
    m_tradingModeLabel->installEventFilter(this);
    m_tradingModeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); // Fixed size to stick to right
    ui->topControlsLayout->addWidget(m_tradingModeLabel, 0, Qt::AlignRight);

    // Create data source indicator (right side: LIVE/REPLAY) - clickable to toggle (only visible in SIM mode)
    m_dataSourceLabel = new QLabel("🟢 LIVE", m_mainWindow);
    Q_CHECK_PTR(m_dataSourceLabel);
    m_dataSourceLabel->setAlignment(Qt::AlignCenter);
    m_dataSourceLabel->setMinimumWidth(80);
    m_dataSourceLabel->setStyleSheet("QLabel { background-color: #228B22; color: #ffffff; padding: 4px 8px; "
                                     "border-radius: 4px; font-weight: bold; }");
    m_dataSourceLabel->setToolTip("Click to toggle between LIVE data and REPLAY mode");
    m_dataSourceLabel->setCursor(Qt::PointingHandCursor);
    m_dataSourceLabel->installEventFilter(this);
    m_dataSourceLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); // Fixed size to stick to right
    ui->topControlsLayout->addWidget(m_dataSourceLabel, 0, Qt::AlignRight);

    // Set up timer to update clock every second in LIVE mode
    m_timeUpdateTimer = new QTimer(this);
    Q_CHECK_PTR(m_timeUpdateTimer);
    bool connected =
        connect(m_timeUpdateTimer, &QTimer::timeout, this, &GUIFrontend::updateTimeDisplay, Qt::UniqueConnection);
    ASSUME_TRUE(connected);
    m_timeUpdateTimer->start(1000); // Update every second
    updateTimeDisplay();            // Initial update

    // In LIVE trading mode, hide data source label and replay widgets (replay not available with real money)
    // In SIM mode, show data source label but hide replay widgets until user enters replay mode
    if (!isSimMode)
    {
        m_dataSourceLabel->setVisible(false);
    }
    ui->priceChart->toolbar()->setReplayWidgetsVisible(false);

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

    connect(this,
            &FrontEnd::currentHighlightedStockBarReceived,
            this,
            &GUIFrontend::onCurrentHighlightedStockBarReceived,
            Qt::DirectConnection);

    connect(this,
            &FrontEnd::currentHighlightedReceivedNewLevel2,
            this,
            &GUIFrontend::onCurrentHighlightedReceivedNewLevel2,
            Qt::DirectConnection);

    connect(this, &FrontEnd::newPositionReceived, this, &GUIFrontend::onNewPositionReceived, Qt::DirectConnection);

    connect(this, &FrontEnd::positionDeleted, this, &GUIFrontend::onPositionDeleted, Qt::DirectConnection);

    connect(this, &FrontEnd::newOrderReceived, this, &GUIFrontend::onNewOrderReceived, Qt::DirectConnection);

    connect(this, &FrontEnd::balanceUpdated, this, &GUIFrontend::onBalanceUpdated, Qt::DirectConnection);

    // When the chart requests missing bars, call the extracted method to handle the request
    connect(ui->priceChart, &StockPriceChart::requestMissingBars, this, &GUIFrontend::requestMissingBarsFromCache);

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

    // Connect position window symbol click
    connect(ui->positionWindow,
            &PositionWindow::symbolClicked,
            this,
            [this](const QString& symbol)
            {
                ui->stockSymbolInput->setText(symbol);
                ui->stockSymbolInput->returnPressed(); // Simulate Enter key press
            });

    // Connect order window symbol click
    connect(ui->orderWindow,
            &OrderWindow::symbolClicked,
            this,
            [this](const QString& symbol)
            {
                ui->stockSymbolInput->setText(symbol);
                ui->stockSymbolInput->returnPressed(); // Simulate Enter key press
            });

    // Connect order window cancel order request
    connect(ui->orderWindow,
            &OrderWindow::cancelOrderRequested,
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

    // Set up the strategies tab (second tab)
    StrategiesTab* strategiesTab = new StrategiesTab(mainAlgo);
    ui->tabWidget->addTab(strategiesTab, "Strategies");

    // Set up the records info tab
    RecordsInfoTab* recordsInfoTab = new RecordsInfoTab();
    ui->tabWidget->addTab(recordsInfoTab, "Records Info");

    // Set up the logging tab
    LoggingTab* loggingTab = new LoggingTab();
    ui->tabWidget->addTab(loggingTab, "Logging");

    // Connect logging tab signals
    connect(loggingTab, &LoggingTab::loggerVisibilityChanged, this, &GUIFrontend::onLoggerVisibilityChanged);
    connect(loggingTab, &LoggingTab::logDepthChanged, this, &GUIFrontend::onLogDepthChanged);

    // Set initial logger visibility and log depth based on persisted settings
    if (ui->liveLogDisplay)
    {
        Q_CHECK_PTR(appStateSettings);
        bool loggerVisible = appStateSettings->value("Logging/LoggerVisible", true).toBool();
        ui->liveLogDisplay->setVisible(loggerVisible);

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

    // Configure the splitter to make the bottom panel (with balances, positions, orders, order entry) as compact as possible
    // Give the top widget (chart) a stretch factor of 1 and bottom widget a stretch factor of 0
    ui->tradeTabSplitter->setStretchFactor(0,
                                           1); // tradeTopWidget gets stretch factor 1
    ui->tradeTabSplitter->setStretchFactor(1,
                                           0); // tradeTabBottomWidget gets stretch factor 0 (minimum size)

    // NOTE: Don't restore the last displayed stock here - wait for authentication
    // It will be restored in onTradeStationAuthStateChanged() when authenticated
}

GUIFrontend::~GUIFrontend()
{
    // Delete main window explicitly (owns all child widgets via Qt parent-child)
    delete m_mainWindow;
    // ui is automatically deleted by std::unique_ptr
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

void GUIFrontend::onCurrentHighlightedStockBarReceived(QString symbol, Bar bar)
{
    ui->priceChart->addLiveBar(symbol, bar);
}

void GUIFrontend::onCurrentHighlightedReceivedNewLevel2(QString symbol,
                                                        Level2 level2,
                                                        double bidAskImbalance,
                                                        double bidDWP,
                                                        double askDWP)
{
    ui->marketDepthTable->updateData(level2.m_bids, level2.m_asks);
    ui->marketDepthTable->updateDWP(bidDWP, askDWP);

    // Update the BAI gauge with the bid-ask imbalance
    ui->baiGauge->setValue(bidAskImbalance);

    // Forward market depth update to OrderEntryWidget for sticky price feature
    ui->orderEntryWidget->onMarketDepthUpdate(symbol, level2);
}

void GUIFrontend::onNewPositionReceived(QString account, Position position)
{
    ui->positionWindow->updatePosition(account, position);

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
}

void GUIFrontend::onPositionDeleted(QString account, QString positionID)
{
    ui->positionWindow->onPositionDeleted(account, positionID);
}

void GUIFrontend::onNewOrderReceived(QString account, Order order)
{
    ui->orderWindow->updateOrder(account, order);

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
}

void GUIFrontend::onBalanceUpdated(Balance balance)
{
    ui->balanceWindow->updateBalance(balance);
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

    // Update the input field to show the uppercase symbol
    ui->stockSymbolInput->setText(symbol);

    // Display the stock
    displayStock(symbol);

    // Save the selected stock to settings for restoration on next startup
    saveLastDisplayedStock(symbol);

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

    ui->priceChart->clearSymbol();
    ui->priceChart->setSymbol(symbol);

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
    if (ui->liveLogDisplay)
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
    }
}

void GUIFrontend::onToggleReplayPlayPause()
{
    // Only works in replay mode
    if (MainApp::getDataSourceMode() != DataSourceMode::Replay)
    {
        return;
    }

    // Toggle via the toolbar method which clicks the button and emits the signal
    ui->priceChart->toolbar()->togglePlayPause();
}

void GUIFrontend::onToggleReplayMode()
{
    // Simulate a click on the data source label — identical to clicking the LIVE/REPLAY button
    QMouseEvent fakeClick(QEvent::MouseButtonRelease,
                          QPointF(),
                          QPointF(),
                          Qt::LeftButton,
                          Qt::LeftButton,
                          Qt::NoModifier);
    eventFilter(m_dataSourceLabel, &fakeClick);
}

void GUIFrontend::onCancelAllOrders()
{
    // Get only cancellable order IDs from the order window (filters by status)
    QStringList orderIds = ui->orderWindow->getCancellableOrderIds();

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

void GUIFrontend::requestMissingBarsFromCache(const QDateTime& from, const QDateTime& to)
{
    OBJ_ASSUME_EQUAL(from.date(), to.date()); // Currently only support same-day requests

    DEBUG << "Request missing bars from " << from << " to " << to;

    BarCache::GetBarsResult_t result =
        MainAlgo::getInstance()->requestMissingBarsDisplayedStock(from.date(), from.time(), to.time());

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
    default:
        Q_UNREACHABLE();
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
    qCInfo(GUIFrontendLog) << "Replay mode entered";

    // Update data source indicator to show REPLAY
    ASSUME_DIFF(m_dataSourceLabel, nullptr);
    m_dataSourceLabel->setText("🔴 REPLAY");
    m_dataSourceLabel->setStyleSheet("QLabel { background-color: #8B0000; color: #ffffff; padding: 4px 8px; "
                                     "border-radius: 4px; font-weight: bold; font-weight: bold; }");

    // Clear live orders and positions from widgets (replay starts with clean slate)
    ui->orderWindow->clearAllOrders();
    ui->positionWindow->clearAllPositions();

    // Clear chart data for fresh replay (bar caches are cleared separately by MainAlgo)
    ui->priceChart->clearChart();

    // Clear market depth table — stale live data must not carry over into replay
    ui->marketDepthTable->clearData();

    // TODO Phase 6: probe DBClient .dbn replay file to detect available schemas (MBP-10, MBP-1, etc.)
    // For now, clear the mode indicator (no legacy TS recorded data)
    ui->marketDepthTable->setExpectedDataMode(false, false);

    // Show replay widgets in toolbar and ensure play button is in stopped state
    ui->priceChart->toolbar()->setReplayWidgetsVisible(true);
    ui->priceChart->toolbar()->setReplayPlaying(false);

    // Set toolbar to PreloadingPaused state (data loaded, waiting for user to press play)
    ui->priceChart->toolbar()->setReplayState(ChartToolbar::ReplayState::PreloadingPaused);

    // Connect replay engine error signal to chart error handler
    ReplayEngine* replayEngine = MainApp::getInstance()->getReplayEngine();
    if (replayEngine != nullptr)
    {
        connect(replayEngine,
                &ReplayEngine::replayDataLoadFailed,
                ui->priceChart,
                &StockPriceChart::onReplayDataLoadFailed,
                Qt::UniqueConnection);
    }

    // Update chart visual (background color and watermark)
    ui->priceChart->setReplayModeActive(true);

    // Update session label and time display (replay time may have changed)
    updateSessionLabel();
    updateTimeDisplay();
}

void GUIFrontend::onReplayModeExited()
{
    qCInfo(GUIFrontendLog) << "Replay mode exited";

    // Update data source indicator to show LIVE
    ASSUME_DIFF(m_dataSourceLabel, nullptr);
    m_dataSourceLabel->setText("🟢 LIVE");
    m_dataSourceLabel->setStyleSheet("QLabel { background-color: #228B22; color: #ffffff; padding: 4px 8px; "
                                     "border-radius: 4px; font-weight: bold; }");

    // Reset play button state and hide replay widgets
    ui->priceChart->toolbar()->setReplayPlaying(false);
    ui->priceChart->toolbar()->setReplayWidgetsVisible(false);

    // Set toolbar state back to Inactive
    ui->priceChart->toolbar()->setReplayState(ChartToolbar::ReplayState::Inactive);

    // Clear chart data (MainAlgo will clear caches and restart live stream)
    ui->priceChart->clearChart();

    // Clear market depth table — replay data must not carry over into live mode
    ui->marketDepthTable->clearData();

    // Restore chart visual
    ui->priceChart->setReplayModeActive(false);

    // Update session label and time display (back to live time)
    updateSessionLabel();
    updateTimeDisplay();
}

void GUIFrontend::onReplayTimeUpdated(QDateTime currentTime)
{
    Q_UNUSED(currentTime)
    // Update session label and time display as replay time advances
    updateSessionLabel();
    updateTimeDisplay();
}

bool GUIFrontend::eventFilter(QObject* p_watched, QEvent* p_event)
{
    // Handle main window close event
    if (p_watched == m_mainWindow && p_event->type() == QEvent::Close)
    {
        // Call shutdown for graceful cleanup (same as Ctrl+Q)
        MainApp::getInstance()->shutdown();
        p_event->accept();
        return true;
    }

    // Handle click on trading mode label
    if (p_watched == m_tradingModeLabel && p_event->type() == QEvent::MouseButtonRelease)
    {
        // Get current and target modes
        TradingMode currentMode = MainApp::getTradingMode();
        TradingMode newMode = (currentMode == TradingMode::Sim) ? TradingMode::Live : TradingMode::Sim;
        QString currentModeStr = (currentMode == TradingMode::Sim) ? "SIM" : "LIVE";
        QString newModeStr = (newMode == TradingMode::Sim) ? "SIM" : "LIVE";

        // Show confirmation dialog
        QMessageBox::StandardButton reply =
            QMessageBox::question(nullptr,
                                  "Change Trading Mode",
                                  QString("Switch from %1 to %2 mode?\n\n"
                                          "This will restart the application to connect to the %3 API.\n\n"
                                          "%4")
                                      .arg(currentModeStr,
                                           newModeStr,
                                           newModeStr,
                                           newMode == TradingMode::Live ? "⚠️ WARNING: LIVE mode uses REAL MONEY!"
                                                                        : "SIM mode uses simulated/paper trading."),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No);

        if (reply == QMessageBox::Yes)
        {
            // Save the new mode
            MainApp::setTradingMode(newMode);

            qCInfo(GUIFrontendLog) << "Trading mode changed to" << newModeStr << "- restarting application";

            // Restart the application using execv() - this replaces the current process
            MainApp::restartApplication();
        }

        return true; // Event handled
    }

    // Handle click on data source label (toggle LIVE/REPLAY)
    if (p_watched == m_dataSourceLabel && p_event->type() == QEvent::MouseButtonRelease)
    {
        if (MainApp::isInReplayMode())
        {
            // Currently in replay mode - exit replay
            MainApp::getInstance()->exitReplayMode();
        }
        else
        {
            // Currently in live mode - enter replay mode (without starting playback)
            // Playback starts when user clicks Play button in ChartToolbar
            ChartToolbar* toolbar = ui->priceChart->toolbar();

            // Scan for available replay days if not already populated
            toolbar->scanAndPopulateReplayDays();

            if (!toolbar->getSelectedReplayDay().isValid())
            {
                QMessageBox::warning(nullptr,
                                     "No Replay Data",
                                     "No recorded data found for replay.\n\n"
                                     "Use the Recorder tab to record market data first.");
                return true;
            }

            QDate replayDate = toolbar->getSelectedReplayDay();
            QTime replayTime = toolbar->getReplayStartTime();
            ReplayEngine::PlaybackSpeed speed = toolbar->getReplaySpeed();

            qCInfo(GUIFrontendLog) << "Entering replay mode for" << replayDate << "at" << replayTime;
            MainApp::getInstance()->enterReplayMode(replayDate, replayTime, speed);
        }

        return true; // Event handled
    }

    return QObject::eventFilter(p_watched, p_event);
}
