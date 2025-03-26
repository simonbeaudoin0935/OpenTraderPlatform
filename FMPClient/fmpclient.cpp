#include "fmpclient.h"
#include "qtestsupport_core.h"
#include <QNetworkAccessManager>
#include <QThread>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QDebug>
#ifdef UNIT_TESTING
#include <QtTest/QtTest>
#endif

// Define the logging category
Q_LOGGING_CATEGORY(FMPClientLog, "FMPClient")

// Initialize static member outside class
FMPClient* FMPClient::instance = nullptr;

QString FMPClient::apiKey = "";

FMPClient& FMPClient::getInstance() {

    if (instance == nullptr) {
        qCDebug(FMPClientLog) << "Singleton instance created";

        instance = new FMPClient();
    }
    return *instance;
}

void FMPClient::setAPIKey(const QString &apiKey)
{
    FMPClient::apiKey = apiKey;
    qCDebug(FMPClientLog) << "API key set to " << apiKey;
}

FMPClient::FMPClient() :
    thread(new QThread()),
    manager(new QNetworkAccessManager(this))
{
    qCDebug(FMPClientLog) << Q_FUNC_INFO << ": FMPClient created using KEY=" << apiKey;

    Q_ASSERT(!apiKey.isEmpty());

    thread->setObjectName("FPMClientThread");

    this->moveToThread(thread);

    connect(thread, &QThread::started, this, &FMPClient::onThreadStarted);
    connect(manager, &QNetworkAccessManager::finished, this, &FMPClient::onReplyFinished);
    thread->start();
}

FMPClient::~FMPClient() {
    qCDebug(FMPClientLog) << "Singleton instance destroyed";

    thread->quit();
    thread->wait();
}


QString FMPClient::buildUrlWithEndpoint(const QString &endpoint) const {
    return baseUrl + endpoint + "?" +
           QString("apikey=%1").arg(apiKey);
}

QString FMPClient::buildUrlWithEndpointAndSymbol(const QString &endpoint, const QString &symbol) const {
    return baseUrl + endpoint + "?" +
           QString("symbol=%1").arg(symbol) + "&" +
           QString("apikey=%1").arg(apiKey);
}

void FMPClient::fetchAsync(const QString &url, RequestType type) {
    // Invoke this method in the distinct thread of the FMPClient singleton.
    // Having ->moveToThread() the FMPClient to a dedicated thread makes that calling to 'this'
    // is actually invoking this method NOT in the caller's thread.
    QMetaObject::invokeMethod(this, [this, url, type]() {
        QNetworkReply *reply = manager->get(QNetworkRequest(url));

        QMutexLocker locker(&pendingRequestsMutex);
        pendingRequests[reply] = { .synchronicity = RequestSynchronicity::Async,
                                   .type = type,
                                   .completed = false,
                                   .jsonArray = nullptr};
    }, Qt::QueuedConnection);
}

void FMPClient::fetchAsyncQuote(const QString &symbol) {
    QString url = buildUrlWithEndpointAndSymbol("quote",symbol);

    fetchAsync(url, RequestType::Quote);
}

void FMPClient::fetchAsyncSharesFloat(const QString &symbol)
{
    QString url = buildUrlWithEndpointAndSymbol("shares-float",symbol);

    fetchAsync(url, RequestType::SharesFloat);
}

/*
 * Underlying function doing the fetching of the JSON common to all fetchXSync methods
 */
