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

#include "TSClient.h"

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
    thread->setObjectName("TSClientThread");

    clientToken = ClientToken::loadFromSettings();
    authToken = AuthToken::loadFromSettings();
    

    // If the token is invalid/absent, we need to perform an authentification with the popup
    if (!clientToken.isValid() || !authToken.isValid()) {
        qCWarning(TSClientLog) << Q_FUNC_INFO <<
            "Auth token or Client token is invalid/absent, will need an authentification process";

        // Schedule an emition for when the event loop is started
        // TODO I have removed this because it crashed the main algo thread. I think an emit of stat
        // change of false should not be done at startup because we are of course not authenticated at
        // first, and the main algo (for now) is written to fatal it happens.
        //QTimer::singleShot(0, this, [this]() {
        //    emit authStateChanged(false, "authentification token invalid or absent at startup");
        //});
    }

    // If the token is valid but expired, we don't need to perform an authentification, we can
    // just perform a refresh
    else if (authToken.isValid() && authToken.isExpired()) {

        qCInfo(TSClientLog) << Q_FUNC_INFO <<
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

        int secsUntilExpiration = authToken.secondsUntilExpiration();

        // Logically if we got here, there HAS to be at least 5 seconds left.
        // Compare against 4 just in case we are at 5 seconds left
        Q_ASSERT(secsUntilExpiration > 4);

        qCInfo(TSClientLog) << Q_FUNC_INFO <<
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
}

TSClient::~TSClient() {
    thread->quit();
    thread->wait();

    qCDebug(TSClientLog) << "Singleton instance destroyed";
}

#ifdef GUI_ENABLED
// Launches a pop up. We will receive a signal when the process finishes
void TSClient::launchAuthProcess() {

    Q_ASSERT(!authInProgress);

    authInProgress = true;
    authWindow = new AuthWindow();
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
#endif


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


void TSClient::onAsyncRefreshTokenFinished(bool completed, const AuthToken &newToken)
{
    bool success = completed && newToken.isValidRefreshedToken() && !newToken.isExpired();

    authInProgress = false;
    authenticated  = success;

    if (false == success) {
        emit authStateChanged(false, "Failed to refresh access token");

        qCCritical(TSClientLog) << Q_FUNC_INFO << "Unsuccessful auth token refresh";

        //TODO retry

        return;
    }

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
            "Programming the next refresh in " << secondsToNextRefreshRequest << " seconds";

        // Launch a request in X seconds from now.
        QTimer::singleShot(1000 * secondsToNextRefreshRequest, this, [this]() {
            refreshAsyncAccessToken();
        });
    }

    QTimer::singleShot(1000, this, [this]() {
        // Based on observation, if we propagate the good new immediately and start
        // making calls, the remote server will send us back an error 401 (unauthenticated)
        // for the first API call. Almost as if the refresh did not properly propagade in their system.
        // Wait a second on our end before propagating the successful authentification as to delay
        // making the first API call.
        emit authStateChanged(true, "Auth token refresh successful");
    });
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
    Q_ASSERT_X(authInProgress == false, Q_FUNC_INFO, "A refresh token is already in progress");

    authInProgress = true;

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Starting an ASYNC token refresh request";

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
                   static_cast<RequestTypeInt>(RequestType::GetRefreshAccessToken),
                   HttpMethod::POST,
                   postData);
    }
}


