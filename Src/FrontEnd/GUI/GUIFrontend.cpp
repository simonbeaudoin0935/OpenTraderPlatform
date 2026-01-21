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
#include "Assume.h"

#include "TSClient.h"
#include "GUIFrontend.h"
#include "ui_GUIFrontend.h"
#include "Tabs/LoggingTab.h"
#include "Tabs/CacheTab.h"
#include "Tabs/RecorderTab.h"
#include "Tabs/ShortcutsTab.h"
#include "Misc/Logging.h"
#include "Misc/Settings.h"
#include "Misc/ShortcutSettings.h"
#include "Assume.h"

#define LOGGING_CATEGORY GUIFrontendLog

Q_LOGGING_CATEGORY(GUIFrontendLog, "GUIFrontend")

GUIFrontend::GUIFrontend(MainAlgo* p_mainAlgo, QObject* parent) : FrontEnd(parent), mainAlgo(p_mainAlgo)
{
    ui = std::make_unique<Ui::GUIFrontend>();
    ui->setupUi(new QMainWindow());

    this->setObjectName("GUIFrontend");

    QMainWindow* mainWindow = static_cast<QMainWindow*>(ui->centralwidget->parent());

    setupDarkTheme(mainWindow);

    mainWindow->showMaximized();

    // Initialize shortcuts from settings
    ShortcutSettings& shortcutSettings = ShortcutSettings::getInstance();

    // Add Ctrl+Q shortcut to quit the application
    m_quitShortcut = new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::QuitApplication), mainWindow);
    // Note: Qt::UniqueConnection may not work reliably with qApp global pointer
    auto quitConnection = connect(m_quitShortcut, &QShortcut::activated, qApp, &QApplication::quit);
    OBJ_ASSUME_TRUE(quitConnection);

    // Add "i" shortcut to focus the stock symbol input box
    m_focusShortcut = new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::FocusStockInput), mainWindow);
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
    m_buyShortcut = new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::ExecuteBuyOrder), mainWindow);
    auto buyConnection =
        connect(m_buyShortcut, &QShortcut::activated, [this]() { ui->orderEntryWidget->executeBuyOrder(); });
    OBJ_ASSUME_TRUE(buyConnection);

    // Add Ctrl+S shortcut to execute sell order
    m_sellShortcut = new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::ExecuteSellOrder), mainWindow);
    auto sellConnection =
        connect(m_sellShortcut, &QShortcut::activated, [this]() { ui->orderEntryWidget->executeSellOrder(); });
    OBJ_ASSUME_TRUE(sellConnection);

    // Add Ctrl+Shift+B shortcut to execute buy to cover order
    m_buyToCoverShortcut =
        new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::ExecuteBuyToCoverOrder), mainWindow);
    auto buyToCoverConnection = connect(m_buyToCoverShortcut,
                                        &QShortcut::activated,
                                        [this]() { ui->orderEntryWidget->executeBuyToCoverOrder(); });
    OBJ_ASSUME_TRUE(buyToCoverConnection);

    // Add Ctrl+Shift+S shortcut to execute sell to cover order
    m_sellToCoverShortcut =
        new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::ExecuteSellToCoverOrder), mainWindow);
    auto sellToCoverConnection = connect(m_sellToCoverShortcut,
                                         &QShortcut::activated,
                                         [this]() { ui->orderEntryWidget->executeSellToCoverOrder(); });
    OBJ_ASSUME_TRUE(sellToCoverConnection);

    // Add Ctrl+X shortcut to cancel all orders
    m_cancelAllOrdersShortcut =
        new QShortcut(shortcutSettings.getShortcut(ShortcutSettings::CancelAllOrders), mainWindow);
    auto cancelAllConnection =
        connect(m_cancelAllOrdersShortcut, &QShortcut::activated, [this]() { onCancelAllOrders(); });
    OBJ_ASSUME_TRUE(cancelAllConnection);

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

    connect(this, &FrontEnd::streamCountUpdated, this, &GUIFrontend::onStreamCountUpdate, Qt::DirectConnection);

    connect(this,
            &FrontEnd::currentHighlightedStockBarReceived,
            this,
            &GUIFrontend::onCurrentHighlightedStockBarReceived,
            Qt::DirectConnection);

    connect(this,
            &FrontEnd::currentHighlightedReceivedNewMarketDepthQuote,
            this,
            &GUIFrontend::onCurrentHighlightedReceivedNewMarketDepthQuote,
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

    // Set up the logging tab
    LoggingTab* loggingTab = new LoggingTab();
    ui->tabWidget->addTab(loggingTab, "Logging");

    // Connect logging tab signals
    connect(loggingTab, &LoggingTab::loggerVisibilityChanged, this, &GUIFrontend::onLoggerVisibilityChanged);
    connect(loggingTab, &LoggingTab::logDepthChanged, this, &GUIFrontend::onLogDepthChanged);

    // Set up the cache tab
    CacheTab* cacheTab = new CacheTab();
    ui->tabWidget->addTab(cacheTab, "Cache");

    // Set up the recorder tab
    RecorderTab* recorderTab = new RecorderTab();
    ui->tabWidget->addTab(recorderTab, "Recorder");

    // Set up the shortcuts tab
    ShortcutsTab* shortcutsTab = new ShortcutsTab();
    ui->tabWidget->addTab(shortcutsTab, "Shortcuts");

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
    // ui is automatically deleted by std::unique_ptr
}

void GUIFrontend::setupDarkTheme(QMainWindow* mainWindow)
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
    mainWindow->setPalette(darkPalette);
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

