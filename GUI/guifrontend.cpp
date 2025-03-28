#include "guifrontend.h"
#include "ui_guifrontend.h"
#include <QJsonDocument>


GuiFrontend::GuiFrontend(QObject* parent) : AppFrontend(parent) {
    ui = new Ui::GuiFrontend();
    ui->setupUi(new QMainWindow());
    static_cast<QMainWindow*>(ui->centralwidget->parent())->show();

    QObject::connect(&updateTimer, &QTimer::timeout, this, &GuiFrontend::onUpdateTimerTimeout);
    QObject::connect(FMPClient::getInstancePtr(), &FMPClient::totalDataReceivedBytesIncreased, this, &GuiFrontend::onFMPClientDataUsageUpdate);

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