void TSClient::emitSignalDemuxer(RequestTypeInt type, const QJsonDocument &doc, bool completed, void *optArg) {
    RequestType requestType = static_cast<RequestType>(type);
    QJsonObject obj = doc.object();

    if (!completed) {
        qCCritical(TSClientLog) << Q_FUNC_INFO << "Async operation not completed";
    }

    switch(requestType) {

        case RequestType::None:
            Q_ASSERT_X(0,"","Should not be None anymore");
            break;

        case RequestType::GetAccounts:
        {
            const QJsonArray accountsArray = obj["Accounts"].toArray();
            QVector<Account> results;

            // Resize the array in advance
            results.reserve(accountsArray.count());

            for (const QJsonValue &json: accountsArray) {
                results.push_back(Account(json.toObject()));
            }

            emit getAccountsAsyncReceived(results);
            break;
        }

        case RequestType::GetBalances:
        {
            const QJsonArray balancesArray = obj["Balances"].toArray();
            QVector<Balance> results;

            // Resize the array in advance
            results.reserve(balancesArray.count());

            for (const QJsonValue &json: balancesArray) {
                results.push_back(Balance(json.toObject()));
            }

            emit getBalancesAsyncReceived(results);
            break;
        }

        case RequestType::GetBars:
        {
            Q_ASSERT(optArg != nullptr);
            QString* symbol = static_cast<QString*>(optArg);

            const QJsonArray barsArray = obj["Bars"].toArray();
            QVector<Bar> results;



            // Resize the array in advance
            results.reserve(barsArray.count());

            for (const QJsonValue &json: barsArray) {
                results.push_back(Bar(json.toObject()));
            }

            emit getBarsAsyncReceived(*symbol, results);

            // symbol was new'ed when the async function get was called
            delete symbol;

            break;
        }

        case RequestType::GetQuoteSnapshots:
            Q_ASSERT(0); //TODO not yet implemented
            break;

        case RequestType::PlaceOrder:
        {
            PlaceOrderResult result;

            result = PlaceOrderResult(doc.object());

            emit placeOrderAsyncReceived(result);

            break;
        }

        case RequestType::CancelOrder:
        {
            CancelOrderResult result;

            result = CancelOrderResult(doc.object());

            emit cancelOrderAsyncReceived(result);

            break;
        }

        case RequestType::GetRefreshAccessToken:
            // No emit on purpose, this is calling a private function of this class
            onAsyncRefreshTokenFinished(completed, AuthToken::receiveAuthToken(obj));
            break;

    default:
        Q_UNREACHABLE();
        break;
    }
}

void TSClient::openStream(const QString &symbol, const QString &endpoint, const QUrlQuery &query, Stream * const stream) {
    Q_ASSERT(!symbol.isEmpty());
    if (symbol != "NOSYMBOL") Q_ASSERT(symbol.length() >= 1 && symbol.length() <= 8);
    Q_ASSERT(!symbol.contains(','));
    Q_ASSERT(symbol.isUpper());
    Q_ASSERT(stream != nullptr);
    Q_ASSERT(!endpoint.isEmpty());

    // Only external callers to TSClient thread should get here. Calling a fetch sync from within the TSClient's
    // thread would cause a deadlock to itself
    Q_ASSERT_X(QThread::currentThread() != thread, Q_FUNC_INFO, "TSClient object cannot call this function itself");

    // FIXME fix this NOSYMBOL shit
    QMetaObject::invokeMethod(this,
        [this, &symbol, &endpoint, &query, stream]()
        {
            QUrl url(QString(BASE_URL_TS_API_SIMULATION) + endpoint + ((symbol=="NOSYMBOL") ? "" : ("/" + symbol)));
            url.setQuery(query);

            QNetworkRequest request(url);
            request.setRawHeader("Authorization", QString("Bearer %1").arg(authToken.getAccessToken()).toUtf8());

            // The stream was new'ed in the caller's thread
            stream->setParent(this); //TODO is this the right thing?
            streams.push_back(stream);

            QNetworkReply *reply = fetchStream(request, static_cast<void*>(stream));

            // The readyRead, finished and errorOccured are connected internaly here.
            // This call starts the timeout timer as well
            stream->setNetworkReply(reply);

            connect(stream, &Stream::receivedAmountOfData, this, &TSClient::onReceivedNewAmountOfData);
            
            // Emit signal that stream count has changed
            emit streamCountChanged(streams.size());
        },
    Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
                                   // finishes executing this lambda so that a valid pointer is returned

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opened Stream " << static_cast<void*>(stream);

    return;
}

void TSClient::closeStream(Stream* const stream) {
    Q_ASSERT(stream != nullptr);
    Q_ASSERT_X(QThread::currentThread() != thread, Q_FUNC_INFO, "TSClient object cannot call this function itself");

    QMetaObject::invokeMethod(this,
        [this, &stream]()
        {
            bool removed = streams.removeOne(stream);
            Q_ASSERT(removed); // The stream was likely already closed, or a bad pointer was passed

            RESTClient::closeStream(static_cast<void*>(stream));

            delete stream;
            
            // Emit signal that stream count has changed
            emit streamCountChanged(streams.size());
        },
    Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread finishes executing this lambda

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closed Stream " << static_cast<void*>(stream);
}
