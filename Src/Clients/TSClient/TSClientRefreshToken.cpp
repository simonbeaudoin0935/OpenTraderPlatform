#include "TSClient.h"
#include "SecureStorage.h"


// Launches authentication GUI dialog.
// We will receive a signal when the process finishes
void TSClient::launchAuthProcess()
{

    OBJ_ASSUME_FALSE(m_authInProgress);

    m_authInProgress = true;

    // Emit signal to update UI that authentication is starting
    emit authStateChanged(false, AuthStateReason::Connecting, "Connecting...");

    m_authHandler = new GUIAuthHandler();
    connect(m_authHandler, &GUIAuthHandler::authFinished, this, &TSClient::onAuthFinished);
    connect(m_authHandler, &QObject::destroyed, this, &TSClient::onAuthHandlerDestroyed);
    m_authHandler->show();
}

void TSClient::onAuthFinished(bool success, AuthToken token, QString reason)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &m_thread);
    if (!success)
    {
        finishAuth(false, reason);
        return;
    }

    // Keep m_authInProgress set until the client credentials saved by the login dialog are reloaded.
    auto* storage = new SecureStorage(this);
    ClientToken::loadFromSettingsAsync(*storage,
                                       [this, storage, token, reason](const ClientToken& p_clientToken)
                                       {
                                           storage->deleteLater();
                                           m_clientToken = p_clientToken;
                                           if (!m_clientToken.isValid())
                                           {
                                               finishAuth(false,
                                                          QString("Could not reload TradeStation credentials "
                                                                  "from the %1")
                                                              .arg(SecureStorage::backendName()));
                                               return;
                                           }

                                           m_authToken = token;
                                           m_apiKey = m_authToken.getAccessToken();
                                           scheduleNextRefreshFromCurrentToken("onAuthFinished");
                                           finishAuth(true, reason);
                                       });
}

void TSClient::finishAuth(const bool p_success, const QString& p_reason)
{
    m_authenticated = p_success;
    m_authInProgress = false;

    if (p_success)
    {
        qCInfo(TSClientLog) << Q_FUNC_INFO << "Auth successful : " << p_reason;
        emit authStateChanged(true, AuthStateReason::ValidToken, p_reason);
        return;
    }

    qCWarning(TSClientLog) << Q_FUNC_INFO << "Auth unsucessful : " << p_reason;
    emit authStateChanged(false, AuthStateReason::AuthFailed, p_reason);
}

void TSClient::persistRefreshedToken()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &m_thread);
    OBJ_ASSUME_TRUE(m_refreshInProgress);

    auto* storage = new SecureStorage(this);
    AuthToken::storeToSettingsAsync(
        *storage,
        m_authToken,
        [this, storage](const bool p_stored)
        {
            storage->deleteLater();
            m_refreshInProgress = false;

            if (!p_stored)
            {
                m_authenticated = false;
                const QString storageError = QString("Could not save refreshed tokens in the %1; "
                                                     "unlock the selected storage backend and reconnect")
                                                 .arg(SecureStorage::backendName());
                qCWarning(TSClientLog) << storageError;
                emit authStateChanged(false, AuthStateReason::AuthFailed, storageError);
                return;
            }

            // Kick a new refresh in 20min - 5s
            scheduleNextRefreshFromCurrentToken("refreshAccessToken/success");

            QTimer::singleShot(
                1000,
                this,
                [this]()
                {
                    // Based on observation, if we propagate the good new immediately and start
                    // making calls, the remote server will send us back an error 401 (unauthenticated)
                    // for the first API call. Almost as if the refresh did not properly propagade in their system.
                    // Wait a second on our end before propagating the successful authentification as to delay
                    // making the first API call.
                    emit authStateChanged(true, AuthStateReason::RefreshSuccessful, "Auth token refresh successful");
                });
        });
}

void TSClient::onAuthHandlerDestroyed()
{
    // TODO race condition possible?
    m_authHandler = nullptr;
}


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

QByteArray
TSClient::buildRefreshTokenQuery(const QString& clientId, const QString& clientSecret, const QString& refreshToken)
{
    QUrlQuery query;
    query.addQueryItem("grant_type", "refresh_token");
    query.addQueryItem("client_id", clientId);
    query.addQueryItem("client_secret", clientSecret);
    query.addQueryItem("refresh_token", refreshToken);
    return query.toString(QUrl::FullyEncoded).toUtf8();
}

