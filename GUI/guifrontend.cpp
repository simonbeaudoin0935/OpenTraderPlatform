#include "guifrontend.h"
#include "ui_guifrontend.h"
#include <QJsonDocument>

#include <fmpclient.h>

GuiFrontend::GuiFrontend(QObject* parent) : AppFrontend(parent) {
    ui = new Ui::GuiFrontend();
    ui->setupUi(new QMainWindow());
    //ui->centralwidget->setParent(this);
    static_cast<QMainWindow*>(ui->centralwidget->parent())->show();

    QObject::connect(&updateTimer, &QTimer::timeout, this, &GuiFrontend::onUpdateTimerTimeout);
    QObject::connect(FMPClient::getInstancePtr(), &FMPClient::totalDataReceivedBytesIncreased, this, &GuiFrontend::onFMPClientDataUsageUpdate);
    QObject::connect(FMPClient::getInstancePtr(), &FMPClient::quoteShortReceived, this, &GuiFrontend::onQuoteShortReceived);


    //TODO test
    updateTimer.start(1000);
}

GuiFrontend::~GuiFrontend() {
    delete ui;
}

void GuiFrontend::onPriceUpdated(const QJsonObject& priceData) {

}

void GuiFrontend::onPricesFetched() {
    ui->logDisplay->append("Prices Fetched - TODO act on prices");
}

void GuiFrontend::onFMPClientDataUsageUpdate(qsizetype newDataUsage)
{
    QString size;

    if (newDataUsage >= 1024 * 1024) {
        double megabytes = static_cast<double>(newDataUsage) / (1024 * 1024);
        size = QString("%1 MB").arg(megabytes, 0, 'f', 2);
    } else if (newDataUsage >= 1024) {
        double kilobytes = static_cast<double>(newDataUsage) / 1024;
        size = QString("%1 KB").arg(kilobytes, 0, 'f', 2);
    } else {
        size = QString("%1 bytes").arg(newDataUsage);
    }

    ui->statusbar->showMessage("FMP usage : " + size);
}

void GuiFrontend::onUpdateTimerTimeout()
{
    //TODO test
    FMPClient::getInstance().fetchAsyncQuoteShort("BTCUSD");
}

void GuiFrontend::onQuoteShortReceived(const QString symbol, double price, double change, qsizetype volume)
{
    ui->logDisplay->append(QString("Price Updated: %1").arg(price));

    ui->priceChart->setSymbol(symbol);

    QDateTime timestamp = QDateTime::currentDateTime();

    if (timestamp.isValid()) {
        ui->priceChart->addPrice(price, timestamp);
    } else {
        ui->logDisplay->append("Cant add a point to the chart, the date is fucked.");
    }
}
