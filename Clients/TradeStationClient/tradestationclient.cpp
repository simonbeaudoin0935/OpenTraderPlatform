#include "tradestationclient.h"
#include <QNetworkAccessManager>
#include <QThread>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QDebug>
#ifdef UNIT_TESTING
#include <QtTest>
#endif

const QString baseUrlTradeStation = "https://sim-api.tradestation.com/v3";

// Define the logging category
Q_LOGGING_CATEGORY(TradeStationClientLog, "TradeStationClient")

// Initialize static member outside class
TradeStationClient* TradeStationClient::instance = nullptr;

TradeStationClient& TradeStationClient::getInstance() {
    if (instance == nullptr) {
        qCDebug(TradeStationClientLog) << "Singleton instance created";
        instance = new TradeStationClient();
    }
    return *instance;
}

TradeStationClient* TradeStationClient::getInstancePtr() {
    if (instance == nullptr) {
        qCDebug(TradeStationClientLog) << "Singleton instance created";
        instance = new TradeStationClient();
    }
    return instance;
}

TradeStationClient::TradeStationClient() :
    RESTClient(baseUrlTradeStation)
{
    qCDebug(TradeStationClientLog) << Q_FUNC_INFO << ": TradeStationClient created using KEY=" << apiKey;

    thread->setObjectName("TradeStationClientThread");

    thread->start();
}

TradeStationClient::~TradeStationClient() {
    qCDebug(TradeStationClientLog) << "Singleton instance destroyed";

    thread->quit();
    thread->wait();
}


void TradeStationClient::emitSignalDemuxer(RequestTypeInt type, const QJsonArray &doc) {
    // TODO: Implement signal demuxing when we add specific request types
}

void TradeStationClient::showAuthWindow(QWidget* parent)
{
    if (!authWindow) {
        authWindow = new AuthWindow(parent);
        connect(authWindow, &AuthWindow::authenticationCompleted,
                this, &TradeStationClient::handleAuthCompleted);
        connect(authWindow, &AuthWindow::authenticationFailed,
                this, &TradeStationClient::handleAuthFailed);
        connect(authWindow, &QObject::destroyed,
                this, &TradeStationClient::handleAuthWindowDestroyed);
        authWindow->exec();
    }
}

void TradeStationClient::handleAuthCompleted(bool success)
{
    authenticated = success;
    emit authenticationStateChanged(authenticated);
}

void TradeStationClient::handleAuthFailed(const QString& error)
{
    authenticated = false;
    emit authenticationError(error);
}

void TradeStationClient::handleAuthWindowDestroyed()
{
    authWindow = nullptr;
}
