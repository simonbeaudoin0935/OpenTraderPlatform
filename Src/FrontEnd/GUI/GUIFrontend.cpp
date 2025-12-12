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

#include "TSClient.h"
#include "GUIFrontend.h"
#include "ui_GUIFrontend.h"
#include "Tabs/LoggingTab.h"
#include "Tabs/CacheTab.h"
#include "Tabs/RecorderTab.h"
#include "Misc/Logging.h"
#include "Misc/Settings.h"

GUIFrontend::GUIFrontend(MainAlgo *mainAlgo, QObject* parent) :
    FrontEnd(parent),
    mainAlgo(mainAlgo)
{
    ui = new Ui::GUIFrontend();
    ui->setupUi(new QMainWindow());
    
    QMainWindow* mainWindow = static_cast<QMainWindow*>(ui->centralwidget->parent());
    
    setupDarkTheme(mainWindow);
    
    mainWindow->showMaximized();

    // Add Ctrl+Q shortcut to quit the application
    QShortcut *quitShortcut = new QShortcut(QKeySequence("Ctrl+Q"), mainWindow);
    connect(quitShortcut, &QShortcut::activated, qApp, &QApplication::quit);

    // Add "i" shortcut to focus the stock symbol input box
    QShortcut *focusShortcut = new QShortcut(QKeySequence("i"), mainWindow);
    connect(focusShortcut, &QShortcut::activated, [this]() { ui->stockSymbolInput->clear(); ui->stockSymbolInput->setFocus(); });

    // Create and setup TradeStation login button
    tradeStationLoginButton = new QPushButton("Login to TradeStation", ui->statusbar);
    tradeStationLoginButton->setFlat(true);  // Make it look like a status bar item
    tradeStationLoginButton->setStyleSheet("QPushButton { background-color: #00A0E9; color: #ffffff; padding: 2px 6px; border-radius: 3px; }");
    ui->statusbar->addPermanentWidget(tradeStationLoginButton);

    // Connect TradeStation login button click to launch auth process
    connect(tradeStationLoginButton, &QPushButton::clicked,
        this, []() {
            // AuthWindow is modal, so it's impossible to click the button while authentication is in progress
            Q_ASSERT(TSClient::getInstance()->isAuthInProgress() == false);
            TSClient::getInstance()->launchAuthProcess();
        });

    // Connect app frontend signals and slots
    connect(this, &FrontEnd::tradeStationAuthStateChanged,
            this, &GUIFrontend::onTradeStationAuthStateChanged,
            Qt::DirectConnection);

    connect(this, &FrontEnd::tradeStationAccountsReceived,
            this, &GUIFrontend::onTradeStationAccountsReceived,
            Qt::DirectConnection);

    connect(this, &FrontEnd::tradeStationDataUsageUpdated,
            this, &GUIFrontend::onTSClientDataUsageUpdate,
            Qt::DirectConnection);

    connect(this, &FrontEnd::streamCountUpdated,
            this, &GUIFrontend::onStreamCountUpdate,
            Qt::DirectConnection);

    connect(this, &FrontEnd::currentHighlightedStockBarReceived,
            this, &GUIFrontend::onCurrentHighlightedStockBarReceived,
            Qt::DirectConnection);

    connect(this, &FrontEnd::currentHighlightedReceivedNewMarketDepthQuote,
            this, &GUIFrontend::onCurrentHighlightedReceivedNewMarketDepthQuote,
            Qt::DirectConnection);

    connect(this, &FrontEnd::newPositionReceived,
            this, &GUIFrontend::onNewPositionReceived,
            Qt::DirectConnection);

    connect(this, &FrontEnd::newOrderReceived,
            this, &GUIFrontend::onNewOrderReceived,
            Qt::DirectConnection);

    connect(this, &FrontEnd::balanceUpdated,
            this, &GUIFrontend::onBalanceUpdated,
            Qt::DirectConnection);

    // When the chart requests missing bars, inside the lambda we call the main algo to get the bars from the displayed stock's bar cache
    // The result can be either immediate (QVector<Bar>) or asynchronous (QFuture<QVector<Bar>>)
    connect(ui->priceChart, &StockPriceChart::requestMissingBars,
            this, [this](QDateTime from, QDateTime to) mutable {
                
                BarCache::GetBarsResult_t result = MainAlgo::getInstance()->requestMissingBarsDisplayedStock(from, to);

                if (std::holds_alternative<QVector<Bar>>(result)) {
                    // The barCache had the bars ready immediately
                    ui->priceChart->onRequestedMissingBarsReceived(std::move(std::get<QVector<Bar>>(result)));

                } else {
                    QFuture<QVector<Bar>> future = std::move(std::get<QFuture<QVector<Bar>>>(result));

                    future.then(this, [this](const QVector<Bar>& bars){
                        ui->priceChart->onRequestedMissingBarsReceived(bars);
                    }).onFailed([](const TSClient::TimeoutException& e){
                        Q_UNUSED(e);
                        Q_ASSERT_X(false, "Get bars request timed out", "Get bars request timed out");
                    }).onFailed([](const TSClient::JSONErrorException& e){
                        Q_UNUSED(e);
                        Q_ASSERT_X(false, "Get bars request JSON error", "Get bars request JSON error");
                    }).onFailed([](const TSClient::OtherErrorException& e){
                        Q_UNUSED(e);
                        Q_ASSERT_X(false, "Get bars request other error", "Get bars request other error");
                    });
                }
            });

    // Connect the stock symbol input to its slot
    connect(ui->stockSymbolInput, &QLineEdit::returnPressed, this, &GUIFrontend::onNewDisplayedStockSelection);

    // Make the stock symbol input convert text to uppercase
    connect(ui->stockSymbolInput, &QLineEdit::textChanged, [this](const QString &text) {
        QString upper = text.toUpper();
        if (upper != text) {
            int pos = ui->stockSymbolInput->cursorPosition();
            ui->stockSymbolInput->blockSignals(true);
            ui->stockSymbolInput->setText(upper);
            ui->stockSymbolInput->setCursorPosition(pos);
            ui->stockSymbolInput->blockSignals(false);
        }
    });

    // Connect position window symbol click
    connect(ui->positionWindow, &PositionWindow::symbolClicked, this, [this](const QString& symbol) {
        ui->stockSymbolInput->setText(symbol);
        ui->stockSymbolInput->returnPressed();  // Simulate Enter key press
    });

    // Connect order window symbol click
    connect(ui->orderWindow, &OrderWindow::symbolClicked, this, [this](const QString& symbol) {
        ui->stockSymbolInput->setText(symbol);
        ui->stockSymbolInput->returnPressed();  // Simulate Enter key press
    });

    // Connect order entry widget
    auto c3 = connect(ui->orderEntryWidget, &OrderEntryWidget::orderPlaced,
                      this, &GUIFrontend::onOrderPlaced, Qt::UniqueConnection);
    Q_ASSERT(c3);

    // Set up the logging tab
    LoggingTab* loggingTab = new LoggingTab();
    ui->tabWidget->addTab(loggingTab, "Logging");

    // Connect logging tab signals
    connect(loggingTab, &LoggingTab::loggerVisibilityChanged,
            this, &GUIFrontend::onLoggerVisibilityChanged);
    connect(loggingTab, &LoggingTab::logDepthChanged,
            this, &GUIFrontend::onLogDepthChanged);

    // Set up the cache tab
    CacheTab* cacheTab = new CacheTab();
    ui->tabWidget->addTab(cacheTab, "Cache");

    // Set up the recorder tab
    RecorderTab* recorderTab = new RecorderTab();
    ui->tabWidget->addTab(recorderTab, "Recorder");

    // Set up the live log display at the bottom
    if (ui->liveLogDisplay) {
        QFont font("Monospace");
        font.setPointSize(9);
        ui->liveLogDisplay->setFont(font);
        
        // Connect to the log broadcaster
        connect(&LogBroadcaster::instance(), &LogBroadcaster::logMessageReceived,
                this, &GUIFrontend::updateLiveLogDisplay, Qt::QueuedConnection);
    }

    // NOTE: Don't restore the last displayed stock here - wait for authentication
    // It will be restored in onTradeStationAuthStateChanged() when authenticated
}