void GUIFrontend::onTSClientDataUsageUpdate(qsizetype newDataUsage)
{
    TSClientDataUsage = newDataUsage;

    QString usageTS = bytesToString(newDataUsage);
    QString usageMemory = bytesToString(memoryUsage);

    ui->statusbar->showMessage("TS usage : " + usageTS + " - Memory usage : " + usageMemory +
                               " - Streams : " + QString::number(streamCount));
}

void GUIFrontend::onTradeStationAccountsReceived(QVector<Account> results)
{
    m_accounts = results; // Store accounts

    for (const Account& account: results)
    {
        ui->logDisplay->append("  ID:" + account.getAccountId());
        ui->logDisplay->append("  Type:" + AccountType::accountTypeToString(account.getAccountType().type));
        ui->logDisplay->append("  Status:" + account.getStatus());
        ui->logDisplay->append("  Currency:" + account.getCurrency());

        // Check AccountDetail if it exists
        const auto& detail = account.getAccountDetail();
        if (detail.has_value())
        {
            ui->logDisplay->append("  Account Detail:");
            ui->logDisplay->append("    Stock Locate Eligible:" + QString::number(detail->isStockLocateEligible));
            ui->logDisplay->append("    Enrolled in RegT Program:" + QString::number(detail->enrolledInRegTProgram));
            ui->logDisplay->append("    Requires Buying Power Warning:" +
                                   QString::number(detail->requiresBuyingPowerWarning));
            ui->logDisplay->append("    Day Trading Qualified:" + QString::number(detail->dayTradingQualified));
            ui->logDisplay->append("    Option Approval Level:" + QString::number(detail->optionApprovalLevel));
            ui->logDisplay->append("    Pattern Day Trader:" + QString::number(detail->patternDayTrader));
        }
        else
        {
            ui->logDisplay->append("  No Account Detail available");
        }
    }

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

    QString usageTS = bytesToString(TSClientDataUsage);
    QString usageMemory = bytesToString(newDataUsage);

    ui->statusbar->showMessage("TS usage : " + usageTS + " - Memory usage : " + usageMemory +
                               " - Streams : " + QString::number(streamCount));
}

void GUIFrontend::onStreamCountUpdate(int count)
{
    streamCount = count;

    QString usageTS = bytesToString(TSClientDataUsage);
    QString usageMemory = bytesToString(memoryUsage);

    ui->statusbar->showMessage("TS usage : " + usageTS + " - Memory usage : " + usageMemory +
                               " - Streams : " + QString::number(count));
}

void GUIFrontend::onCurrentHighlightedStockBarReceived(QString symbol, Bar bar)
{
    ui->priceChart->addLiveBar(symbol, bar);
}

void GUIFrontend::onCurrentHighlightedReceivedNewMarketDepthQuote(QString symbol,
                                                                  MarketDepthQuote quote,
                                                                  double bidAskImbalance,
                                                                  double bidDWP,
                                                                  double askDWP)
{
    Q_UNUSED(symbol);

    ui->marketDepthTable->updateData(quote.getBids(), quote.getAsks());
    ui->marketDepthTable->updateDWP(bidDWP, askDWP);

    // Update the BAI gauge with the bid-ask imbalance
    ui->baiGauge->setValue(bidAskImbalance);
}

void GUIFrontend::onNewPositionReceived(QString account, Position position)
{
    ui->positionWindow->updatePosition(account, position);
}

void GUIFrontend::onPositionDeleted(QString account, QString positionID)
{
    ui->positionWindow->onPositionDeleted(account, positionID);
}

void GUIFrontend::onNewOrderReceived(QString account, Order order)
{
    ui->orderWindow->updateOrder(account, order);
}

void GUIFrontend::onBalanceUpdated(Balance balance)
{
    ui->balanceWindow->updateBalance(balance);
}

void GUIFrontend::onTradeStationAuthStateChanged(bool isAuthenticated, QString reason)
{
    static bool isFirstTime = true;
    QString log;

    if (isAuthenticated)
    {
        tradeStationLoginButton->setText("TradeStation Connected");
        tradeStationLoginButton->setStyleSheet(
            "QPushButton { background-color: #E6FFE6; color: #4CAF50; padding: 2px 6px; border-radius: 3px; }");
        log += "TradeStation Client AUTHENTICATED : " + reason;

        // Restore the last displayed stock now that we're authenticated
        // Only do this once on the first successful authentication
        if (!m_hasRestoredLastStock)
        {
            m_hasRestoredLastStock = true;
            restoreLastDisplayedStock();
        }
    }
    else
    {
        if (isFirstTime)
        {
            // If its the first time we receive this signal and its negative state, it just
            // means that at startup we are not authenticated, not that there was an error.
            // Present the normal blue button to login
            tradeStationLoginButton->setText("Login to TradeStation");
            tradeStationLoginButton->setStyleSheet(
                "QPushButton { background-color: #00A0E9; color: #ffffff; padding: 2px 6px; border-radius: 3px; }");
        }
        else
        {
            tradeStationLoginButton->setText("Login Failed: " + reason);
            tradeStationLoginButton->setStyleSheet(
                "QPushButton { background-color: #FFE6E6; color: #f44336; padding: 2px 6px; border-radius: 3px; }");
            log += "TradeStation Client UN-AUTHENTICATED : " + reason;
        }
    }

    ui->logDisplay->append(log);
    isFirstTime = false;
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
    }
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
        // The barCache had the bars ready immediately
        ui->priceChart->onRequestedMissingBarsReceived(std::get<std::shared_ptr<QVector<Bar>>>(result));
    }
    else if (std::holds_alternative<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(result))
    {
        std::get<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(result).then(
            this,
            [this, from, to](std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>&& bars)
            {
                if (bars.has_value())
                {
                    qInfo() << "Successfully retrieved missing bars from BarCache";
                    ui->priceChart->onRequestedMissingBarsReceived(bars.value());
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
