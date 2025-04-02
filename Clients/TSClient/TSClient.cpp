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

#define ENDPOINT_STREAM_BARS               "marketdata/stream/barcharts"
#define ENDPOINT_STREAM_MARKET_DEPTH_QUOTE "marketdata/stream/marketdepth/quotes"

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
        QTimer::singleShot(0, this, [this]() {
            emit authStateChanged(false, "authentification token invalid or absent at startup");
        });
    }

    // If the token is valid but expired, we don't need to perform an authentification, we can
    // just perform a refresh
    else if (authToken.isValid() && authToken.isExpired()) {

        qCDebug(TSClientLog) << Q_FUNC_INFO <<
            "Auth token is valid but expired, perform a refresh now.";

        // Schedule a refresh for when the thread starts
        QTimer::singleShot(0, this, [this]() {
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
        QTimer::singleShot(0, this, [this]() {
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

    bool success = fetchSync(request, jsonDocumentFromReplyToDelete);

    if (false == success) {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonDocumentFromReplyToDelete == nullptr);

        return false;
    }

    // The positive return value implies jsonDocumentFromReplyToDelete has been allocated to something
    Q_ASSERT(jsonDocumentFromReplyToDelete != nullptr);

    // The API returns a single object with an "Accounts" array
    QJsonObject responseObj = jsonDocumentFromReplyToDelete->object();
    const QJsonArray accountsArray = responseObj["Accounts"].toArray();

    // Resize the array in advance
    results.reserve(accountsArray.count());

    for (const QJsonValue &json: accountsArray) {
        results.push_back(AccountsResult(json.toObject()));
    }

    // This pointer to a JSON document was allocated in the fetchSync and needs to be deleted after use
    TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);

    return true;
}

void TSClient::fetchAsyncAccounts()
{
    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, "brokerage/accounts");
    fetchAsync(request, static_cast<RequestTypeInt>(RequestType::Accounts));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching accounts";
}

void TSClient::onAsyncRefreshTokenFinished(const AuthToken &newToken)
{
    bool success = newToken.isValidRefreshedToken() && !newToken.isExpired();

    authInProgress = false;
    authenticated  = success;

    if (false == success) {
        emit authStateChanged(false, "Failed to refresh access token");

        qCDebug(TSClientLog) << Q_FUNC_INFO <<
            "Unsuccessful auth token refresh";

        return;
    }

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

    // TODO kick a watchdog timer
}

bool TSClient::refreshSyncAccessToken()
{
    QJsonDocument *jsonDocumentFromReplyToDelete = nullptr;
    bool success;

    authInProgress = true;

    Q_ASSERT_X(0, "refreshSyncAccessToken", "DO NOT USE, CAUSES A DEADLOCK");
    Q_ASSERT(authToken.isValid()); // Note we don't check if authToken is expired, as it can be logically both
    Q_ASSERT(clientToken.isValid());

    qCDebug(TSClientLog) << Q_FUNC_INFO <<
        "Starting a SYNC token refresh request";


    // Fetch the token
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

    if (false == success) {
        Q_ASSERT(jsonDocumentFromReplyToDelete == nullptr);

        qCWarning(TSClientLog) << "Failed to refresh access token";
        authenticated = false;
        authInProgress = false;
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

    //TODO store that new token
    authToken = newToken;

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
            const QJsonArray accountsArray = obj["Accounts"].toArray();
            QVector<AccountsResult> results;

            // Resize the array in advance
            results.reserve(accountsArray.count());

            for (const QJsonValue &json: accountsArray) {
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

void TSClient::processStreamFinished(QByteArray &rawData, void *arg)
{
    Q_ASSERT(arg != nullptr);
    Q_UNUSED(rawData)
#warning REMOVE this is dead code
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

    qCDebug(TSClientLog).noquote() << Q_FUNC_INFO << "Received JSON : \n" << jsonDocumentFromReplyToDelete->toJson(QJsonDocument::Indented);

    // This pointer to a JSON array was allocated in the fetchSync and needs to be deleted after use
    TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);

    return true;
}

void TSClient::placeAsyncOrder(const PlaceOrderRequest &order) {
    Q_UNUSED(order);

    Q_ASSERT_X(0, "placeAsyncOrder", "TODO implement");
}

void TSClient::openStream(const QString &symbol, const QString &endpoint, const QUrlQuery &query, Stream * const stream) {
    Q_ASSERT(!symbol.isEmpty());
    Q_ASSERT(symbol.length() >= 1 && symbol.length() <= 7);
    Q_ASSERT(!symbol.contains(','));
    Q_ASSERT(symbol.isUpper());
    Q_ASSERT(stream != nullptr);
    Q_ASSERT(!endpoint.isEmpty());

    QMetaObject::invokeMethod(this,
        [this, &symbol, &endpoint, &query, stream]()
        {
            QUrl url(QString(BASE_URL_TS_API_SIMULATION) + endpoint + "/" + symbol);
            url.setQuery(query);

            QNetworkRequest request(url);
            request.setRawHeader("Authorization", QString("Bearer %1").arg(authToken.getAccessToken()).toUtf8());
            //request.setRawHeader("Accept", "application/json");
            //request.setRawHeader("Connection", "keep-alive");

            // The stream was new'ed in the caller's thread
            stream->setParent(this);
            streams.push_back(stream);

            QNetworkReply *reply = fetchStream(request, static_cast<void*>(stream));

            // The readyRead, finished and errorOccured are connected internaly here.
            // This call starts the timeout timer as well
            stream->setNetworkReply(reply);

            connect(stream, &Stream::receivedAmountOfData, this, &TSClient::onReceivedNewAmountOfData);
        },
    Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
                                   // finishes executing this lambda so that a valid pointer is returned

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opened Stream " << static_cast<void*>(stream);

    return;
}

void TSClient::closeStream(Stream* const stream) {
    Q_ASSERT(stream != nullptr);

    QMetaObject::invokeMethod(this,
        [this, &stream]()
        {
            bool removed = streams.removeOne(stream);
            Q_ASSERT(removed); // The stream was likely already closed, or a bad pointer was passed

            RESTClient::closeStream(static_cast<void*>(stream));

            delete stream;
        },
    Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread finishes executing this lambda

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closed Stream " << static_cast<void*>(stream);
}


StreamBars *TSClient::openStreamBars(QString &symbol, unsigned int interval, StreamBarsUnit unit, unsigned int barsback, StreamBarsSessionTemplate sessionTemplate)
{
    // Interval that each bar will consist of - for minute bars, the number of minutes aggregated in a single bar. For bar units other than minute, value must be 1.
    if (unit == StreamBarsUnit::Minute) {Q_ASSERT(interval >= 1);}
    else { Q_ASSERT(interval == 1);}
    Q_ASSERT(barsback <= 57600);

    const QString endpoint = ENDPOINT_STREAM_BARS;

    QUrlQuery query;
    query.addQueryItem("interval", QString::number(interval));
    query.addQueryItem("unit", [unit]() -> QString {
        switch (unit) {
        case StreamBarsUnit::Minute: return "Minute";
        case StreamBarsUnit::Daily: return "Daily";
        case StreamBarsUnit::Weekly: return "Weekly";
        case StreamBarsUnit::Monthly: return "Monthly";
        default: Q_UNREACHABLE_RETURN("Unknown");
        }
    }());
    query.addQueryItem("barsback", QString::number(interval));
    query.addQueryItem("sessiontemplate", [sessionTemplate]() -> QString {
        switch (sessionTemplate) {
        case StreamBarsSessionTemplate::USEQPre: return "USEQPre";
        case StreamBarsSessionTemplate::USEQPost: return "USEQPost";
        case StreamBarsSessionTemplate::USEPreAndPost: return "USEPreAndPost";
        case StreamBarsSessionTemplate::USEQ24Hour: return "USEQ24Hour";
        case StreamBarsSessionTemplate::Default: return "Default";
        default: Q_UNREACHABLE_RETURN("Unknown");
        }
    }());

    StreamBars * stream = new StreamBars();
    stream->moveToThread(thread);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamBars" << static_cast<void*>(stream);

    TSClient::openStream(symbol, endpoint, query, stream);

    return stream;
}

void TSClient::closeStreamBars(StreamBars *stream)
{
    Q_ASSERT(stream != nullptr);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closing StreamBars " << static_cast<void*>(stream);

    TSClient::closeStream(stream);
}

StreamMarketDepthQuote* TSClient::openStreamMarketDepthQuote(QString &symbol, unsigned int depth)
{
    Q_ASSERT(depth >= 1 && depth <= 20);

    const QString endpoint = ENDPOINT_STREAM_MARKET_DEPTH_QUOTE;
    QUrlQuery query;
    query.addQueryItem("maxlevels", QString::number(depth));

    StreamMarketDepthQuote * stream = new StreamMarketDepthQuote();
    stream->moveToThread(thread);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamMarketDepthQuote " << static_cast<void*>(stream);

    TSClient::openStream(symbol, endpoint, query, stream);

    // Specially to StreakMarketDepth Quote/Aggregates, there is the possibility that the client account
    // do not have access to level2 data.
    connect(stream, &Stream::marketDepthNotAvailable,
            this, &TSClient::marketDepthNotAvailable);

    return stream;
}

void TSClient::closeStreamMarketDepthQuote(StreamMarketDepthQuote *stream)
{
    Q_ASSERT(stream != nullptr);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closing StreamMarketDepthQuote " << static_cast<void*>(stream);

    TSClient::closeStream(stream);
}