GUIFrontend::~GUIFrontend() {
    delete ui;
}

void GUIFrontend::setupDarkTheme(QMainWindow* mainWindow) {
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

    QString usageTS  = bytesToString(newDataUsage);
    QString usageMemory = bytesToString(memoryUsage);

    ui->statusbar->showMessage("TS usage : " + usageTS + " - Memory usage : " + usageMemory + " - Streams : " + QString::number(streamCount));
}

void GUIFrontend::onTradeStationAccountsReceived(QVector<Account> results)
{
    for (const Account& account : results) {
        ui->logDisplay->append("  ID:" + account.getAccountId());
        ui->logDisplay->append("  Type:" + AccountType::accountTypeToString(account.getAccountType().type));
        ui->logDisplay->append("  Status:" + account.getStatus());
        ui->logDisplay->append("  Currency:" + account.getCurrency());

        // Check AccountDetail if it exists
        const auto& detail = account.getAccountDetail();
        if (detail.has_value()) {
            ui->logDisplay->append("  Account Detail:");
            ui->logDisplay->append("    Stock Locate Eligible:" + QString::number(detail->isStockLocateEligible));
            ui->logDisplay->append("    Enrolled in RegT Program:" + QString::number(detail->enrolledInRegTProgram));
            ui->logDisplay->append("    Requires Buying Power Warning:" + QString::number(detail->requiresBuyingPowerWarning));
            ui->logDisplay->append("    Day Trading Qualified:" + QString::number(detail->dayTradingQualified));
            ui->logDisplay->append("    Option Approval Level:" + QString::number(detail->optionApprovalLevel));
            ui->logDisplay->append("    Pattern Day Trader:" + QString::number(detail->patternDayTrader));
        } else {
            ui->logDisplay->append("  No Account Detail available");
        }
    }
    
    // Pass accounts to order entry widget
    ui->orderEntryWidget->setAccounts(results);
}

