#include <QNetworkAccessManager>
#include <QThread>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QDebug>
#include <QCoreApplication>

#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"
#include "CONSTANTS.h"
#include "MainApp.h"
#include "ThreadNames.h"
#include "SecureStorage.h"
#include "AuthenticatedNetworkAccessManager.h"
#include "Stream/MockNetworkAccessManager.h"

#define LOGGING_CATEGORY TSClientLog

Q_LOGGING_CATEGORY(TSClientLog, "TSClient")


// Initialize static member outside class
TSClient* TSClient::m_instance = nullptr;

TSClient* TSClient::getInstance()
{
    if (m_instance == nullptr)
    {
        qCDebug(TSClientLog) << "Singleton instance created";
        m_instance = new TSClient();
    }
    return m_instance;
}

void TSClient::destroyInstance()
{
    ASSUME_TRUE(m_instance != nullptr);
    qCDebug(TSClientLog) << "Destroying singleton instance";
    delete m_instance;
    m_instance = nullptr;
}

TSClient::~TSClient()
{
    qCDebug(TSClientLog) << "TSClient shutting down";
    m_shuttingDown.store(true, std::memory_order_release);

    // Thread affinity assertion - destructor must be called from main thread
    OBJ_ASSUME_EQUAL(QThread::currentThread(), QCoreApplication::instance()->thread());

    // CRITICAL: Delete Stream child objects BEFORE QNetworkAccessManager
    // Streams need a valid QNetworkAccessManager to abort their network replies
    QMetaObject::invokeMethod(
        this,
        [this]()
        {
            // This lambda executes on TSClient thread
            const QObjectList childrenList = children();
            for (QObject* child: childrenList)
            {
                // Skip TradeStation managers (active and retired) - delete them AFTER streams
                if (qobject_cast<AuthenticatedNetworkAccessManager*>(child) != nullptr)
                    continue;

                child->deleteLater(); // Schedule Stream deletion
            }
        },
        Qt::BlockingQueuedConnection);

    // Process events to delete Streams (while QNetworkAccessManager still valid)
    QMetaObject::invokeMethod(this, []() { QCoreApplication::processEvents(); }, Qt::BlockingQueuedConnection);

    // Now delete the QNetworkAccessManagers after all Streams are gone
    QMetaObject::invokeMethod(
        this,
        [this]()
        {
            const auto managers = findChildren<AuthenticatedNetworkAccessManager*>(Qt::FindDirectChildrenOnly);
            for (AuthenticatedNetworkAccessManager* manager: managers)
            {
                manager->deleteLater();
            }
        },
        Qt::BlockingQueuedConnection);

    // Process final deleteLater
    QMetaObject::invokeMethod(this, []() { QCoreApplication::processEvents(); }, Qt::BlockingQueuedConnection);

    // Now safe to stop thread (all objects deleted in correct order)
    m_thread.quit();

    // Wait for thread to finish (with timeout)
    if (!m_thread.wait(5000))
    {
        qCWarning(TSClientLog) << "TSClient thread did not finish within timeout, terminating";
        m_thread.terminate();
        m_thread.wait();
    }
}

TSClient::TSClient() : m_authenticated(false), m_refreshInProgress(false), m_networkManager(createNetworkManager())
{
    m_thread.setObjectName("TSClientThread");
    this->moveToThread(&m_thread);

    // Set up base URL based on trading mode
    m_baseUrl.setScheme(TSClientHosts::SCHEME);
    const char* host =
        (MainApp::getTradingMode() == TradingMode::Sim) ? TSClientHosts::SIM_HOST : TSClientHosts::LIVE_HOST;
    m_baseUrl.setHost(host);
    m_baseUrl.setPath(TSClientHosts::API_VERSION);

    qInfo() << "TSClient connecting to:" << m_baseUrl.host();

    if (SecureStorage::activeBackend() == SecureStorage::Backend::YubiKey)
    {
        connect(&m_thread, &QThread::started, this, &TSClient::loadStartupCredentials);
    }
    else
    {
        loadStartupCredentials();
    }
}

