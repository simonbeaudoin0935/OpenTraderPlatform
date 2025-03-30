#include "GuiFrontend.h"
#include "ui_guifrontend.h"
#include <QJsonDocument>


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

GuiFrontend::GuiFrontend(QObject* parent) : AppFrontend(parent) {
    ui = new Ui::GuiFrontend();
    ui->setupUi(new QMainWindow());
    static_cast<QMainWindow*>(ui->centralwidget->parent())->show();

    // Create and setup TradeStation login button
    tradeStationLoginButton = new QPushButton("Login to TradeStation", ui->statusbar);
    tradeStationLoginButton->setFlat(true);  // Make it look like a status bar item
    tradeStationLoginButton->setStyleSheet("QPushButton { background-color: #00A0E9; color: #ffffff; padding: 2px 6px; border-radius: 3px; }");
    ui->statusbar->addPermanentWidget(tradeStationLoginButton);

    // Connect TradeStation signals and slots
    connect(tradeStationLoginButton, &QPushButton::clicked, this, &GuiFrontend::onTradeStationLoginClicked);

    // Connect app frontend signals and slots
    connect(this, &AppFrontend::tradeStationAuthStateChanged,
            this, &GuiFrontend::onTradeStationAuthStateChanged);

    connect(this, &AppFrontend::tradeStationAccountsReceived,
            this, &GuiFrontend::onTradeStationAccountsReceived);



    connect(this, &AppFrontend::fmpDataUsageUpdated,
            this, &GuiFrontend::onFMPClientDataUsageUpdate);

    connect(this, &AppFrontend::tradeStationDataUsageUpdated,
            this, &GuiFrontend::onTradeStationClientDataUsageUpdate);

    // TODO disconnect this and pass through the frontend
    QObject::connect(FMPClient::getInstancePtr(), &FMPClient::quoteShortReceived, this, &GuiFrontend::onQuoteShortReceived);


    //TODO test
    QObject::connect(&updateTimer, &QTimer::timeout, this, &GuiFrontend::onUpdateTimerTimeout);
    updateTimer.start(1000);
}

GuiFrontend::~GuiFrontend() {
    delete ui;
}

void GuiFrontend::onFMPClientDataUsageUpdate(qsizetype newDataUsage)
{
    FMPClientDataUsage = newDataUsage;

    QString usageFMP = bytesToString(newDataUsage);
    QString usageTS  = bytesToString(TradeStationClientDataUsage);
    QString usageMemory = bytesToString(memoryUsage);


    ui->statusbar->showMessage("FMP usage : " + usageFMP + " - TS usage : " + usageTS + " - Memory usage : " + usageMemory);
}

void GuiFrontend::onTradeStationClientDataUsageUpdate(qsizetype newDataUsage)
{
    TradeStationClientDataUsage = newDataUsage;

    QString usageFMP = bytesToString(FMPClientDataUsage);
    QString usageTS  = bytesToString(newDataUsage);
    QString usageMemory = bytesToString(memoryUsage);

    ui->statusbar->showMessage("FMP usage : " + usageFMP + " - TS usage : " + usageTS + " - Memory usage : " + usageMemory);
}

void GuiFrontend::onTradeStationAccountsReceived(QVector<AccountResult> results)
{
    for (const AccountResult& account : results) {
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
    QString usageTS  = bytesToString(TradeStationClientDataUsage);
    QString usageMemory = bytesToString(newDataUsage);

    ui->statusbar->showMessage("FMP usage : " + usageFMP + " - TS usage : " + usageTS + " - Memory usage : " + usageMemory);
}

void GuiFrontend::onUpdateTimerTimeout()
{
    //TODO test
    FMPClient::getInstance().fetchAsyncQuoteShort("AAPL");
}

void GuiFrontend::onQuoteShortReceived(const FMPClient::QuoteShortResult quoteResult)
{
    //ui->logDisplay->append(QString("Price Updated: %1").arg(quoteResult.price));

    ui->priceChart->setSymbol(quoteResult.symbol);

    QDateTime timestamp = QDateTime::currentDateTime();

    if (timestamp.isValid()) {
        ui->priceChart->addPrice(quoteResult.price, timestamp);
    } else {
        ui->logDisplay->append("Cant add a point to the chart, the date is fucked.");
    }
}

void GuiFrontend::onTradeStationLoginClicked() {
#warning rework this, or at least better document that its this thread executing it. There is a race for sure with the TSClient internal flags
    // AuthWindow is modal, so it's impossible to click the button while authentication is in progress
    Q_ASSERT(!TradeStationClient::getInstance().isAuthInProgress());
    TradeStationClient::getInstance().launchAuthProcess(static_cast<QMainWindow*>(ui->centralwidget->parent()));
}

void GuiFrontend::onTradeStationAuthStateChanged(bool isAuthenticated, QString reason) {
    static bool isFirstTime = true;
    QString log;


    if (isAuthenticated) {
        tradeStationLoginButton->setText("TradeStation Connected");
        tradeStationLoginButton->setStyleSheet("QPushButton { background-color: #E6FFE6; color: #4CAF50; padding: 2px 6px; border-radius: 3px; }");
        log += "TradeStation Client AUTHENTICATED : " + reason;

        TradeStationClient::getInstance().fetchAsyncAccounts();

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