void GUIFrontend::onMemoryUsageUpdate(qsizetype newDataUsage)
{
    memoryUsage = newDataUsage;

    QString usageTS  = bytesToString(TSClientDataUsage);
    QString usageMemory = bytesToString(newDataUsage);

    ui->statusbar->showMessage("TS usage : " + usageTS + " - Memory usage : " + usageMemory + " - Streams : " + QString::number(streamCount));
}

void GUIFrontend::onStreamCountUpdate(int count)
{
    streamCount = count;

    QString usageTS  = bytesToString(TSClientDataUsage);
    QString usageMemory = bytesToString(memoryUsage);

    ui->statusbar->showMessage("TS usage : " + usageTS + " - Memory usage : " + usageMemory + " - Streams : " + QString::number(count));
}

void GUIFrontend::onCurrentHighlightedStockBarReceived(QString symbol, Bar bar)
{
    ui->priceChart->setSymbol(symbol);
    ui->priceChart->addBar(bar);
}

void GUIFrontend::onCurrentHighlightedReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP)
{
    Q_UNUSED(symbol);

    ui->marketDepthTable->updateData(quote.getBids(), quote.getAsks());
    ui->marketDepthTable->updateDWP(bidDWP, askDWP);

    // Update the BAI gauge with the bid-ask imbalance
    ui->baiGauge->setValue(bidAskImbalance);
}

void GUIFrontend::onNewPositionReceived(QString account, Position position) {
    ui->positionWindow->updatePosition(account, position);
}

void GUIFrontend::onNewOrderReceived(QString account, Order order) {
    ui->orderWindow->updateOrder(account, order);
}

void GUIFrontend::onBalanceUpdated(Balance balance) {
    ui->balanceWindow->updateBalance(balance);
}

