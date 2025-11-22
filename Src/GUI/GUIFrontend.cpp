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

#include "TSClient.h"
#include "GUIFrontend.h"
#include "ui_GUIFrontend.h"
#include "LoggingTab.h"
#include "CacheTab.h"
#include "Misc/Logging.h"

GUIFrontend::GUIFrontend(MainAlgo *mainAlgo, QObject* parent) :
    AppFrontend(parent),
    mainAlgo(mainAlgo)
{
    ui = new Ui::GUIFrontend();
    ui->setupUi(new QMainWindow());
    
    QMainWindow* mainWindow = static_cast<QMainWindow*>(ui->centralwidget->parent());
    
    setupDarkTheme(mainWindow);
    
    mainWindow->showMaximized();

    // Add Ctrl+Q shortcut to quit the application
    QShortcut *quitShortcut = new QShortcut(QKeySequence("Ctrl+Q"), mainWindow);
    bool connection1 = connect(quitShortcut, &QShortcut::activated, qApp, &QApplication::quit, Qt::UniqueConnection);
    Q_ASSERT_X(connection1, "GUIFrontend", "Failed to create unique connection for quit shortcut");

    // Add "i" shortcut to focus the stock symbol input box
    QShortcut *focusShortcut = new QShortcut(QKeySequence("i"), mainWindow);
    bool connection2 = connect(focusShortcut, &QShortcut::activated, [this]() { ui->stockSymbolInput->clear(); ui->stockSymbolInput->setFocus(); }, Qt::UniqueConnection);
    Q_ASSERT_X(connection2, "GUIFrontend", "Failed to create unique connection for focus shortcut");

    // Create and setup TradeStation login button
    tradeStationLoginButton = new QPushButton("Login to TradeStation", ui->statusbar);
    tradeStationLoginButton->setFlat(true);  // Make it look like a status bar item
    tradeStationLoginButton->setStyleSheet("QPushButton { background-color: #00A0E9; color: #ffffff; padding: 2px 6px; border-radius: 3px; }");
    ui->statusbar->addPermanentWidget(tradeStationLoginButton);

    // Connect TradeStation signals and slots
    bool connection3 = connect(tradeStationLoginButton, &QPushButton::clicked, this, &GUIFrontend::onTradeStationLoginClicked, Qt::UniqueConnection);
    Q_ASSERT_X(connection3, "GUIFrontend", "Failed to create unique connection for TradeStation login button");

    // Connect app frontend signals and slots
    bool connection4 = connect(this, &AppFrontend::tradeStationAuthStateChanged,
            this, &GUIFrontend::onTradeStationAuthStateChanged,
            Qt::DirectConnection | Qt::UniqueConnection);
    Q_ASSERT_X(connection4, "GUIFrontend", "Failed to create unique connection for tradeStationAuthStateChanged");

    bool connection5 = connect(this, &AppFrontend::tradeStationAccountsReceived,
            this, &GUIFrontend::onTradeStationAccountsReceived,
            Qt::DirectConnection | Qt::UniqueConnection);
    Q_ASSERT_X(connection5, "GUIFrontend", "Failed to create unique connection for tradeStationAccountsReceived");

    bool connection6 = connect(this, &AppFrontend::fmpDataUsageUpdated,
            this, &GUIFrontend::onFMPClientDataUsageUpdate,
            Qt::DirectConnection | Qt::UniqueConnection);
    Q_ASSERT_X(connection6, "GUIFrontend", "Failed to create unique connection for fmpDataUsageUpdated");

    bool connection7 = connect(this, &AppFrontend::tradeStationDataUsageUpdated,
            this, &GUIFrontend::onTSClientDataUsageUpdate,
            Qt::DirectConnection | Qt::UniqueConnection);
    Q_ASSERT_X(connection7, "GUIFrontend", "Failed to create unique connection for tradeStationDataUsageUpdated");

    bool connection8 = connect(this, &AppFrontend::streamCountUpdated,
            this, &GUIFrontend::onStreamCountUpdate,
            Qt::DirectConnection | Qt::UniqueConnection);
    Q_ASSERT_X(connection8, "GUIFrontend", "Failed to create unique connection for streamCountUpdated");

    bool connection9 = connect(this, &AppFrontend::currentHighlightedStockBarReceived,
            this, &GUIFrontend::onCurrentHighlightedStockBarReceived,
            Qt::DirectConnection | Qt::UniqueConnection);
    Q_ASSERT_X(connection9, "GUIFrontend", "Failed to create unique connection for currentHighlightedStockBarReceived");

    bool connection10 = connect(this, &AppFrontend::currentHighlightedReceivedNewMarketDepthQuote,
            this, &GUIFrontend::onCurrentHighlightedReceivedNewMarketDepthQuote,
            Qt::DirectConnection | Qt::UniqueConnection);
    Q_ASSERT_X(connection10, "GUIFrontend", "Failed to create unique connection for currentHighlightedReceivedNewMarketDepthQuote");

    bool connection11 = connect(this, &AppFrontend::newPositionReceived,
            this, &GUIFrontend::onNewPositionReceived,
            Qt::DirectConnection | Qt::UniqueConnection);
    Q_ASSERT_X(connection11, "GUIFrontend", "Failed to create unique connection for newPositionReceived");

    // Propagate up when the chart needs missing bars to display
    bool connection12 = connect(ui->priceChart, &StockPriceChart::requestMissingBars,
            this, &AppFrontend::requestMissingBars, Qt::UniqueConnection);
    Q_ASSERT_X(connection12, "GUIFrontend", "Failed to create unique connection for requestMissingBars");

    // Connect the stock symbol input to its slot
    bool connection13 = connect(ui->stockSymbolInput, &QLineEdit::returnPressed, this, &GUIFrontend::onNewDisplayedStockSelection, Qt::UniqueConnection);
    Q_ASSERT_X(connection13, "GUIFrontend", "Failed to create unique connection for stockSymbolInput returnPressed");

    // Make the stock symbol input convert text to uppercase
    bool connection14 = connect(ui->stockSymbolInput, &QLineEdit::textChanged, [this](const QString &text) {
        QString upper = text.toUpper();
        if (upper != text) {
            int pos = ui->stockSymbolInput->cursorPosition();
            ui->stockSymbolInput->blockSignals(true);
            ui->stockSymbolInput->setText(upper);
            ui->stockSymbolInput->setCursorPosition(pos);
            ui->stockSymbolInput->blockSignals(false);
        }
    }, Qt::UniqueConnection);
    Q_ASSERT_X(connection14, "GUIFrontend", "Failed to create unique connection for stockSymbolInput textChanged");

    // Connect position window symbol click
    bool connection15 = connect(ui->positionWindow, &PositionWindow::symbolClicked, this, [this](const QString& symbol) {
        ui->stockSymbolInput->setText(symbol);
        ui->stockSymbolInput->returnPressed();  // Simulate Enter key press
    }, Qt::UniqueConnection);
    Q_ASSERT_X(connection15, "GUIFrontend", "Failed to create unique connection for positionWindow symbolClicked");

    // Set up the logging tab
    LoggingTab* loggingTab = new LoggingTab();
    ui->tabWidget->addTab(loggingTab, "Logging");

    // Connect logging tab signals
    bool connection16 = connect(loggingTab, &LoggingTab::loggerVisibilityChanged,
            this, &GUIFrontend::onLoggerVisibilityChanged, Qt::UniqueConnection);
    Q_ASSERT_X(connection16, "GUIFrontend", "Failed to create unique connection for loggerVisibilityChanged");
    bool connection17 = connect(loggingTab, &LoggingTab::logDepthChanged,
            this, &GUIFrontend::onLogDepthChanged, Qt::UniqueConnection);
    Q_ASSERT_X(connection17, "GUIFrontend", "Failed to create unique connection for logDepthChanged");

    // Set up the cache tab
    CacheTab* cacheTab = new CacheTab();
    ui->tabWidget->addTab(cacheTab, "Cache");

    // Set up the live log display at the bottom
    if (ui->liveLogDisplay) {
        QFont font("Monospace");
        font.setPointSize(9);
        ui->liveLogDisplay->setFont(font);
        
        // Connect to the log broadcaster
        bool connection18 = connect(&LogBroadcaster::instance(), &LogBroadcaster::logMessageReceived,
                this, &GUIFrontend::updateLiveLogDisplay, Qt::QueuedConnection | Qt::UniqueConnection);
        Q_ASSERT_X(connection18, "GUIFrontend", "Failed to create unique connection for logMessageReceived");
    }
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

void GUIFrontend::onFMPClientDataUsageUpdate(qsizetype newDataUsage)
{
    FMPClientDataUsage = newDataUsage;

    QString usageFMP = bytesToString(newDataUsage);
    QString usageTS  = bytesToString(TSClientDataUsage);
    QString usageMemory = bytesToString(memoryUsage);

    ui->statusbar->showMessage("FMP usage : " + usageFMP + " - TS usage : " + usageTS + " - Memory usage : " + usageMemory + " - Streams : " + QString::number(streamCount));
}

void GUIFrontend::onTSClientDataUsageUpdate(qsizetype newDataUsage)
{
    TSClientDataUsage = newDataUsage;

    QString usageFMP = bytesToString(FMPClientDataUsage);
    QString usageTS  = bytesToString(newDataUsage);
    QString usageMemory = bytesToString(memoryUsage);

    ui->statusbar->showMessage("FMP usage : " + usageFMP + " - TS usage : " + usageTS + " - Memory usage : " + usageMemory + " - Streams : " + QString::number(streamCount));
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
}

void GUIFrontend::onMemoryUsageUpdate(qsizetype newDataUsage)
{
    memoryUsage = newDataUsage;

    QString usageFMP = bytesToString(FMPClientDataUsage);
    QString usageTS  = bytesToString(TSClientDataUsage);
    QString usageMemory = bytesToString(newDataUsage);

    ui->statusbar->showMessage("FMP usage : " + usageFMP + " - TS usage : " + usageTS + " - Memory usage : " + usageMemory + " - Streams : " + QString::number(streamCount));
}

void GUIFrontend::onStreamCountUpdate(int count)
{
    streamCount = count;

    QString usageFMP = bytesToString(FMPClientDataUsage);
    QString usageTS  = bytesToString(TSClientDataUsage);
    QString usageMemory = bytesToString(memoryUsage);

    ui->statusbar->showMessage("FMP usage : " + usageFMP + " - TS usage : " + usageTS + " - Memory usage : " + usageMemory + " - Streams : " + QString::number(count));
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

void GUIFrontend::onRequestedMissingBarsDisplayedStockReceived(QVector<Bar> bars)
{
    ui->priceChart->onRequestedMissingBarsReceived(bars);
}

void GUIFrontend::onTradeStationLoginClicked() {
    // AuthWindow is modal, so it's impossible to click the button while authentication is in progress
    Q_ASSERT(!TSClient::getInstance().isAuthInProgress());
    TSClient::getInstance().launchAuthProcess();
}

void GUIFrontend::onTradeStationAuthStateChanged(bool isAuthenticated, QString reason) {
    static bool isFirstTime = true;
    QString log;

    if (isAuthenticated) {
        tradeStationLoginButton->setText("TradeStation Connected");
        tradeStationLoginButton->setStyleSheet("QPushButton { background-color: #E6FFE6; color: #4CAF50; padding: 2px 6px; border-radius: 3px; }");
        log += "TradeStation Client AUTHENTICATED : " + reason;

        TSClient::getInstance().getAccountsAsync();
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

void GUIFrontend::onNewDisplayedStockSelection()
{
    QString symbol = ui->stockSymbolInput->text().toUpper();

    // Update the input field to show the uppercase symbol
    ui->stockSymbolInput->setText(symbol);

    if (symbol == currentlyDisplayedSymbol) {
        qWarning() << "Symbol " << symbol << " is already the currently displayed symbol";
        return;
    }

    currentlyDisplayedSymbol = symbol;

    ui->priceChart->clearSymbol();

    QMetaObject::invokeMethod(mainAlgo,
                              "onSelectDisplayedStock",
                              Qt::QueuedConnection,
                              Q_ARG(QString, symbol)); // Pass the symbol parameter

    // Clear focus from the input box after processing
    ui->stockSymbolInput->clearFocus();
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
