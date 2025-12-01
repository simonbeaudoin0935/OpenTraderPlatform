#include "TSClient.h"


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
    const QNetworkRequest request = buildRefreshTokenRequest();
    const QByteArray     postData = buildRefreshTokenQuery(m_clientToken.getClientId(), m_clientToken.getClientSecret(), m_authToken.getRefreshToken());

    // Make the POST request
    m_asyncTokenRefreshRequestId = sendAsyncRequest(request,
                                                    AsyncRequestType_t::GetRefreshAccessToken,
                                                    HttpMethod::POST,
                                                    postData);

    Q_ASSERT(m_asyncTokenRefreshRequestId > 0);
}

void TSClient::processAsyncRefreshTokenFinished(TSClient::AsyncRequestID_t requestID, TSClient::AsyncRequestStatus_e status, AuthToken newToken)
{
    Q_ASSERT_X(m_asyncTokenRefreshRequestId == requestID, "token refresh", "Stored refresh request ID does not match the finished one");

    m_asyncTokenRefreshRequestId = 0;
    m_refreshInProgress = false;

    if (status == AsyncRequestStatus_e::ERROR) {
        qCCritical(TSClientLog) << "Refresh request in error";
        Q_ASSERT(false);
        return;
    } else if (status == AsyncRequestStatus_e::TIMEOUT) {
        qCWarning(TSClientLog) << "Received refreshed token timeout";

        // Retry in one second
        QTimer::singleShot(1000, this, [this]() {
            refreshAsyncAccessToken();
        });

        return;
    }
    
    Q_ASSERT(status == AsyncRequestStatus_e::SUCCESS);

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

    m_apiKey = m_authToken.getAccessToken();

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