void GUIFrontend::onTradeStationAuthStateChanged(bool isAuthenticated, QString reason) {
    static bool isFirstTime = true;
    QString log;

    if (isAuthenticated) {
        tradeStationLoginButton->setText("TradeStation Connected");
        tradeStationLoginButton->setStyleSheet("QPushButton { background-color: #E6FFE6; color: #4CAF50; padding: 2px 6px; border-radius: 3px; }");
        log += "TradeStation Client AUTHENTICATED : " + reason;
        
        // Restore the last displayed stock now that we're authenticated
        // Only do this once on the first successful authentication
        if (!m_hasRestoredLastStock) {
            m_hasRestoredLastStock = true;
            restoreLastDisplayedStock();
        }
    } else {
        if (isFirstTime) {
            // If its the first time we receive this signal and its negative state, it just
            // means that at startup we are not authenticated, not that there was an error.
            // Present the normal blue button to login
            tradeStationLoginButton->setText("Login to TradeStation");
            tradeStationLoginButton->setStyleSheet("QPushButton { background-color: #00A0E9; color: #ffffff; padding: 2px 6px; border-radius: 3px; }");
        } else {
            tradeStationLoginButton->setText("Login Failed: " + reason);
            tradeStationLoginButton->setStyleSheet("QPushButton { background-color: #FFE6E6; color: #f44336; padding: 2px 6px; border-radius: 3px; }");
            log += "TradeStation Client UN-AUTHENTICATED : " + reason;
        }
    }

    ui->logDisplay->append(log);
    isFirstTime = false;
}

QString GUIFrontend::bytesToString(qint64 bytes) {
    if (bytes >= 1024 * 1024) {
        double megabytes = static_cast<double>(bytes) / (1024 * 1024);
        return QString("%1 MB").arg(megabytes, 0, 'f', 2);
    } else if (bytes >= 1024) {
        double kilobytes = static_cast<double>(bytes) / 1024;
        return QString("%1 KB").arg(kilobytes, 0, 'f', 2);
    } else {
        return QString("%1 bytes").arg(bytes);
    }
}

bool GUIFrontend::isValidStockSymbol(const QString& symbol) const {
    // Check if symbol is empty
    if (symbol.isEmpty()) {
        return false;
    }

    // Check for leading or trailing whitespace
    if (symbol != symbol.trimmed()) {
        return false;
    }

    // Check length (typical stock symbols are 1-10 characters)
    if (symbol.length() > 10 || symbol.length() < 1) {
        return false;
    }

    // Check for valid characters: alphanumeric, dots, hyphens, slashes
    QRegularExpression validSymbolRegex("^[A-Z0-9.\\-/]+$");
    if (!validSymbolRegex.match(symbol).hasMatch()) {
        return false;
    }

    return true;
}