void TSClient::refreshAccessToken()
{
    // Only TSClient thread can call this function
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &m_thread);
    // Auth process is ongoing, cannot refresh token
    OBJ_ASSUME_FALSE(m_authInProgress);
    // A refresh token is already in progress
    OBJ_ASSUME_FALSE(m_refreshInProgress);
    if (!m_authToken.isValid() || !m_clientToken.isValid())
    {
        m_authenticated = false;
        qCWarning(TSClientLog) << "Skipping token refresh: credentials are unavailable";
        emit authStateChanged(false, AuthStateReason::AuthFailed, "Authenticate in Credentials before reconnecting");
        return;
    }
    // Note we don't check if authToken is expired, as it can be logically both

    m_refreshInProgress = true;

    qCDebug(TSClientLog) << "Starting an ASYNC token refresh request";

    // Emit signal to update UI that we're attempting to connect
    emit authStateChanged(false, AuthStateReason::Connecting, "Connecting...");


    // Build the request and query using our static helper methods
    const QNetworkRequest request = buildRefreshTokenRequest();
    const QByteArray postData = buildRefreshTokenQuery(m_clientToken.getClientId(),
                                                       m_clientToken.getClientSecret(),
                                                       m_authToken.getRefreshToken());

    QNetworkReply* reply = m_networkManager->post(request, postData);
    Q_CHECK_PTR(reply);

    auto b = connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply]() mutable
        {
            QByteArray rawData = reply->readAll();

            processNewAmountOfDataReceived(rawData.size());

            switch (reply->error())
            {
            // Happy path
            case QNetworkReply::NoError:
            {
                QJsonParseError parseError;
                QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

                if (parseError.error != QJsonParseError::NoError)
                {
                    qCCritical(TSClientLog) << "Failed to parse JSON:" << parseError.errorString();
                    QTimer::singleShot(1000, this, [this]() { refreshAccessToken(); });
                    break;
                }

                if (!doc.isObject())
                {
                    qCCritical(TSClientLog) << " : JSON is not an object";
                    QTimer::singleShot(1000, this, [this]() { refreshAccessToken(); });
                    break;
                }

                //TODO happy path
                AuthToken newToken = AuthToken::receiveRefreshedAuthToken(doc.object(), m_authToken.getRefreshToken());
                m_authenticated = newToken.isValid() && !newToken.isExpired();

                if (m_authenticated == false)
                {
                    qCCritical(TSClientLog) << "Received refreshed token invalid";

                    // Don't retry if the token is invalid - user needs to manually re-authenticate
                    emit authStateChanged(false, AuthStateReason::TokenExpired, "Token expired");
                    break;
                }

                qCInfo(TSClientLog) << "Successful auth token refresh";

                // Keep a rotated token in memory even if persistence fails.
                m_authToken = newToken;
                m_apiKey = m_authToken.getAccessToken();
                // m_refreshInProgress stays set until the async keyring write completes.
                reply->deleteLater();
                persistRefreshedToken();
                return;
            }

            // timeout
            case QNetworkReply::HostNotFoundError:
            case QNetworkReply::UnknownNetworkError:
            {
                qCCritical(TSClientLog) << ": refreshAccessToken(): Timeout with the reply: " << reply->errorString()
                                        << " : " << reply->error();
                QTimer::singleShot(1000, this, [this]() { refreshAccessToken(); });
                break;
            }

            case QNetworkReply::AuthenticationRequiredError:
            {
                m_authenticated = false;
                m_apiKey.clear();
                qCWarning(TSClientLog) << "Token refresh rejected; automatic retries stopped";
                emit authStateChanged(false,
                                      AuthStateReason::TokenExpired,
                                      "TradeStation rejected the refresh credentials; log in again");
                break;
            }

            // other errors
            default:
            {
                qCCritical(TSClientLog) << ": refreshAccessToken(): Error with reply: " << reply->errorString() << " : "
                                        << reply->error();
                QTimer::singleShot(1000, this, [this]() { refreshAccessToken(); });
                break;
            }
            };

            m_refreshInProgress = false;
            reply->deleteLater();
        });
    OBJ_ASSUME_TRUE(b);

    qCDebug(TSClientLog) << "Sent refreshAccessToken() to Network Manager";

    //TODO store the promise to be able to act on it
}
