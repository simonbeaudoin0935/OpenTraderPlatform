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

const QUrl baseUrlTradeStation("https://sim-api.tradestation.com/v3/");

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

QNetworkRequest TradeStationClient::buildRefreshTokenRequest(const QString &clientId, 
                                                           const QString &clientSecret,
                                                           const QString &refreshToken) {
    QUrl url;
    url.setScheme("https");
    url.setHost("signin.tradestation.com");
    url.setPath("/oauth/token"); // Leading slash ensures absolute path

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    // Get the post data directly as QByteArray
    QByteArray postData = buildRefreshTokenQuery(clientId, clientSecret, refreshToken);
    request.setHeader(QNetworkRequest::ContentLengthHeader, postData.size());

    return request;
}

QByteArray TradeStationClient::buildRefreshTokenQuery(const QString &clientId,
                                                   const QString &clientSecret,
                                                   const QString &refreshToken) {
    QUrlQuery query;
    query.addQueryItem("grant_type", "refresh_token");
    query.addQueryItem("client_id", clientId);
    query.addQueryItem("client_secret", clientSecret);
    query.addQueryItem("refresh_token", refreshToken);
    return query.toString(QUrl::FullyEncoded).toUtf8();
}

bool TradeStationClient::fetchSyncAccounts(QVector<AccountResult> &results)
{
    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, "brokerage/accounts", "");
    QJsonDocument *jsonDocumentFromReplyToDelete = nullptr;

    bool ret = fetchSync(request, jsonDocumentFromReplyToDelete);

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

bool TradeStationClient::refreshSyncAccessToken()
{
    // Variable declarations
    QString refreshToken;
    QString clientId;
    QString clientSecret;

    QJsonDocument *jsonDocumentFromReplyToDelete = nullptr;
    bool success;

    // Get the tokens from the AuthWindow and credentials store
    { 
        // Get the current refresh token from AuthWindow
        refreshToken = AuthWindow::getRefreshToken();
        Q_ASSERT_X(!refreshToken.isEmpty(), "refreshSyncAccessToken", "No refresh token available");

        // Load credentials from settings
        QSettings credentialsStore(QSettings::IniFormat, QSettings::UserScope,
                                "TradeStationAuth", "Credentials");
        clientId = credentialsStore.value("credentials/client_id").toString();
        clientSecret = credentialsStore.value("credentials/client_secret").toString();

        Q_ASSERT_X(!clientId.isEmpty(), "refreshSyncAccessToken", "Client ID not available");
        Q_ASSERT_X(!clientSecret.isEmpty(), "refreshSyncAccessToken", "Client secret not available");
    }

    {
        QNetworkRequest request;
        QByteArray postData;

        // Build the request and query using our static helper methods
        request = buildRefreshTokenRequest(clientId, clientSecret, refreshToken);
        postData = buildRefreshTokenQuery(clientId, clientSecret, refreshToken);

        // Make the POST request
        success = fetchSync(request, jsonDocumentFromReplyToDelete, HttpMethod::POST, &postData);
    }

    if (!success) {
        qCWarning(TradeStationClientLog) << "Failed to refresh access token";
        Q_ASSERT(jsonDocumentFromReplyToDelete == nullptr);
        return false;
    } else {
        Q_ASSERT(jsonDocumentFromReplyToDelete != nullptr);
    }

    QJsonObject response = jsonDocumentFromReplyToDelete->object();
        
    Q_ASSERT(response.contains("access_token") && !response["access_token"].toString().isEmpty());
    Q_ASSERT(response.contains("scope") && response["scope"].toString() == "openid offline_access");
    Q_ASSERT(response.contains("expires_in") && response["expires_in"].toInt() == 1200);
    Q_ASSERT(response.contains("id_token") && !response["id_token"].toString().isEmpty());
    Q_ASSERT(response.contains("token_type") && response["token_type"].toString() == "Bearer");

    QString newAccessToken = response["access_token"].toString();
    QString newIdToken = response["id_token"].toString();
 
    // Update the tokens in AuthWindow
    // TODO: Consider adding a method in AuthWindow to update tokens directly
    // For now, we'll save them using the existing mechanism
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "TradeStationAuth", "Tokens");
    settings.setValue("access_token", newAccessToken);
    settings.setValue("id_token", newIdToken);
    settings.setValue("token_timeout_seconds", 1200);
    settings.setValue("token_received_time", QDateTime::currentDateTime().toString(Qt::ISODate));



    settings.sync();
               
    TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);
    return true;
}