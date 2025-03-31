#include "TSClient.h"
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

#define BASE_URL_TS_API_SIMULATION "https://sim-api.tradestation.com/v3/"

#define AT_THREAD_START 0

// Define the logging category
Q_LOGGING_CATEGORY(TSClientLog, "TSClient")

// Initialize static member outside class
TSClient* TSClient::instance = nullptr;

TSClient& TSClient::getInstance() {
    if (instance == nullptr) {
        qCDebug(TSClientLog) << "Singleton instance created";
        instance = new TSClient();
    }
    return *instance;
}

TSClient* TSClient::getInstancePtr() {
    if (instance == nullptr) {
        qCDebug(TSClientLog) << "Singleton instance created";
        instance = new TSClient();
    }
    return instance;
}

TSClient::TSClient() :
    RESTClient(QUrl(BASE_URL_TS_API_SIMULATION)),
    authenticated(false),
    authInProgress(false)
{
    authToken = AuthToken::loadFromSettings();
    clientToken = ClientToken::loadFromSettings();

    // If the token is invalid/absent, we need to perform an authentification with the popup
    if (!authToken.isValid() || !clientToken.isValid()) {
        qCDebug(TSClientLog) << Q_FUNC_INFO <<
            "Auth token or Client token is invalid/absent, will need an authentification process";

        // Schedule an emition for when the event loop is started
        QTimer::singleShot(AT_THREAD_START, this, [this]() {
            emit authStateChanged(false, "authentification token invalid or absent at startup");
        });
    }

    // If the token is valid but expired, we don't need to perform an authentification, we can
    // just perform a refresh
    else if (authToken.isValid() && authToken.isExpired()) {

        qCDebug(TSClientLog) << Q_FUNC_INFO <<
            "Auth token is valid but expired, perform a refresh now.";

        // Schedule a refresh for when the thread starts
        QTimer::singleShot(AT_THREAD_START, this, [this]() {
            refreshAsyncAccessToken();
        });
    }

    // If the token is valid and not expired (has at least 5s left in it,
    // start using it
    else if (authToken.isValid() && !authToken.isExpired()) {

        authenticated = true;

        RESTClient::setAPIKey(authToken.getAccessToken());

        qCDebug(TSClientLog) << Q_FUNC_INFO <<
            ": TSClient created using KEY=" << authToken.getAccessToken();

        int secsUntilExpiration = authToken.secondsUntilExpiration();

        // Logically if we got here, there HAS to be at least 5 seconds left.
        // Compare against 4 just in case we are at 5 seconds left
        Q_ASSERT(secsUntilExpiration > 4);

        qCDebug(TSClientLog) << Q_FUNC_INFO <<
            "Auth token is valid and already not expired, still has " <<
            secsUntilExpiration  << "second left to it";

        int secondsToNextRefreshRequest = authToken.secondsToNextRefreshRequest();

        // Logically if we are here this HAS to be t least 1s
        Q_ASSERT(secondsToNextRefreshRequest > 1);

        qCDebug(TSClientLog) << Q_FUNC_INFO <<
            "Initiating a refresh in " << secondsToNextRefreshRequest << "seconds";

        // Launch a request in X seconds from now.
        QTimer::singleShot(1000 * secondsToNextRefreshRequest, this, [this]() {
            refreshAsyncAccessToken();
        });

        // Schedule an emition for when the event loop is started
        QTimer::singleShot(AT_THREAD_START, this, [this]() {
            emit authStateChanged(true, "Auth token valid and not expired");
        });

    } else {
        Q_UNREACHABLE();
    }

    thread->setObjectName("TSClientThread");
}

TSClient::~TSClient() {
    qCDebug(TSClientLog) << "Singleton instance destroyed";

    thread->quit();
    thread->wait();
}

// Launches a pop up. We will receive a signal when the process finishes
void TSClient::launchAuthProcess(QWidget* parent) {

    if (authInProgress) {
        qCWarning(TSClientLog) << "Authentication process already in progress";
        Q_ASSERT(0); // TODO check if necessary
        return;
    }

    authInProgress = true;
    authWindow = new AuthWindow(parent);
    connect(authWindow, &AuthWindow::authFinished, this, &TSClient::onAuthFinished);
    connect(authWindow, &QObject::destroyed, this, &TSClient::onAuthWindowDestroyed);
    authWindow->show();
}

void TSClient::onAuthFinished(bool success, AuthToken token, QString reason) {
    authenticated = success;
    authInProgress = false;

    if (success) {
        bool stored = AuthToken::storeToSettings(token);
        Q_ASSERT(stored);
        qDebug(TSClientLog) << Q_FUNC_INFO <<
            "Auth successful : " << reason;
    } else {
        qDebug(TSClientLog) << Q_FUNC_INFO <<
            "Auth unsucessful : " << reason;
    }
    emit authStateChanged(authenticated, reason);
}

