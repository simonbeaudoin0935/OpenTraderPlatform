#include <QNetworkAccessManager>
#include <QThread>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QDebug>

#include "TSClient.h"

Q_LOGGING_CATEGORY(TSClientLog, "TSClient")

#define BASE_URL_SCHEME                "https"
#define BASE_URL_HOST_SIMULATION       "sim-api.tradestation.com"
#define BASE_URL_HOST_VERSION          "/v3/"

#define ENDPOINT_GET_QUOTE_SNAPSHOTS       "marketdata/quotes/%1"
#define ENDPOINT_GET_BARS                  "marketdata/barcharts/%1"
#define ENDPOINT_STREAM_BARS               "marketdata/stream/barcharts"
#define ENDPOINT_STREAM_MARKET_DEPTH_QUOTE "marketdata/stream/marketdepth/quotes"

#define ENDPOINT_GET_ACCOUNTS              "brokerage/accounts"
#define ENDPOINT_GET_BALANCES              "brokerage/accounts/%1/balances"
#define ENDPOINT_STREAM_ORDERS             "brokerage/stream/accounts/%1/orders"
#define ENDPOINT_STREAM_POSITIONS          "brokerage/stream/accounts/%1/positions"

#define ENDPOINT_PLACE_ORDER               "orderexecution/orders"
#define ENDPOINT_CANCEL_ORDER              "orderexecution/orders/%1"

// Initialize static member outside class
TSClient* TSClient::m_instance = nullptr;
    
TSClient& TSClient::getInstance()
{
    if (m_instance == nullptr) {
        qCDebug(TSClientLog) << "Singleton instance created";
        m_instance = new TSClient();
    }
    return *m_instance;
}

TSClient* TSClient::getInstancePtr()
{
    if (m_instance == nullptr) {
        qCDebug(TSClientLog) << "Singleton instance created";
        m_instance = new TSClient();
    }
    return m_instance;
}

