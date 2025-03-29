#include "guifrontend.h"
#include "ui_guifrontend.h"
#include <QJsonDocument>


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
    connect(this, &AppFrontend::tradeStationAuthStateChanged,
            this, &GuiFrontend::onTradeStationAuthStateChanged);
    connect(this, &AppFrontend::tradeStationAuthError,
            this, &GuiFrontend::onTradeStationAuthError);

    QObject::connect(&updateTimer, &QTimer::timeout, this, &GuiFrontend::onUpdateTimerTimeout);
    connect(this, &AppFrontend::fmpDataUsageUpdated,
            this, &GuiFrontend::onFMPClientDataUsageUpdate);

    // TODO test
    QObject::connect(FMPClient::getInstancePtr(), &FMPClient::quoteShortReceived, this, &GuiFrontend::onQuoteShortReceived);


    //TODO test
    updateTimer.start(1000);
}

GuiFrontend::~GuiFrontend() {
    delete ui;
}

QString bytesToString(qint64 bytes) {
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

void GuiFrontend::onFMPClientDataUsageUpdate(qsizetype newDataUsage)
{
    FMPDataUsage = newDataUsage;

    QString usageFMP = bytesToString(newDataUsage);
    QString usageMemory = bytesToString(memoryUsage);


    ui->statusbar->showMessage("FMP usage : " + usageFMP + " - Memory usage : " + usageMemory);
}

void GuiFrontend::onMemoryUsageUpdate(qint64 newDataUsage)
{
    memoryUsage = newDataUsage;

    QString usageFMP = bytesToString(FMPDataUsage);
    QString usageMemory = bytesToString(newDataUsage);

    ui->statusbar->showMessage("FMP usage : " + usageFMP + " - Memory usage : " + usageMemory);
}

void GuiFrontend::onUpdateTimerTimeout()
{
    //TODO test
    FMPClient::getInstance().fetchAsyncQuoteShort("AAPL");
}

void GuiFrontend::onQuoteShortReceived(const FMPClient::QuoteShortResult quoteResult)
{
    ui->logDisplay->append(QString("Price Updated: %1").arg(quoteResult.price));

    ui->priceChart->setSymbol(quoteResult.symbol);

    QDateTime timestamp = QDateTime::currentDateTime();

    if (timestamp.isValid()) {
        ui->priceChart->addPrice(quoteResult.price, timestamp);
    } else {
        ui->logDisplay->append("Cant add a point to the chart, the date is fucked.");
    }
}

void GuiFrontend::onTradeStationLoginClicked() {
    // AuthWindow is modal, so it's impossible to click the button while authentication is in progress
    Q_ASSERT(!TradeStationClient::getInstance().isAuthInProgress());
    TradeStationClient::getInstance().launchAuthProcess(static_cast<QMainWindow*>(ui->centralwidget->parent()));
}

void GuiFrontend::onTradeStationAuthStateChanged(bool isAuthenticated) {
    if (isAuthenticated) {
        tradeStationLoginButton->setText("TradeStation Connected");
        tradeStationLoginButton->setStyleSheet("QPushButton { background-color: #E6FFE6; color: #4CAF50; padding: 2px 6px; border-radius: 3px; }");
    } else {
        tradeStationLoginButton->setText("Login to TradeStation");
        tradeStationLoginButton->setStyleSheet("QPushButton { background-color: #00A0E9; color: #ffffff; padding: 2px 6px; border-radius: 3px; }");
    }
}

void GuiFrontend::onTradeStationAuthError(const QString& error) {
    tradeStationLoginButton->setText("Login Failed: " + error);
    tradeStationLoginButton->setStyleSheet("QPushButton { background-color: #FFE6E6; color: #f44336; padding: 2px 6px; border-radius: 3px; }");
}
