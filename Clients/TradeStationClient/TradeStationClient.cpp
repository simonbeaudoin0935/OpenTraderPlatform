#include "TradeStationClient.h"
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
    RESTClient(baseUrlTradeStation),
    authenticated(false),
    authInProgress(false)
{
    authToken = AuthToken::loadFromSettings();
    clientToken = ClientToken::loadFromSettings();

    // If the token is invalid/absent, we need to perform an authentification with the popup
    if (!authToken.isValid() || !clientToken.isValid()) {
        qCDebug(TradeStationClientLog) << Q_FUNC_INFO <<
            "Auth token or Client token is invalid/absent, will need an authentification process";

        QTimer::singleShot(0, this, [this]() {
            emit authStateChanged(false, "authentification token invalid or absent at startup");
        });
    }

    // If the token is valid but expired, we don't need to perform an authentification, we can
    // just perform a refresh
    else if (authToken.isValid() && authToken.isExpired()) {

        qCDebug(TradeStationClientLog) << Q_FUNC_INFO <<
            "Auth token is valid but expired, perform a refresh now.";

        // Its okay to do this in the constructor, this will queue the request for when
        // the event loop starts
        refreshAsyncAccessToken();
    }

    // If the token is valid and not expired (has at least 5s left in it,
    // start using it
    else if (authToken.isValid() && !authToken.isExpired()) {

        authenticated = true;

        RESTClient::setAPIKey(authToken.getAccessToken());

        qCDebug(TradeStationClientLog) << Q_FUNC_INFO <<
            ": TradeStationClient created using KEY=" << authToken.getAccessToken();

        int secsUntilExpiration = authToken.secondsUntilExpiration();

        // Logically if we got here, there HAS to be at least 5 seconds left.
        // Compare against 4 just in case we are at 5 seconds left
        Q_ASSERT(secsUntilExpiration > 4);

        qCDebug(TradeStationClientLog) << Q_FUNC_INFO <<
            "Auth token is valid and already not expired, still has " <<
            secsUntilExpiration  << "second left to it";

        int secondsToNextRefreshRequest = authToken.secondsToNextRefreshRequest();

        // Logically if we are here this HAS to be t least 1s
        Q_ASSERT(secondsToNextRefreshRequest > 1);

        qCDebug(TradeStationClientLog) << Q_FUNC_INFO <<
            "Initiating a refresh in " << secondsToNextRefreshRequest << "seconds";

        // Launch a request in X seconds from now.
        QTimer::singleShot(1000 * secondsToNextRefreshRequest, this, [this]() {
            refreshAsyncAccessToken();
        });

        QTimer::singleShot(0, this, [this]() {
            emit authStateChanged(true, "Auth token valid and not expired");
        });

    } else {
        Q_UNREACHABLE();
    }

    thread->setObjectName("TradeStationClientThread");
}

TradeStationClient::~TradeStationClient() {
    qCDebug(TradeStationClientLog) << "Singleton instance destroyed";

    thread->quit();
    thread->wait();
}

// Launches a pop up. We will receive a signal when the process finishes
void TradeStationClient::launchAuthProcess(QWidget* parent) {

    if (authInProgress) {
        qCWarning(TradeStationClientLog) << "Authentication process already in progress";
        Q_ASSERT(0); // TODO check if necessary
        return;
    }

    authInProgress = true;
    authWindow = new AuthWindow(parent);
    connect(authWindow, &AuthWindow::authFinished, this, &TradeStationClient::onAuthFinished);
    connect(authWindow, &QObject::destroyed, this, &TradeStationClient::onAuthWindowDestroyed);
    authWindow->show();
}

void TradeStationClient::onAuthFinished(bool success, AuthToken token, QString reason) {
    authenticated = success;
    authInProgress = false;

    if (success) {
        bool stored = AuthToken::storeToSettings(token);
        Q_ASSERT(stored);
        qDebug(TradeStationClientLog) << Q_FUNC_INFO <<
            "Auth successful : " << reason;
    } else {
        qDebug(TradeStationClientLog) << Q_FUNC_INFO <<
            "Auth unsucessful : " << reason;
    }
    emit authStateChanged(authenticated, reason);
}

void TradeStationClient::onAuthWindowDestroyed() {
    // TODO race contition possible?
    authWindow = nullptr;
}

QNetworkRequest TradeStationClient::buildRefreshTokenRequest() {
    QUrl url;
    url.setScheme("https");
    url.setHost("signin.tradestation.com");
    url.setPath("/oauth/token"); // Leading slash ensures absolute path

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

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

void TradeStationClient::fetchAsyncAccounts()
{
    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, "brokerage/accounts");
    fetchAsync(request, static_cast<RequestTypeInt>(RequestType::Accounts));

    qCDebug(TradeStationClientLog) << Q_FUNC_INFO << "Fetching accounts";
}

