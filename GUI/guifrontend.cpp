#include "guifrontend.h"
#include "ui_guifrontend.h"
#include <QJsonDocument>

GuiFrontend::GuiFrontend(QObject* parent) : AppFrontend(parent) {
    ui = new Ui::GuiFrontend();
    ui->setupUi(new QMainWindow());
    //ui->centralwidget->setParent(this);
    static_cast<QMainWindow*>(ui->centralwidget->parent())->show();
}

GuiFrontend::~GuiFrontend() {
    delete ui;
}

void GuiFrontend::onPriceUpdated(const QJsonObject& priceData) {
    ui->logDisplay->append(QString("Price Updated: %1").arg(QString(QJsonDocument(priceData).toJson(QJsonDocument::Compact))));

    ui->priceChart->setSymbol("AAPL"); // TODO this is static temporarily

    double price = priceData["Last"].toString().toDouble();  // Convert string to double
    QDateTime timestamp = QDateTime::fromString(priceData["Time"].toString(), Qt::ISODate);

    if (timestamp.isValid()) {
        ui->priceChart->addPrice(price, timestamp);
    } else {
        ui->logDisplay->append("Cant add a point to the chart, the date is fucked.");
    }
}

void GuiFrontend::onPricesFetched() {
    ui->logDisplay->append("Prices Fetched - TODO act on prices");
}
