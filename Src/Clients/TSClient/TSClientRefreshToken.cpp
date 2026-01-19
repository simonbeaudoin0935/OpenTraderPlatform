#include "TSClient.h"


// Launches authentication UI (GUI dialog or TUI console prompts)
// We will receive a signal when the process finishes
void TSClient::launchAuthProcess()
{

    Q_ASSERT(m_authInProgress == false);

    m_authInProgress = true;

#ifdef GUI_ENABLED
    m_authHandler = new GUIAuthHandler();
    connect(m_authHandler, &GUIAuthHandler::authFinished, this, &TSClient::onAuthFinished);
    connect(m_authHandler, &QObject::destroyed, this, &TSClient::onAuthHandlerDestroyed);
    m_authHandler->show();
#else
    m_authHandler = new TUIAuthHandler();
    connect(m_authHandler, &TUIAuthHandler::authFinished, this, &TSClient::onAuthFinished);
    connect(m_authHandler, &QObject::destroyed, this, &TSClient::onAuthHandlerDestroyed);
    m_authHandler->startAuthentication();
#endif
}

void TSClient::onAuthFinished(bool success, AuthToken token, QString reason)
{
    m_authenticated = success;
    m_authInProgress = false;

    if (success)
    {
        bool stored = AuthToken::storeToSettings(token);
        Q_ASSERT(stored);

        // Update the TSClient's auth token and API key
        m_authToken = token;
        m_apiKey = m_authToken.getAccessToken();

        // Schedule the next token refresh (20 minutes - 5 seconds)
        int secondsToNextRefreshRequest = m_authToken.secondsToNextRefreshRequest();
        Q_ASSERT(secondsToNextRefreshRequest > 1 && secondsToNextRefreshRequest <= 1195);

        qCDebug(TSClientLog) << "Programming the next refresh in " << secondsToNextRefreshRequest << " seconds";

        QTimer::singleShot(1000 * secondsToNextRefreshRequest, this, [this]() { refreshAccessToken(); });

        qCInfo(TSClientLog) << Q_FUNC_INFO << "Auth successful : " << reason;
    }
    else
    {
        qCWarning(TSClientLog) << Q_FUNC_INFO << "Auth unsucessful : " << reason;
    }
    emit authStateChanged(m_authenticated, reason);
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
    Q_ASSERT_X(QThread::currentThread() == m_thread,
               qPrintable(QThread::currentThread()->objectName()),
               "Only TSClient thread can call this function");
    Q_ASSERT_X(m_authInProgress == false, Q_FUNC_INFO, "Auth process is ongoing, cannot refresh token");
    Q_ASSERT_X(m_refreshInProgress == false, Q_FUNC_INFO, "A refresh token is already in progress");
    Q_ASSERT(m_authToken.isValid());
    Q_ASSERT(m_clientToken.isValid());
    // Note we don't check if authToken is expired, as it can be logically both

    m_refreshInProgress = true;

    qCDebug(TSClientLog) << "Starting an ASYNC token refresh request";


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
                    qCCritical(TSClientLog) << "Content of the bad data : " << rawData;
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
                AuthToken newToken = AuthToken::receiveAuthToken(doc.object());
                m_authenticated = newToken.isValidRefreshedToken() && !newToken.isExpired();

                if (m_authenticated == false)
                {
                    emit authStateChanged(false, "Received refreshed token invalid");
                    qCCritical(TSClientLog) << "Received refreshed token invalid";

                    //TODO probably need more
                    QTimer::singleShot(1000, this, [this]() { refreshAccessToken(); });
                    break;
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

                    qCDebug(TSClientLog) << "Programming the next refresh in " << secondsToNextRefreshRequest
                                         << " seconds";

                    // Launch a request in X seconds from now.
                    QTimer::singleShot(1000 * secondsToNextRefreshRequest, this, [this]() { refreshAccessToken(); });
                }

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
                        emit authStateChanged(true, "Auth token refresh successful");
                    });
                break;
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
    Q_ASSERT(b);

    qCDebug(TSClientLog) << "Sent refreshAccessToken() to Network Manager";

    //TODO store the promise to be able to act on it
}