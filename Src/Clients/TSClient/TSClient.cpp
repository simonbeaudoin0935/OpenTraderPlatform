#include <QNetworkAccessManager>
#include <QThread>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QDebug>

#include "TSClient.h"
#include "Logging.h"

#define LOGGING_CATEGORY TSClientLog

Q_LOGGING_CATEGORY(TSClientLog, "TSClient")

#define BASE_URL_SCHEME                "https"
#define BASE_URL_HOST_SIMULATION       "sim-api.tradestation.com"
#define BASE_URL_HOST_VERSION          "/v3/"



// Initialize static member outside class
TSClient* TSClient::m_instance = nullptr;
    
TSClient* TSClient::getInstance()
{
    if (m_instance == nullptr) {
        qCDebug(TSClientLog) << "Singleton instance created";
        m_instance = new TSClient();
    }
    return m_instance;
}

TSClient::~TSClient()
{
    Q_ASSERT(false); // Destructor should never be called for singleton
}

TSClient::TSClient() :
    QObject(),
    m_thread(new QThread()),
    m_networkManager(new QNetworkAccessManager(this)),
    m_authenticated(false),
    m_refreshInProgress(false)
{
    this->moveToThread(m_thread);

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

        INFO << "Auth token is valid but expired, perform a refresh now.";

        // Schedule a refresh for when the thread starts
        QTimer::singleShot(0, this, [this]() {
            refreshAccessToken();
        });
    }

    // If the token is valid and not expired (has at least 5s left in it,
    // start using it
    else if (m_authToken.isValid() && !m_authToken.isExpired()) {

        m_authenticated = true;

        m_apiKey = m_authToken.getAccessToken();

        int secsUntilExpiration = m_authToken.secondsUntilExpiration();

        // Logically if we got here, there HAS to be at least 5 seconds left.
        // Compare against 4 just in case we are at 5 seconds left
        Q_ASSERT(secsUntilExpiration > 4);

        INFO << "Auth token is valid and already not expired, still has " << secsUntilExpiration  << "second left to it";

        int secondsToNextRefreshRequest = m_authToken.secondsToNextRefreshRequest();

        // Logically if we are here this HAS to be at least 1s
        Q_ASSERT(secondsToNextRefreshRequest >= 1);

        DEBUG << "Initiating a refresh in " << secondsToNextRefreshRequest << "seconds";

        // Launch a request in X seconds from now.
        QTimer::singleShot(1000 * secondsToNextRefreshRequest, this, [this]() {
            refreshAccessToken();
        });

        // Schedule an emition for when the event loop is started
        QTimer::singleShot(0, this, [this]() {
            emit authStateChanged(true, "Auth token valid and not expired");
        });

    } else {
        Q_UNREACHABLE();
    }
}

QNetworkRequest TSClient::buildNetworkRequest(const QString &endpoint, const QUrlQuery &query) const
{
    Q_ASSERT(!m_baseUrl.isEmpty());
    Q_ASSERT(!m_apiKey.isEmpty());
    Q_ASSERT(!endpoint.isEmpty());
    Q_ASSERT(!endpoint.contains(QRegularExpression("%\\d+"))); // Ensure no unformatted parameters remain

    QUrl url(m_baseUrl);

    url.setPath(url.path() + endpoint);
    url.setQuery(query);

    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());

    return request;
}

void TSClient::processNewAmountOfDataReceived(size_t bytesReceived)
{
    if (bytesReceived == 0) {
        CRITICAL << "No data received in this readyRead/finished";
    } else {
        m_totalDataReceivedBytes += bytesReceived;

        //DEBUG << "Received " << bytesReceived << " bytes, total now " << m_totalDataReceivedBytes << " bytes";
        emit totalDataReceivedBytesIncreased(m_totalDataReceivedBytes);
    }
}


#ifdef UNIT_TESTING
bool TSClient::isCleanedUp()
{
    // The pending requests tracking was removed from TSClient.
    // The isCleanedUp() check is now primarily about verifying no open streams remain.
    // For now, just check the stream count.
    return Stream::getNumberOpenStream() == 0;
}
#endif