TSClient::TSClient() :
    RESTClient(),
    m_authenticated(false),
    m_refreshInProgress(false)
{
    m_baseUrl.setScheme(BASE_URL_SCHEME);
    m_baseUrl.setHost(BASE_URL_HOST_SIMULATION);
    m_baseUrl.setPath(BASE_URL_HOST_VERSION);

    m_thread->setObjectName("TSClientThread");
    m_clientToken = ClientToken::loadFromSettings();
    m_authToken = AuthToken::loadFromSettings();
    

    // If the token is invalid/absent, we need to perform an authentification with the popup
    if (!m_clientToken.isValid() || !m_authToken.isValid()) {
        qCWarning(TSClientLog) << Q_FUNC_INFO <<"Auth token or Client token is invalid/absent, will need an authentification process";

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
    else if (m_authToken.isValid() && m_authToken.isExpired()) {

        qCInfo(TSClientLog) << Q_FUNC_INFO << "Auth token is valid but expired, perform a refresh now.";

        // Schedule a refresh for when the thread starts
        QTimer::singleShot(0, this, [this]() {
            refreshAsyncAccessToken();
        });
    }

    // If the token is valid and not expired (has at least 5s left in it,
    // start using it
    else if (m_authToken.isValid() && !m_authToken.isExpired()) {

        m_authenticated = true;

        RESTClient::setAPIKey(m_authToken.getAccessToken());

        int secsUntilExpiration = m_authToken.secondsUntilExpiration();

        // Logically if we got here, there HAS to be at least 5 seconds left.
        // Compare against 4 just in case we are at 5 seconds left
        Q_ASSERT(secsUntilExpiration > 4);

        qCInfo(TSClientLog) << "Auth token is valid and already not expired, still has " << secsUntilExpiration  << "second left to it";

        int secondsToNextRefreshRequest = m_authToken.secondsToNextRefreshRequest();

        // Logically if we are here this HAS to be t least 1s
        Q_ASSERT(secondsToNextRefreshRequest > 1);

        qCDebug(TSClientLog) << "Initiating a refresh in " << secondsToNextRefreshRequest << "seconds";

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

TSClient::~TSClient()
{
    m_thread->quit();
    m_thread->wait();

    qCDebug(TSClientLog) << "Singleton instance destroyed";
}

#ifdef GUI_ENABLED
// Launches a pop up. We will receive a signal when the process finishes
void TSClient::launchAuthProcess()
{

    Q_ASSERT(m_authInProgress == false);

    m_authInProgress = true;
    m_authWindow = new AuthWindow();
    connect(m_authWindow, &AuthWindow::authFinished, this, &TSClient::onAuthFinished);
    connect(m_authWindow, &QObject::destroyed, this, &TSClient::onAuthWindowDestroyed);
    m_authWindow->show();
}

void TSClient::onAuthFinished(bool success, AuthToken token, QString reason)
{
    m_authenticated = success;
    m_authInProgress = false;

    if (success) {
        bool stored = AuthToken::storeToSettings(token);
        Q_ASSERT(stored);
        qDebug(TSClientLog) << Q_FUNC_INFO <<
            "Auth successful : " << reason;
    } else {
        qDebug(TSClientLog) << Q_FUNC_INFO <<
            "Auth unsucessful : " << reason;
    }
    emit authStateChanged(m_authenticated, reason);
}

void TSClient::onAuthWindowDestroyed()
{
    // TODO race contition possible?
    m_authWindow = nullptr;
}
#endif


QNetworkRequest TSClient::buildRefreshTokenRequest()
{
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
                                            const QString &refreshToken)
{
    QUrlQuery query;
    query.addQueryItem("grant_type", "refresh_token");
    query.addQueryItem("client_id", clientId);
    query.addQueryItem("client_secret", clientSecret);
    query.addQueryItem("refresh_token", refreshToken);
    return query.toString(QUrl::FullyEncoded).toUtf8();
}

void TSClient::refreshAsyncAccessToken()
{
    Q_ASSERT_X(QThread::currentThread() == m_thread, qPrintable(QThread::currentThread()->objectName()), "Only TSClient thread can call this function");
    Q_ASSERT_X(m_asyncTokenRefreshRequestId == 0, Q_FUNC_INFO, "A refresh token request is already ongoing");
    Q_ASSERT_X(m_authInProgress == false, Q_FUNC_INFO, "Auth process is ongoing, cannot refresh token");
    Q_ASSERT_X(m_refreshInProgress == false, Q_FUNC_INFO, "A refresh token is already in progress");
    Q_ASSERT(m_authToken.isValid());
    Q_ASSERT(m_clientToken.isValid());
    // Note we don't check if authToken is expired, as it can be logically both

    m_refreshInProgress = true;

    qCDebug(TSClientLog) << "Starting an ASYNC token refresh request";


    // Build the request and query using our static helper methods
    QNetworkRequest request = buildRefreshTokenRequest();
    QByteArray postData     = buildRefreshTokenQuery(m_clientToken.getClientId(), m_clientToken.getClientSecret(), m_authToken.getRefreshToken());

    // Make the POST request
    m_asyncTokenRefreshRequestId = fetchAsync(request,
                                            static_cast<RequestTypeInt>(RequestType::GetRefreshAccessToken),
                                            HttpMethod::POST,
                                            postData);

    Q_ASSERT(m_asyncTokenRefreshRequestId > 0);
}

void TSClient::onAsyncRefreshTokenFinished(RESTClient::requestID_t requestID, RequestStatus status, AuthToken newToken)
{
    Q_ASSERT_X(m_asyncTokenRefreshRequestId == requestID, "token refresh", "Stored refresh request ID does not match the finished one");

    m_asyncTokenRefreshRequestId = 0;
    m_refreshInProgress = false;

    if (status == RequestStatus::ERROR) {
        qCCritical(TSClientLog) << "Refresh request in error";
        Q_ASSERT(false);
        return;
    } else if (status == RequestStatus::TIMEOUT) {
        qCWarning(TSClientLog) << "Received refreshed token timeout";

        // Retry in one second
        QTimer::singleShot(1000, this, [this]() {
            refreshAsyncAccessToken();
        });

        return;
    }
    
    Q_ASSERT(status == RequestStatus::SUCCESS);

    m_authenticated = newToken.isValidRefreshedToken() && !newToken.isExpired();;

    if (m_authenticated == false) {
        emit authStateChanged(false, "Received refreshed token invalid");

        qCCritical(TSClientLog) << "Received refreshed token invalid";

        //TODO retry

        return;
    }

    qCInfo(TSClientLog) << "Successful auth token refresh";

    // For some reason (security maybe) the new token return doesn't contain the refresh_key
    // All other fields are good (which is why it needs a special isValidRefreshedToken()
    // methods that does like isValid(), but omits the refresh_token field)
    // Now, we want to store this new token on disk, but we first need to retreive the
    // refresh_token from the actual token, stick it in there then save.
    AuthToken validNewToken(newToken); 
    
    validNewToken.setRefreshToken(m_authToken.getRefreshToken());

    AuthToken::storeToSettings(validNewToken);

    m_authToken = validNewToken;

    RESTClient::setAPIKey(m_authToken.getAccessToken());

    // Kick a new refresh in 20min - 5s
    {
        int secondsToNextRefreshRequest = m_authToken.secondsToNextRefreshRequest();
        // Logically if we are here this HAS to be t least 1s
        Q_ASSERT(secondsToNextRefreshRequest > 1 && secondsToNextRefreshRequest <= 1195);

        qCDebug(TSClientLog) << "Programming the next refresh in " << secondsToNextRefreshRequest << " seconds";

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


void TSClient::emitSignalDemuxer(RequestTypeBase_t type, const QJsonDocument &doc, RESTClient::requestID_t requestID, RequestStatus status, void *optArg)
{
    RequestType_t requestType = static_cast<RequestType_t>(type);
    QJsonObject obj = doc.object();

    Q_ASSERT(requestID > 0);

    if (status != RequestStatus::SUCCESS) {
        qCCritical(TSClientLog) << Q_FUNC_INFO << "Async operation not completed";
    }

    switch(requestType) {

        case RequestType::None:
            Q_ASSERT_X(0,"","Should not be None anymore");
            break;

        case RequestType::GetAccounts:
        {
            QVector<Account> results;

            if (status == RequestStatus::SUCCESS) {
                const QJsonArray accountsArray = obj["Accounts"].toArray();
            
                for (const QJsonValue &json: accountsArray) {
                    results.push_back(Account(json.toObject()));
                }
            }

            emit receivedAsyncGetAccounts(requestID, status, results);
            break;
        }

        case RequestType::GetBalances:
        {
            QVector<Balance> results;

            if (status == RequestStatus::SUCCESS) {
                const QJsonArray balancesArray = obj["Balances"].toArray();

                for (const QJsonValue &json: balancesArray) {
                    results.push_back(Balance(json.toObject()));
                }
            }

            emit receivedAsyncGetBalances(requestID, status, results);
            break;
        }

        case RequestType::GetBars:
        {
            QVector<Bar> results;

            Q_ASSERT(optArg != nullptr);
            QString* symbol = static_cast<QString*>(optArg);

            if (status == RequestStatus::SUCCESS) {
                const QJsonArray barsArray = obj["Bars"].toArray();

                for (const QJsonValue &json: barsArray) {
                    results.push_back(Bar(json.toObject()));
                }
            }

            emit receivedAsyncGetBars(requestID, status, *symbol, results);

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

            if (status == RequestStatus::SUCCESS) {
                result = PlaceOrderResult(doc.object());
            }

            emit receivedAsyncPlaceOrder(requestID, status, result);

            break;
        }

        case RequestType::CancelOrder:
        {
            CancelOrderResult result;

            if (status == RequestStatus::SUCCESS) {
                result = CancelOrderResult(doc.object());
            }

            emit receivedAsyncCancelOrder(requestID, status, result);

            break;
        }

        case RequestType::GetRefreshAccessToken:
        {
            AuthToken token;
            if (status == RequestStatus::SUCCESS) {
                token = AuthToken::receiveAuthToken(obj);
            }
            // No emit on purpose, this is calling a private function of this class
            onAsyncRefreshTokenFinished(requestID, status, token);
            break;
        }

    default:
        Q_UNREACHABLE();
        break;
    }
}

void TSClient::closeStream(Stream* const stream)
{
    Q_ASSERT(stream != nullptr);
    Q_ASSERT_X(QThread::currentThread() != m_thread, Q_FUNC_INFO, "TSClient object cannot call this function itself");

    QMetaObject::invokeMethod(this,
        [this, &stream]()
        {
            bool removed = m_streams.remove(stream->getNetworkReply());
            Q_ASSERT(removed); // The stream was likely already closed, or a bad pointer was passed

            RESTClient::closeStream(static_cast<void*>(stream));

            delete stream;
            
            // Emit signal that stream count has changed
            emit streamCountChanged(m_streams.size());
        },
    Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread finishes executing this lambda

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closed Stream " << static_cast<void*>(stream);
}











size_t TSClient::getAccountsAsync(const std::chrono::milliseconds &timeout)
{
    Q_ASSERT_X(QThread::currentThread() != m_thread, qPrintable(QThread::currentThread()->objectName()), "TSClient object cannot call this function itself");

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching accounts async";

    QNetworkRequest request = buildRequest(ENDPOINT_GET_ACCOUNTS);

    return fetchAsync(request, static_cast<RequestTypeInt>(RequestType::GetAccounts));
}

size_t TSClient::getBalancesAsync(const QString &accounts, const std::chrono::milliseconds &timeout)
{
    Q_ASSERT_X(QThread::currentThread() != m_thread, qPrintable(QThread::currentThread()->objectName()), "TSClient object cannot call this function itself");

    QNetworkRequest request = buildRequest(QString(ENDPOINT_GET_BALANCES).arg(accounts));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching Balances";

    return fetchAsync(request, static_cast<RequestTypeInt>(RequestType::GetBalances));
}

size_t TSClient::getBarsAsync(const QString &symbol,
                           unsigned int interval,
                           Bar::BarUnit unit,
                           unsigned int barsback,
                           Bar::BarSessionTemplate sessionTemplate,
                           QDateTime firstDate,
                           QDateTime lastDate,
                           const std::chrono::milliseconds &timeout)
{
    Q_ASSERT_X(QThread::currentThread() != m_thread, qPrintable(QThread::currentThread()->objectName()), "TSClient object cannot call this function itself");

    // Interval that each bar will consist of - for minute bars, the number of minutes aggregated in a single bar. For bar units other than minute, value must be 1.
    if (unit == Bar::BarUnit::Minute) {Q_ASSERT(interval >= 1);}
    else { Q_ASSERT(interval == 1);}
    Q_ASSERT(barsback <= 57600);

    if (barsback > 0) Q_ASSERT(firstDate == QDateTime());

    QUrlQuery query = Bar::buildUrlQuery(interval, unit, barsback, sessionTemplate, firstDate, lastDate);

    Q_ASSERT(!symbol.isEmpty());

    QNetworkRequest request = buildRequest(QString(ENDPOINT_GET_BARS).arg(symbol), query);

    void* arg = static_cast<void*> (new QString(symbol));
    Q_CHECK_PTR(arg);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching Bars for symbols : " << symbol;

    return fetchAsync(request, static_cast<RequestTypeInt>(RequestType::GetBars), HttpMethod::GET, QByteArray(), arg);
}


size_t TSClient::getQuoteSnapshotsAsync(QString &symbols, const std::chrono::milliseconds &timeout)
{
    Q_ASSERT_X(QThread::currentThread() != m_thread, qPrintable(QThread::currentThread()->objectName()), "TSClient object cannot call this function itself");
    Q_ASSERT(!symbols.isEmpty());

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching quotes for symbols : " << symbols;

    QNetworkRequest request = buildRequest(QString(ENDPOINT_GET_QUOTE_SNAPSHOTS).arg(symbols));

    return fetchAsync(request, static_cast<RequestTypeInt>(RequestType::GetQuoteSnapshots));
}

size_t TSClient::placeOrderAsync(const PlaceOrderRequest &order, const std::chrono::milliseconds &timeout)
{
    Q_ASSERT_X(QThread::currentThread() != m_thread, qPrintable(QThread::currentThread()->objectName()), "TSClient object cannot call this function itself");
    Q_ASSERT(order.isValid());

    QNetworkRequest request = buildRequest(ENDPOINT_PLACE_ORDER);

    QByteArray postData = QJsonDocument(order.toJson()).toJson(QJsonDocument::Compact);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Placing order async";

    return fetchAsync(request,
                      static_cast<RequestTypeInt>(RequestType::PlaceOrder),
                      HttpMethod::POST,
                      postData);
}

size_t TSClient::cancelOrderAsync(const QString &orderID, const std::chrono::milliseconds &timeout)
{
    Q_ASSERT_X(QThread::currentThread() != m_thread, qPrintable(QThread::currentThread()->objectName()), "TSClient object cannot call this function itself");
    Q_ASSERT(!orderID.isEmpty());
    Q_ASSERT(QRegularExpression("^[0-9]+$").match(orderID).hasMatch());

    QNetworkRequest request = buildRequest(QString(ENDPOINT_CANCEL_ORDER).arg(orderID));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Cancel order async";

    return fetchAsync(request,
                      static_cast<RequestTypeInt>(RequestType::CancelOrder),
                      HttpMethod::DELETE);
}

StreamPositions *TSClient::openStreamPositions(QString &accountID, bool changes)
{
    Q_ASSERT(accountID.length() >= 8); // normal account numbers have 8 digits, sim have additional letters

    const QString endpoint = QString(ENDPOINT_STREAM_POSITIONS).arg(accountID);

    QUrlQuery query;
    query.addQueryItem("changes", changes? "true":"false");

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamPositions";

    return openStream<StreamPositions>("NOSYMBOL", endpoint, query, accountID);
}

void TSClient::closeStreamPositions(StreamPositions *stream)
{
    Q_ASSERT(stream != nullptr);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closing StreamPositions " << static_cast<void*>(stream);

    TSClient::closeStream(stream);
}

StreamOrders* TSClient::openStreamOrders(QString &accountID) {
    Q_ASSERT(accountID.length() >= 8); // normal account numbers have 8 digits, sim have additional letters

    const QString endpoint = QString(ENDPOINT_STREAM_ORDERS).arg(accountID);

    QUrlQuery query;

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamOrders";

    return openStream<StreamOrders>("NOSYMBOL", endpoint, query, accountID);
}

void TSClient::closeStreamOrders(StreamOrders* stream) {
    Q_ASSERT(stream != nullptr);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closing StreamOrders " << static_cast<void*>(stream);

    TSClient::closeStream(stream);
}

StreamBars *TSClient::openStreamBars(const QString &symbol,
                                     unsigned int interval,
                                     Bar::BarUnit unit,
                                     unsigned int barsback,
                                     Bar::BarSessionTemplate sessionTemplate,
                                     bool mock)
{
    // Interval that each bar will consist of - for minute bars, the number of minutes aggregated in a single bar. For bar units other than minute, value must be 1.
    if (unit == Bar::BarUnit::Minute) {Q_ASSERT(interval >= 1);}
    else { Q_ASSERT(interval == 1);}
    Q_ASSERT(barsback <= 57600);

    const QString endpoint = ENDPOINT_STREAM_BARS;

    QUrlQuery query = Bar::buildUrlQuery(interval, unit, barsback, sessionTemplate);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamBars";

    return openStream<StreamBars>(symbol, endpoint, query, symbol);
}

void TSClient::closeStreamBars(StreamBars *stream)
{
    Q_ASSERT(stream != nullptr);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closing StreamBars " << static_cast<void*>(stream);

    TSClient::closeStream(stream);
}

StreamMarketDepthQuote* TSClient::openStreamMarketDepthQuote(const QString &symbol, unsigned int depth)
{
    Q_ASSERT(depth >= 1 && depth <= 20);

    const QString endpoint = ENDPOINT_STREAM_MARKET_DEPTH_QUOTE;
    QUrlQuery query;
    query.addQueryItem("maxlevels", QString::number(depth));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamMarketDepthQuote";

    return openStream<StreamMarketDepthQuote>(symbol, endpoint, query, symbol);
}

void TSClient::closeStreamMarketDepthQuote(StreamMarketDepthQuote *stream)
{
    Q_ASSERT(stream != nullptr);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closing StreamMarketDepthQuote " << static_cast<void*>(stream);

    TSClient::closeStream(stream);
}