void TradeStationClient::onAsyncRefreshTokenFinished(const AuthToken &newToken)
{
    authInProgress = false;
    bool success = newToken.isValidRefreshedToken() && !newToken.isExpired();

    if (success) {
        authenticated = true;

        // For some reason (security maybe) the new token return doesn't contain the refresh_key
        // All other fields are good (which is why it needs a special isValidRefreshedToken()
        // methods that does like isValid(), but omits the refresh_token field)
        // Now, we want to store this new token on disk, but we first need to retreive the
        // refresh_token from the actual token, stick it in there then save.
        emit authStateChanged(true, "Auth token refresh successful");\

        qCDebug(TradeStationClientLog) << Q_FUNC_INFO <<
            "Successful auth token refresh";

        // For some reason (security maybe) the new token return doesn't contain the refresh_key
        // All other fields are good (which is why it needs a special isValidRefreshedToken()
        // methods that does like isValid(), but omits the refresh_token field)
        // Now, we want to store this new token on disk, but we first need to retreive the
        // refresh_token from the actual token, stick it in there then save.
        AuthToken validNewToken(newToken.getAccessToken(),
                                authToken.getRefreshToken(),
                                newToken.getIdToken(),
                                newToken.getTokenType(),
                                newToken.getScope(),
                                newToken.getExpiresIn(),
                                newToken.getReceivedAt());

        AuthToken::storeToSettings(validNewToken);

        authToken = validNewToken;

        RESTClient::setAPIKey(authToken.getAccessToken());

        // Kick a new refresh in 20min - 5s
        {
            int secondsToNextRefreshRequest = authToken.secondsToNextRefreshRequest();

            // Logically if we are here this HAS to be t least 1s
            Q_ASSERT(secondsToNextRefreshRequest > 1 && secondsToNextRefreshRequest <= 1195);

            qCDebug(TradeStationClientLog) << Q_FUNC_INFO <<
                "Programming the nest refresh in " << secondsToNextRefreshRequest << "seconds";

            // Launch a request in X seconds from now.
            QTimer::singleShot(1000 * secondsToNextRefreshRequest, this, [this]() {
                refreshAsyncAccessToken();
            });
        }
    } else {
        authenticated = false;
        emit authStateChanged(false, "Failed to refresh access token");

        qCDebug(TradeStationClientLog) << Q_FUNC_INFO <<
            "Unsuccessful auth token refresh";
    }

#warning kick a watchdog timer in case the reply never comes
}

bool TradeStationClient::refreshSyncAccessToken()
{
    QJsonDocument *jsonDocumentFromReplyToDelete = nullptr;
    bool success;

    Q_ASSERT_X(0, "refreshSyncAccessToken", "DO NOT USE, CAUSES A DEADLOCK");

    authInProgress = true;

    qCDebug(TradeStationClientLog) << Q_FUNC_INFO <<
        "Starting a SYNC token refresh request";

    // Make sure we are good to go
    {
        // Note we don't check if authToken is expired, as it can be logically both
        Q_ASSERT(authToken.isValid());
        Q_ASSERT(clientToken.isValid());
    }

    {
        QNetworkRequest request;
        QByteArray postData;

        // Build the request and query using our static helper methods
        request = buildRefreshTokenRequest();
        postData = buildRefreshTokenQuery(clientToken.getClientId(),
                                          clientToken.getClientSecret(),
                                          authToken.getRefreshToken());

        // Make the POST request
        success = fetchSync(request, jsonDocumentFromReplyToDelete, HttpMethod::POST, postData);
    }

    if (!success) {
        qCWarning(TradeStationClientLog) << "Failed to refresh access token";
        authenticated = false;
        authInProgress = false;
        Q_ASSERT(jsonDocumentFromReplyToDelete == nullptr);
        emit authStateChanged(false, "Failed to refresh access token");
        return false;
    }

    Q_ASSERT(jsonDocumentFromReplyToDelete != nullptr);

    QJsonObject response = jsonDocumentFromReplyToDelete->object();
    TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);

    AuthToken newToken = AuthToken::receiveAuthToken(response);

    if (!newToken.isValid()) {
        qCWarning(TradeStationClientLog) << "Received refreshed token is invalid";
        authenticated = false;
        authInProgress = false;

        emit authStateChanged(false, "Received refreshed token is invalid");
        return false;
    }

    authInProgress = false;
    authenticated = true;
    emit authStateChanged(true, "Token refresh successful");

    return true;
}

void TradeStationClient::refreshAsyncAccessToken()
{
    Q_ASSERT_X(authInProgress == false,
               Q_FUNC_INFO,
               "A refresh token is already in progress");

    authInProgress = true;

    qCDebug(TradeStationClientLog) << Q_FUNC_INFO <<
        "Starting an ASYNC token refresh request";

    // Make sure we are good to go
    {
        // Note we don't check if authToken is expired, as it can be logically both
        Q_ASSERT(authToken.isValid());
        Q_ASSERT(clientToken.isValid());
    }

    {
        QNetworkRequest request;
        QByteArray postData;

        // Build the request and query using our static helper methods
        request = buildRefreshTokenRequest();
        postData = buildRefreshTokenQuery(clientToken.getClientId(),
                                          clientToken.getClientSecret(),
                                          authToken.getRefreshToken());

        // Make the POST request
        fetchAsync(request,
                   static_cast<RequestTypeInt>(RequestType::RefreshAccessToken),
                   HttpMethod::POST,
                   postData);
    }
}


void TradeStationClient::emitSignalDemuxer(RequestTypeInt type, const QJsonDocument &doc) {
    RequestType requestType = static_cast<RequestType>(type);
    QJsonObject obj = doc.object();

    switch(requestType) {

        case RequestType::None:
            Q_ASSERT_X(0,"","Should not be None anymore");
            break;

        case RequestType::Accounts:
        {
            QJsonArray accountsArray = obj["Accounts"].toArray();
            QVector<AccountResult> results;

            // Resize the array in advance
            results.reserve(accountsArray.count());

            for (QJsonValue json: accountsArray) {
                results.push_back(AccountResult(json.toObject()));
            }

            emit accountsReceived(results);
            break;
        }

        case RequestType::RefreshAccessToken:
            // No emit on purpose, this is calling a private function of this class
            onAsyncRefreshTokenFinished(AuthToken::receiveAuthToken(obj));
            break;

    default:
        Q_UNREACHABLE();
        break;
    }
}