void TSClient::onAuthWindowDestroyed() {
    // TODO race contition possible?
    authWindow = nullptr;
}

QNetworkRequest TSClient::buildRefreshTokenRequest() {
    QUrl url;
    url.setScheme("https");
    url.setHost("signin.tradestation.com");
    url.setPath("/oauth/token"); // Leading slash ensures absolute path

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    return request;
}

QByteArray TSClient::buildRefreshTokenQuery(const QString &clientId,
                                                   const QString &clientSecret,
                                                   const QString &refreshToken) {
    QUrlQuery query;
    query.addQueryItem("grant_type", "refresh_token");
    query.addQueryItem("client_id", clientId);
    query.addQueryItem("client_secret", clientSecret);
    query.addQueryItem("refresh_token", refreshToken);
    return query.toString(QUrl::FullyEncoded).toUtf8();
}

bool TSClient::fetchSyncAccounts(QVector<AccountsResult> &results)
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
            results.push_back(AccountsResult(json.toObject()));
        }

        // This pointer to a JSON document was allocated in the fetchSync and needs to be deleted after use
        TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);
    } else {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonDocumentFromReplyToDelete == nullptr);
    }

    return ret;
}

void TSClient::fetchAsyncAccounts()
{
    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, "brokerage/accounts");
    fetchAsync(request, static_cast<RequestTypeInt>(RequestType::Accounts));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching accounts";
}

void TSClient::onAsyncRefreshTokenFinished(const AuthToken &newToken)
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

        qCDebug(TSClientLog) << Q_FUNC_INFO <<
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

            qCDebug(TSClientLog) << Q_FUNC_INFO <<
                "Programming the next refresh in " << secondsToNextRefreshRequest << "seconds";

            // Launch a request in X seconds from now.
            QTimer::singleShot(1000 * secondsToNextRefreshRequest, this, [this]() {
                refreshAsyncAccessToken();
            });
        }
    } else {
        authenticated = false;
        emit authStateChanged(false, "Failed to refresh access token");

        qCDebug(TSClientLog) << Q_FUNC_INFO <<
            "Unsuccessful auth token refresh";
    }

#warning TODO kick a watchdog timer in case the reply never comes
}

bool TSClient::refreshSyncAccessToken()
{
    QJsonDocument *jsonDocumentFromReplyToDelete = nullptr;
    bool success;

    Q_ASSERT_X(0, "refreshSyncAccessToken", "DO NOT USE, CAUSES A DEADLOCK");

    authInProgress = true;

    qCDebug(TSClientLog) << Q_FUNC_INFO <<
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
        qCWarning(TSClientLog) << "Failed to refresh access token";
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
        qCWarning(TSClientLog) << "Received refreshed token is invalid";
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

void TSClient::refreshAsyncAccessToken()
{
    Q_ASSERT_X(authInProgress == false,
               Q_FUNC_INFO,
               "A refresh token is already in progress");

    authInProgress = true;

    qCDebug(TSClientLog) << Q_FUNC_INFO <<
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


void TSClient::emitSignalDemuxer(RequestTypeInt type, const QJsonDocument &doc) {
    RequestType requestType = static_cast<RequestType>(type);
    QJsonObject obj = doc.object();

    switch(requestType) {

        case RequestType::None:
            Q_ASSERT_X(0,"","Should not be None anymore");
            break;

        case RequestType::Accounts:
        {
            QJsonArray accountsArray = obj["Accounts"].toArray();
            QVector<AccountsResult> results;

            // Resize the array in advance
            results.reserve(accountsArray.count());

            for (QJsonValue json: accountsArray) {
                results.push_back(AccountsResult(json.toObject()));
            }

            emit accountsAsyncReceived(results);
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

bool TSClient::placeSyncOrder(const PlaceOrderRequest &order, PlaceOrderResult &result) {

    QJsonDocument *jsonDocumentFromReplyToDelete = nullptr;

    Q_ASSERT(order.isValid());

    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, "orderexecution/orders");

    QByteArray postData = QJsonDocument(order.toJson()).toJson(QJsonDocument::Compact);

    bool success = fetchSync(request,
                             jsonDocumentFromReplyToDelete,
                             HttpMethod::POST,
                             postData);
    
    

    if (!success) {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonDocumentFromReplyToDelete == nullptr);

        return false;
    }

    // The positive return value implies jsonDocumentFromReplyToDelete has been allocated to something
    Q_ASSERT(jsonDocumentFromReplyToDelete != nullptr);

    result = PlaceOrderResult(jsonDocumentFromReplyToDelete->object());

    qDebug().noquote() << "result : \n" << jsonDocumentFromReplyToDelete->toJson(QJsonDocument::Indented);

    // This pointer to a JSON array was allocated in the fetchSync and needs to be deleted after use
    TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);

    return true;
}

void TSClient::placeAsyncOrder(const PlaceOrderRequest &order) {
#warning complete
}

