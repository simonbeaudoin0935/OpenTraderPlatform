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

const QString baseUrlTradeStation = "fuck";

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

    Q_ASSERT(!apiKey.isEmpty());

    thread->setObjectName("TradeStationClientThread");

    this->moveToThread(thread);

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
