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

const QString baseUrlTradeStation = "https://sim-api.tradestation.com/v3/";

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

void TradeStationClient::emitSignalDemuxer(RequestTypeInt type, const QJsonDocument &doc) {
    Q_UNUSED(type);
    Q_UNUSED(doc);
    // TODO: Implement signal demuxing when we add specific request types
}

void TradeStationClient::launchAuthProcess(QWidget* parent) {

    if (authInProgress) {
        qCWarning(TradeStationClientLog) << "Authentication process already in progress";
        return;
    }

    authInProgress = true;
    authWindow = new AuthWindow(parent);
    connect(authWindow, &AuthWindow::authenticationCompleted, this, &TradeStationClient::handleAuthCompleted);
    connect(authWindow, &AuthWindow::authenticationFailed, this, &TradeStationClient::handleAuthFailed);
    connect(authWindow, &QObject::destroyed, this, &TradeStationClient::handleAuthWindowDestroyed);
    authWindow->show();
}

void TradeStationClient::handleAuthCompleted(bool success) {
    authenticated = success;
    authInProgress = false;
    emit authenticationStateChanged(authenticated);
}

void TradeStationClient::handleAuthFailed(const QString error) {
    authenticated = false;
    authInProgress = false;
    emit authenticationError(error);
}

void TradeStationClient::handleAuthWindowDestroyed() {
    authWindow = nullptr;
}

bool TradeStationClient::fetchSyncAccounts(QVector<AccountResult> &results)
{
    QString url = buildUrlWithEndpointAndApiKeyHeaderParam("brokerage/accounts");
    QJsonDocument *jsonDocumentFromReplyToDelete = nullptr;

    bool ret = fetchSync(url, jsonDocumentFromReplyToDelete);

    if (ret) {
        // The positive return value implies jsonDocumentFromReplyToDelete has been allocated to something
        Q_ASSERT(jsonDocumentFromReplyToDelete != nullptr);

        // The API returns a single object with an "Accounts" array
        QJsonObject responseObj = jsonDocumentFromReplyToDelete->object();
        QJsonArray accountsArray = responseObj["Accounts"].toArray();

        // Resize the array in advance
        results.reserve(accountsArray.count());

        for (QJsonValue json: accountsArray) {
            results.push_back(AccountResult(json.toObject()));
        }

        // This pointer to a JSON document was allocated in the fetchSync and needs to be deleted after use
        TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);
    } else {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonDocumentFromReplyToDelete == nullptr);
    }

    return ret;
}