bool FMPClient::fetchSync(const QString &url, QJsonArray *&jsonArrayFromReplyToDelete) {
    QNetworkReply *reply = nullptr;

    // Invoke this method in the distinct thread of the FMPClient singleton.
    // Having ->moveToThread() the FMPClient to a dedicated thread makes that calling to 'this'
    // is actually invoking this method NOT in the caller's thread.
    QMetaObject::invokeMethod(this, [this, url, &reply]() {
        QNetworkRequest request(url);
        reply = manager->get(request);

        qCDebug(FMPClientLog) << Q_FUNC_INFO <<
            " : Thread [" << QThread::currentThread()->objectName() <<
            "] executed the queued GET request and registered the reply " << static_cast<void*>(reply) << " for later reception.";

        QMutexLocker locker(&pendingRequestsMutex);
        {
            pendingRequests[reply] = {RequestSynchronicity::Sync};
        }

    }, Qt::QueuedConnection);

    qCDebug(FMPClientLog) << Q_FUNC_INFO <<
        " : Thread [" << QThread::currentThread()->objectName() <<
        "] invoked the queued method to GET url " << url ;

    QMutexLocker locker(&pendingRequestsMutex);

    // Mutex is released while we wait
    if (!waitCondition.wait(&pendingRequestsMutex, fetchSyncTimeoutMs)) {
        // Mutex is re-aquired

        qCWarning(FMPClientLog) << Q_FUNC_INFO <<
            " : The condition wait timed out after " << fetchSyncTimeoutMs <<
            "ms. Removing the *reply " << static_cast<void*>(reply) << " from pendingRequests";

        // If we timed out before receiving the reply, we remove the *reply from the map.
        // This is important so that *if* the reply/error eventually arrives, its onReplyFinished()
        // processing will notice it is not in the map. This will signify to the onReplyFinished()
        // that there is nobody waiting for this reply anymore.
        bool was_removed = pendingRequests.remove(reply);

        // There is a logic problem if the *reply was not in the map at this point
        Q_ASSERT(was_removed);

        return false;
    }

    // At this point, we got notified by the waitCondition. The FMPClient thread MUST have executed
    // the invoked method above and populated the reply
    Q_ASSERT(reply != nullptr);

    // TODO asserts on the container here toooo

    if (pendingRequests.contains(reply) && pendingRequests[reply].completed) {
        jsonArrayFromReplyToDelete = pendingRequests[reply].jsonArray;

        qCDebug(FMPClientLog) << Q_FUNC_INFO << " : The *reply " << static_cast<void*>(reply) << " successfuly completed.";

        pendingRequests.remove(reply);

        return true;
    }

    jsonArrayFromReplyToDelete = nullptr;

    pendingRequests.remove(reply);

    qWarning() << Q_FUNC_INFO << " : The pending requests did not contain the reply, or was not marked as completed.";

    return false;
}

bool FMPClient::fetchSyncQuote(const QString &symbol, double &price, double &bid, double &ask) {
    QString url = buildUrlWithEndpointAndSymbol("quote",symbol);
    QJsonArray *jsonArrayFromReplyToDelete = nullptr;

    bool success = fetchSync(url, jsonArrayFromReplyToDelete);

    if (success) {
        // The positive return value implies jsonArrayFromReplyToDelete has been allocated to something
        Q_ASSERT(jsonArrayFromReplyToDelete != nullptr);

        QJsonObject obj = jsonArrayFromReplyToDelete->first().toObject();

        Q_ASSERT(symbol == obj["symbol"].toString());

        price = obj["price"].toDouble();
        bid = obj["bidPrice"].toDouble();
        ask = obj["askPrice"].toDouble();

        // This pointer to a JSON array was allocated in the fetchSync and needs to be deleted after use
        TRACK_DELETED_JSON_ARRAY(delete jsonArrayFromReplyToDelete);
    } else {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonArrayFromReplyToDelete == nullptr);
    }

    return success;
}

bool FMPClient::fetchSyncSharesFloat(const QString &symbol, QString &date, double &freeFloat, double &floatShares, double &outstandingShares)
{
    QString url = buildUrlWithEndpointAndSymbol("shares-float",symbol);
    QJsonArray *jsonArrayFromReplyToDelete;

    bool ret = fetchSync(url, jsonArrayFromReplyToDelete);

    if (ret) {
        // The positive return value implies jsonArrayFromReplyToDelete has been allocated to something
        Q_ASSERT(jsonArrayFromReplyToDelete != nullptr);

        QJsonObject obj = jsonArrayFromReplyToDelete->first().toObject();

        Q_ASSERT(symbol == obj["symbol"].toString());

        date = obj["date"].toString();
        freeFloat = obj["freeFloat"].toDouble();
        floatShares = obj["floatShares"].toDouble();
        outstandingShares = obj["outstandingShares"].toDouble();

        // This pointer to a JSON array was allocated in the fetchSync and needs to be deleted after use
        TRACK_DELETED_JSON_ARRAY(delete jsonArrayFromReplyToDelete);
    } else {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonArrayFromReplyToDelete == nullptr);
    }

    return ret;
}

void FMPClient::onThreadStarted() const {
}

