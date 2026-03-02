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
                // Skip QNetworkAccessManager - delete it AFTER streams
                if (child == m_networkManager)
                    continue;

                child->deleteLater(); // Schedule Stream deletion
            }
        },
        Qt::BlockingQueuedConnection);

    // Process events to delete Streams (while QNetworkAccessManager still valid)
    QMetaObject::invokeMethod(this, []() { QCoreApplication::processEvents(); }, Qt::BlockingQueuedConnection);

    // Now delete QNetworkAccessManager after all Streams are gone
    QMetaObject::invokeMethod(this, [this]() { m_networkManager->deleteLater(); }, Qt::BlockingQueuedConnection);

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

TSClient::TSClient()
    : m_authenticated(false), m_refreshInProgress(false), m_networkManager(new QNetworkAccessManager(this))
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

        // Logically if we got here, there HAS to be at least 5 seconds left.
        // Compare against 4 just in case we are at 5 seconds left
        ASSUME_GT(secsUntilExpiration, 4);

        INFO << "Auth token is valid and already not expired, still has " << secsUntilExpiration << "second left to it";

        int secondsToNextRefreshRequest = m_authToken.secondsToNextRefreshRequest();

        // Logically if we are here this HAS to be at least 1s
        ASSUME_GTE(secondsToNextRefreshRequest, 1);

        DEBUG << "Initiating a refresh in " << secondsToNextRefreshRequest << "seconds";

        // Launch a request in X seconds from now.
        QTimer::singleShot(1000 * secondsToNextRefreshRequest, this, [this]() { refreshAccessToken(); });

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

QNetworkRequest TSClient::buildNetworkRequest(const QString& endpoint, const QUrlQuery& query) const
{
    OBJ_ASSUME_FALSE(m_baseUrl.isEmpty());
    OBJ_ASSUME_FALSE(m_apiKey.isEmpty());
    OBJ_ASSUME_FALSE(endpoint.isEmpty());
    // Ensure no unformatted parameters remain
    OBJ_ASSUME_FALSE(endpoint.contains(QRegularExpression("%\\d+")));

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
    if (bytesReceived == 0)
    {
        CRITICAL << "No data received in this readyRead/finished";
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
