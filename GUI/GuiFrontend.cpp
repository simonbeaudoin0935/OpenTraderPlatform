#include "GuiFrontend.h"
#include "ui_guifrontend.h"
#include <QJsonDocument>
#include <QHeaderView>
#include <QLabel>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPalette>
#include <QApplication>

GuiFrontend::GuiFrontend(QObject* parent) : AppFrontend(parent) {
    ui = new Ui::GuiFrontend();
    ui->setupUi(new QMainWindow());
    
    // Setup dark theme for the entire application
    QMainWindow* mainWindow = static_cast<QMainWindow*>(ui->centralwidget->parent());
    
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
    mainWindow->show();

    // Create and setup TradeStation login button
    tradeStationLoginButton = new QPushButton("Login to TradeStation", ui->statusbar);
    tradeStationLoginButton->setFlat(true);  // Make it look like a status bar item
    tradeStationLoginButton->setStyleSheet("QPushButton { background-color: #00A0E9; color: #ffffff; padding: 2px 6px; border-radius: 3px; }");
    ui->statusbar->addPermanentWidget(tradeStationLoginButton);

    // Connect TradeStation signals and slots
    connect(tradeStationLoginButton, &QPushButton::clicked, this, &GuiFrontend::onTradeStationLoginClicked);

    // Connect app frontend signals and slots
    connect(this, &AppFrontend::tradeStationAuthStateChanged,
            this, &GuiFrontend::onTradeStationAuthStateChanged,
            Qt::DirectConnection);

    connect(this, &AppFrontend::tradeStationAccountsReceived,
            this, &GuiFrontend::onTradeStationAccountsReceived,
            Qt::DirectConnection);

    connect(this, &AppFrontend::fmpDataUsageUpdated,
            this, &GuiFrontend::onFMPClientDataUsageUpdate,
            Qt::DirectConnection);

    connect(this, &AppFrontend::tradeStationDataUsageUpdated,
            this, &GuiFrontend::onTSClientDataUsageUpdate,
            Qt::DirectConnection);

    connect(this, &AppFrontend::marketDepthNotAvailable,
            this, &GuiFrontend::onMarketDepthNotAvailable,
            Qt::DirectConnection);

    connect(this, &AppFrontend::currentHighlightedStockBarReceived,
            this, &GuiFrontend::onCurrentHighlightedStockBarReceived,
            Qt::DirectConnection);

    connect(this, &AppFrontend::currentHighlightedReceivedNewMarketDepthQuote,
            this, &GuiFrontend::onCurrentHighlightedReceivedNewMarketDepthQuote,
            Qt::DirectConnection);

    connect(this, &AppFrontend::onNewPositionReceived,
            this, &GuiFrontend::onNewPositionReceived,
            Qt::DirectConnection);
}

GuiFrontend::~GuiFrontend() {
    delete ui;
}

void GuiFrontend::onFMPClientDataUsageUpdate(qsizetype newDataUsage)
{
    FMPClientDataUsage = newDataUsage;

    QString usageFMP = bytesToString(newDataUsage);
    QString usageTS  = bytesToString(TSClientDataUsage);
    QString usageMemory = bytesToString(memoryUsage);

    ui->statusbar->showMessage("FMP usage : " + usageFMP + " - TS usage : " + usageTS + " - Memory usage : " + usageMemory);
}

void GuiFrontend::onTSClientDataUsageUpdate(qsizetype newDataUsage)
{
    TSClientDataUsage = newDataUsage;

    QString usageFMP = bytesToString(FMPClientDataUsage);
    QString usageTS  = bytesToString(newDataUsage);
    QString usageMemory = bytesToString(memoryUsage);

    ui->statusbar->showMessage("FMP usage : " + usageFMP + " - TS usage : " + usageTS + " - Memory usage : " + usageMemory);
}

void GuiFrontend::onTradeStationAccountsReceived(QVector<Account> results)
{
    for (const Account& account : results) {
        ui->logDisplay->append("  ID:" + account.getAccountId());
        ui->logDisplay->append("  Type:" + account.getAccountType());
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

void GuiFrontend::onMemoryUsageUpdate(qint64 newDataUsage)
{
    memoryUsage = newDataUsage;

    QString usageFMP = bytesToString(FMPClientDataUsage);
    QString usageTS  = bytesToString(TSClientDataUsage);
    QString usageMemory = bytesToString(newDataUsage);

    ui->statusbar->showMessage("FMP usage : " + usageFMP + " - TS usage : " + usageTS + " - Memory usage : " + usageMemory);
}

void GuiFrontend::onMarketDepthNotAvailable()
{
    QMessageBox::critical(ui->centralwidget, "Missing Level 2 data",
                          "Received error 403 when accessing Level 2.\n"
                          "This means Level 2 data is not activated on the account.\n"
                          "You need to go to :\n"
                          "https://clientcenter.tradestation.com/support/myaccount/change_data.aspx\n"
                          "And subscribe to NASDAQ Real-Time Data Package #3.\n"
                          "And by extention the Enhanced Market Depth package.");
}

void GuiFrontend::onCurrentHighlightedStockBarReceived(QString symbol, Bar bar)
{
    ui->priceChart->setSymbol(symbol);
    ui->priceChart->addBar(bar);
}

void GuiFrontend::onCurrentHighlightedReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote, double bidAskImbalance)
{
    ui->marketDepthTable->updateData(quote.getBids(), quote.getAsks(), bidAskImbalance);
    
    // Update the RAI gauge with the bid-ask imbalance
    ui->raiGauge->setValue(bidAskImbalance);
}

void GuiFrontend::onNewPositionReceived(QString account, Position position) {
    ui->positionWindow->updatePosition(account, position);
}

void GuiFrontend::onQuoteShortReceived(const FMPClient::QuoteShortResult quoteResult)
{
    qCritical() << "UNUSED";
}

void GuiFrontend::onTradeStationLoginClicked() {
#warning rework this, or at least better document that its this thread executing it. There is a race for sure with the TSClient internal flags
    // AuthWindow is modal, so it's impossible to click the button while authentication is in progress
    Q_ASSERT(!TSClient::getInstance().isAuthInProgress());
    TSClient::getInstance().launchAuthProcess(static_cast<QMainWindow*>(ui->centralwidget->parent()));
}

void GuiFrontend::onTradeStationAuthStateChanged(bool isAuthenticated, QString reason) {
    static bool isFirstTime = true;
    QString log;

    if (isAuthenticated) {
        tradeStationLoginButton->setText("TradeStation Connected");
        tradeStationLoginButton->setStyleSheet("QPushButton { background-color: #E6FFE6; color: #4CAF50; padding: 2px 6px; border-radius: 3px; }");
        log += "TradeStation Client AUTHENTICATED : " + reason;

        TSClient::getInstance().fetchAsyncAccounts();
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

QString GuiFrontend::bytesToString(qint64 bytes) {
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