void TSClient::loadStartupCredentials()
{
    m_clientToken = ClientToken::loadFromSettings();
    m_authToken = AuthToken::loadFromSettings();


    // If the token is invalid/absent, we need to perform an authentification with the popup
    if (!m_clientToken.isValid() || !m_authToken.isValid())
    {
        qCWarning(TSClientLog) << Q_FUNC_INFO
                               << "Auth token or Client token is invalid/absent, will need an authentification process";

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
    else if (m_authToken.isValid() && m_authToken.isExpired())
    {

        INFO << "Auth token is valid but expired, perform a refresh now.";

        // Schedule a refresh for when the thread starts
        QTimer::singleShot(0, this, [this]() { refreshAccessToken(); });
    }

    // If the token is valid and not expired (has at least 5s left in it,
    // start using it
    else if (m_authToken.isValid() && !m_authToken.isExpired())
    {

        m_authenticated = true;

        m_apiKey = m_authToken.getAccessToken();

        int secsUntilExpiration = m_authToken.secondsUntilExpiration();

        if (secsUntilExpiration <= AuthConstants::EXPIRY_BUFFER_SECONDS)
        {
            qCWarning(TSClientLog) << "Token is very close to refresh threshold at startup (" << secsUntilExpiration
                                   << "s left), scheduling immediate refresh";
        }

        INFO << "Auth token is valid and already not expired, still has " << secsUntilExpiration
             << " second left to it";
        scheduleNextRefreshFromCurrentToken("startup");

        // Schedule an emition for when the event loop is started
        QTimer::singleShot(
            0,
            this,
            [this]() { emit authStateChanged(true, AuthStateReason::ValidToken, "Auth token valid and not expired"); });
    }
    else
    {
        Q_UNREACHABLE();
    }
}

void TSClient::scheduleNextRefreshFromCurrentToken(const char* p_context)
{
    OBJ_ASSUME_TRUE(m_authToken.isValid());

    const int secondsToNextRefreshRequest = m_authToken.secondsToNextRefreshRequest();
    OBJ_ASSUME_LTE(secondsToNextRefreshRequest, AuthConstants::MAX_SECONDS_TO_NEXT_REFRESH_REQUEST);

    if (secondsToNextRefreshRequest <= 0)
    {
        qCWarning(TSClientLog) << p_context << ": token is already within refresh buffer ("
                               << secondsToNextRefreshRequest << "s), refreshing immediately";
        QTimer::singleShot(0, this, [this]() { refreshAccessToken(); });
        return;
    }

    qCDebug(TSClientLog) << p_context << ": scheduling next refresh in " << secondsToNextRefreshRequest << " seconds";
    QTimer::singleShot(1000 * secondsToNextRefreshRequest, this, [this]() { refreshAccessToken(); });
}

AuthenticatedNetworkAccessManager* TSClient::createNetworkManager()
{
    auto* manager = new AuthenticatedNetworkAccessManager(
        [this](const QNetworkRequest& p_request)
        {
            if (p_request.url() == buildRefreshTokenRequest().url())
            {
                return m_clientToken.isValid() && m_authToken.isValid() && !m_authInProgress;
            }
            const bool authorized =
                m_authenticated && !m_apiKey.isEmpty() && m_authToken.isValid() && !m_authToken.isExpired();
            if (!authorized && m_authenticated)
            {
                m_authenticated = false;
                emit authStateChanged(false,
                                      AuthStateReason::Connecting,
                                      "TradeStation requests paused pending authentication or token refresh");
            }
            return authorized;
        },
        this);
    Q_CHECK_PTR(manager);

    const auto c = connect(manager,
                           &AuthenticatedNetworkAccessManager::connectionStalled,
                           this,
                           &TSClient::onNetworkConnectionStalled);
    OBJ_ASSUME_TRUE(c);

    return manager;
}

QNetworkAccessManager* TSClient::activeNetworkManager() const
{
    if (m_mode == Mode::Replay && m_mockNetworkManager != nullptr)
    {
        return m_mockNetworkManager;
    }
    return m_networkManager;
}

/*
 * All TradeStation requests share one HTTP/2 connection per QNetworkAccessManager. After some server
 * GOAWAY frames, that connection keeps serving established streams but never answers new requests,
 * so every reconnect/REST call hangs. Qt cannot open a second connection to the same host from the same
 * manager, so new traffic is moved to a fresh manager (fresh connection); the old one is retired and
 * deletes itself once the streams still using it are gone.
 */
void TSClient::onNetworkConnectionStalled(const QString& p_description)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), this->thread());

    auto* const reporter = qobject_cast<AuthenticatedNetworkAccessManager*>(sender());
    OBJ_ASSUME_DIFF(reporter, nullptr);

    // Stalls reported by an already-retired manager were handled when it was retired
    if (reporter != m_networkManager || m_shuttingDown.load(std::memory_order_acquire))
    {
        return;
    }

    CRITICAL << "TradeStation HTTP/2 connection stalled (" << p_description
             << "); moving new requests to a fresh connection";

    AuthenticatedNetworkAccessManager* const stalledManager = m_networkManager;
    m_networkManager = createNetworkManager();
    stalledManager->retire();
}

QNetworkRequest TSClient::buildStreamRequest(const QString& endpoint, const QUrlQuery& query) const
{
    QNetworkRequest request = buildNetworkRequest(endpoint, query);
    request.setAttribute(AuthenticatedNetworkAccessManager::StreamingRequestAttribute, true);
    return request;
}

QNetworkRequest TSClient::buildNetworkRequest(const QString& endpoint, const QUrlQuery& query) const
{
    OBJ_ASSUME_FALSE(m_baseUrl.isEmpty());
    // In replay mode the mock network manager handles the request without real credentials
    OBJ_ASSUME_FALSE(endpoint.isEmpty());
    // Ensure no unformatted parameters remain
    OBJ_ASSUME_FALSE(endpoint.contains(QRegularExpression("%\\d+")));

    QUrl url(m_baseUrl);

    url.setPath(url.path() + endpoint);
    url.setQuery(query);

    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (m_mode == Mode::Replay || !m_apiKey.isEmpty())
    {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    }

    return request;
}

void TSClient::processNewAmountOfDataReceived(size_t bytesReceived)
{
    if (bytesReceived == 0)
    {
        // Empty bodies occur on locally blocked requests and network failures.
        // The request handler reports the actual failure; byte accounting is not an error.
        return;
    }
    else
    {
        m_totalDataReceivedBytes += static_cast<qsizetype>(bytesReceived);

        //DEBUG << "Received " << bytesReceived << " bytes, total now " << m_totalDataReceivedBytes << " bytes";
        emit totalDataReceivedBytesIncreased(m_totalDataReceivedBytes);
    }
}


#ifdef UNIT_TESTING
bool TSClient::isCleanedUp()
{
    return (StreamPositions::getNumberOfPositionStreams() == 0 && StreamOrders::getNumberOfOrderStreams() == 0);
}
#endif