void GUIFrontend::onNewDisplayedStockSelection()
{
    QString symbol = ui->stockSymbolInput->text().toUpper();

    // Validate the stock symbol
    if (!isValidStockSymbol(symbol)) {
        QMessageBox::warning(nullptr, "Invalid Symbol", 
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

void GUIFrontend::displayStock(const QString& symbol) {
    if (symbol == currentlyDisplayedSymbol) {
        qWarning() << "Symbol " << symbol << " is already the currently displayed symbol";
        return;
    }

    currentlyDisplayedSymbol = symbol;

    ui->priceChart->clearSymbol();

    // Update the order entry widget with the new symbol
    ui->orderEntryWidget->setSymbol(symbol);

    QMetaObject::invokeMethod(mainAlgo,
                              "onSelectDisplayedStock",
                              Qt::QueuedConnection,
                              Q_ARG(QString, symbol)); // Pass the symbol parameter
}

void GUIFrontend::updateLiveLogDisplay(const QString& message) {
    if (!ui->liveLogDisplay) {
        return;
    }

    // Check if the user is currently at the bottom of the log
    bool wasAtBottom = ui->liveLogDisplay->verticalScrollBar()->value() == ui->liveLogDisplay->verticalScrollBar()->maximum();

    ui->liveLogDisplay->append(message);

    // Enforce max log lines
    QTextDocument* doc = ui->liveLogDisplay->document();
    int lineCount = doc->lineCount();

    if (lineCount > maxLiveLogLines) {
        QTextCursor cursor(doc);
        cursor.movePosition(QTextCursor::Start);
        
        // Calculate how many lines to remove
        int linesToRemove = lineCount - maxLiveLogLines;
        
        // Select and delete the excess lines
        for (int i = 0; i < linesToRemove; ++i) {
            cursor.select(QTextCursor::LineUnderCursor);
            cursor.removeSelectedText();
            cursor.deleteChar(); // Remove the newline
        }
    }

    // Only auto-scroll to bottom if the user was already at the bottom
    if (wasAtBottom) {
        QTextCursor cursor = ui->liveLogDisplay->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui->liveLogDisplay->setTextCursor(cursor);
    }
}

void GUIFrontend::onLoggerVisibilityChanged(bool visible) {
    if (ui->liveLogDisplay) {
        ui->liveLogDisplay->setVisible(visible);
    }
}

void GUIFrontend::onLogDepthChanged(int maxLines) {
    maxLiveLogLines = maxLines;
    
    // Trim current log display if needed
    if (ui->liveLogDisplay) {
        QTextDocument* doc = ui->liveLogDisplay->document();
        int lineCount = doc->lineCount();
        
        if (lineCount > maxLiveLogLines) {
            QTextCursor cursor(doc);
            cursor.movePosition(QTextCursor::Start);
            
            int linesToRemove = lineCount - maxLiveLogLines;
            
            for (int i = 0; i < linesToRemove; ++i) {
                cursor.select(QTextCursor::LineUnderCursor);
                cursor.removeSelectedText();
                cursor.deleteChar();
            }
        }
    }
}

void GUIFrontend::saveLastDisplayedStock(const QString& symbol) {
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("GUI/LastDisplayedStock", symbol);
    appStateSettings->sync();
    qInfo() << "Saved last displayed stock:" << symbol;
}

void GUIFrontend::restoreLastDisplayedStock() {
    Q_CHECK_PTR(appStateSettings);
    QString lastSymbol = appStateSettings->value("GUI/LastDisplayedStock").toString().toUpper();
    
    if (lastSymbol.isEmpty()) {
        qInfo() << "No previously displayed stock to restore";
        return;
    }
    
    if (!isValidStockSymbol(lastSymbol)) {
        qWarning() << "Previously saved stock symbol is invalid:" << lastSymbol;
        return;
    }
    
    qInfo() << "Restoring last displayed stock:" << lastSymbol;
    
    // Set the symbol in the input box (uppercase)
    ui->stockSymbolInput->setText(lastSymbol);
    
    // Display the stock without saving again
    displayStock(lastSymbol);
}

void GUIFrontend::onOrderPlaced(const PlaceOrderRequest& order) {
    qInfo() << "Placing order:" << order.toJsonString();
    
    // Submit order to TSClient
    QFuture<PlaceOrderResult> future = TSClient::getInstance()->placeOrder(order);
    
    future.then(this, [this](const PlaceOrderResult& result) {
        if (result.hasErrors()) {
            QString errorMsg = "Order failed:\n";
            for (const auto& error : result.getErrors()) {
                errorMsg += error.getMessage() + "\n";
                if (error.getError().has_value()) {
                    errorMsg += "Error: " + error.getError().value() + "\n";
                }
            }
            QMessageBox::critical(nullptr, "Order Error", errorMsg);
            qCritical() << "Order placement failed:" << errorMsg;
        } else {
            QString successMsg = "Order(s) placed successfully:\n";
            for (const auto& orderItem : result.getOrders()) {
                successMsg += "Order ID: " + orderItem.getOrderID() + "\n";
                successMsg += orderItem.getMessage() + "\n";
            }
            QMessageBox::information(nullptr, "Order Success", successMsg);
            qInfo() << "Order placement successful:" << successMsg;
        }
    }).onFailed([](const TSClient::TimeoutException& e){
        Q_UNUSED(e);
        QMessageBox::critical(nullptr, "Order Error", "Order request timed out. Please try again.");
        qCritical() << "Order placement timed out";
    }).onFailed([](const TSClient::JSONErrorException& e){
        Q_UNUSED(e);
        QMessageBox::critical(nullptr, "Order Error", "Failed to parse order response from server.");
        qCritical() << "Order placement JSON error";
    }).onFailed([](const TSClient::OtherErrorException& e){
        Q_UNUSED(e);
        QMessageBox::critical(nullptr, "Order Error", "An error occurred while placing the order.");
        qCritical() << "Order placement error";
    });
}