void FMPClient::onReplyFinished(QNetworkReply *reply) {
    QJsonDocument doc;
    RequestInfo *info;

#ifdef UNIT_TESTING
    if (simulate_reply_network_latency) {
        qCDebug(FMPClientLog) << Q_FUNC_INFO <<
            " : Performing a (fetchSyncTimeoutMs + 1000) of " << (fetchSyncTimeoutMs + 1000) <<
            "ms delay in [" << QThread::currentThread()->objectName() << "] to simulate extreme network latency. ZZZZzzzzz..";

        QTest::qWait(fetchSyncTimeoutMs + 1000);
    }
#endif

    // Mutex is intentionally aquired after the unit test latency delay above
    QMutexLocker locker(&pendingRequestsMutex);

    if (!pendingRequests.contains(reply)) {

        qCWarning(FMPClientLog) << Q_FUNC_INFO <<
            " : pendingRequests did not containt the reply " << static_cast<void*>(reply) << " : Likely due to a caller's timeout.";

        goto delete_later;
    }

    info = &pendingRequests[reply];

    qCDebug(FMPClientLog) << Q_FUNC_INFO <<
        " : Thread [" << QThread::currentThread()->objectName() <<
        "] working on reply of request " << static_cast<void*>(reply);


    // Those are the default values, just being explicit by resetting them to default
    info->jsonArray = nullptr;
    info->completed = false;

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(FMPClientLog) << Q_FUNC_INFO << " : Error with the reply " << static_cast<void*>(reply) << " : " << reply->errorString();
        goto notify;
    }

    doc = QJsonDocument::fromJson(reply->readAll());

    if (doc.isNull() || !doc.isArray()) {
        qCWarning(FMPClientLog) << Q_FUNC_INFO << " : JSON doc is null or not an array";
        goto notify;
    }

    if (doc.array().isEmpty()) {
        qCWarning(FMPClientLog) << Q_FUNC_INFO << " : Doc array is empty";
        goto notify;
    }

    // Only if the request is SYNC do we need to store the reply in the pendindRequests map and allocate a json array
    if (info->synchronicity == RequestSynchronicity::Sync) {
        info->completed = true;
        TRACK_NEW_JSON_ARRAY(info->jsonArray = new QJsonArray(doc.array()));
    } else {
        // In the case of Async request, no need use new
    }

notify:
    if (info->synchronicity == RequestSynchronicity::Async) {
        emitSignalDemuxer(info->type, doc.array());
        pendingRequests.remove(reply);
    } else if (info->synchronicity == RequestSynchronicity::Sync) {
        waitCondition.wakeOne();
    } else {
        Q_UNREACHABLE();
    }

delete_later:

    reply->deleteLater();

#ifdef UNIT_TESTING
    if (simulate_reply_network_latency) {
        qCDebug(FMPClientLog) << Q_FUNC_INFO << " : Signaling onReplyFinished_sem";
        onReplyFinished_sem.release(1);
    }
#endif
}

void FMPClient::emitSignalDemuxer(RequestType type, const QJsonArray &doc) {

    QJsonObject obj = doc.first().toObject();

    switch(type) {

    case RequestType::None:
        Q_ASSERT_X(0,"","Should not be None anymore");
        break;

    case RequestType::Quote:
        emit quoteReceived(obj["symbol"].toString(),
                           obj["price"].toDouble(),
                           obj["bidPrice"].toDouble(),
                           obj["askPrice"].toDouble());
        break;
    case RequestType::SharesFloat:
        emit sharesFloatReceived(obj["symbol"].toString(),
                                 obj["date"].toString(),
                                 obj["freeFloat"].toDouble(),
                                 obj["floatShares"].toDouble(),
                                 obj["outstandingShares"].toDouble());

        break;

    default:
        Q_UNREACHABLE();
        break;
    }
}

#ifdef UNIT_TESTING
bool FMPClient::isCleanedUp()
{
    bool isClean = true;

    if (!pendingRequests.isEmpty()) {
        qCWarning(FMPClientLog) << Q_FUNC_INFO << " : pendingRequests not empty";
        isClean = false;
    }

    if (allocated_json_arrays != 0) {
        qCWarning(FMPClientLog) << Q_FUNC_INFO << " : allocated_json_arrays != 0 : " << allocated_json_arrays;
        isClean = false;
    }

    return isClean;
}
#endif
