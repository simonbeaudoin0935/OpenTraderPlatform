#include "tradestationclient.h"
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
Q_LOGGING_CATEGORY(TradeStationClientLog, "TradeStationClient")

// Initialize static member outside class
TradeStationClient* TradeStationClient::instance = nullptr;

QString TradeStationClient::apiKey = "";

TradeStationClient& TradeStationClient::getInstance() {
    if (instance == nullptr) {
        qCDebug(TradeStationClientLog) << "Singleton instance created";
        instance = new TradeStationClient();
    }
    return *instance;
}

TradeStationClient* TradeStationClient::getInstancePtr() {
    if (instance == nullptr) {
        qCDebug(TradeStationClientLog) << "Singleton instance created";
        instance = new TradeStationClient();
    }
    return instance;
}

void TradeStationClient::setAPIKey(const QString &apiKey)
{
    TradeStationClient::apiKey = apiKey;
    qCDebug(TradeStationClientLog) << "API key set to " << apiKey;
}

qsizetype TradeStationClient::getTotalDataReceivedBytes() const
{
    return totalDataReceivedBytes.load(std::memory_order_relaxed);
}

TradeStationClient::TradeStationClient() :
    thread(new QThread()),
    manager(new QNetworkAccessManager(this))
{
    qCDebug(TradeStationClientLog) << Q_FUNC_INFO << ": TradeStationClient created using KEY=" << apiKey;

    Q_ASSERT(!apiKey.isEmpty());

    thread->setObjectName("TradeStationClientThread");

    this->moveToThread(thread);

    connect(thread, &QThread::started, this, &TradeStationClient::onThreadStarted);
    connect(manager, &QNetworkAccessManager::finished, this, &TradeStationClient::onReplyFinished);
    thread->start();
}

TradeStationClient::~TradeStationClient() {
    qCDebug(TradeStationClientLog) << "Singleton instance destroyed";

    thread->quit();
    thread->wait();
}

QString TradeStationClient::buildUrlWithEndpoint(const QString &endpoint) const {
    return baseUrl + endpoint + "?" +
           QString("apikey=%1").arg(apiKey);
}

QString TradeStationClient::buildUrlWithEndpointAndSymbol(const QString &endpoint, const QString &symbol) const {
    return baseUrl + endpoint + "?" +
           QString("symbol=%1").arg(symbol) + "&" +
           QString("apikey=%1").arg(apiKey);
}

QString TradeStationClient::buildUrlWithEndpointAndParamsList(const QString &endpoint, const QString &paramsList) const
{
    return baseUrl + endpoint + "?" +
           paramsList + "&" +
           QString("apikey=%1").arg(apiKey);
}

void TradeStationClient::fetchAsync(const QString &url, RequestType type) {
    // Invoke this method in the distinct thread of the TradeStationClient singleton.
    // Having ->moveToThread() the TradeStationClient to a dedicated thread makes that calling to 'this'
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

bool TradeStationClient::fetchSync(const QString &url, QJsonArray *&jsonArrayFromReplyToDelete) {
    QNetworkReply *reply = nullptr;

    // Invoke this method in the distinct thread of the TradeStationClient singleton.
    // Having ->moveToThread() the TradeStationClient to a dedicated thread makes that calling to 'this'
    // is actually invoking this method NOT in the caller's thread.
    QMetaObject::invokeMethod(this, [this, url, &reply]() {
        QNetworkRequest request(url);
        reply = manager->get(request);

        qCDebug(TradeStationClientLog) << Q_FUNC_INFO <<
            " : Thread [" << QThread::currentThread()->objectName() <<
            "] executed the queued GET request and registered the reply " << static_cast<void*>(reply) << " for later reception.";

        QMutexLocker locker(&pendingRequestsMutex);
        {
            pendingRequests[reply] = {RequestSynchronicity::Sync};
        }

    }, Qt::QueuedConnection);

    qCDebug(TradeStationClientLog) << Q_FUNC_INFO <<
        " : Thread [" << QThread::currentThread()->objectName() <<
        "] invoked the queued method to GET url " << url ;

    QMutexLocker locker(&pendingRequestsMutex);

    // Mutex is released while we wait
    if (!waitCondition.wait(&pendingRequestsMutex, fetchSyncTimeoutMs)) {
        // Mutex is re-aquired

        qCWarning(TradeStationClientLog) << Q_FUNC_INFO <<
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

    // At this point, we got notified by the waitCondition. The TradeStationClient thread MUST have executed
    // the invoked method above and populated the reply
    Q_ASSERT(reply != nullptr);

    if (pendingRequests.contains(reply) && pendingRequests[reply].completed) {
        jsonArrayFromReplyToDelete = pendingRequests[reply].jsonArray;

        qCDebug(TradeStationClientLog) << Q_FUNC_INFO << " : The *reply " << static_cast<void*>(reply) << " successfuly completed.";

        pendingRequests.remove(reply);

        return true;
    }

    jsonArrayFromReplyToDelete = nullptr;

    pendingRequests.remove(reply);

    qWarning() << Q_FUNC_INFO << " : The pending requests did not contain the reply, or was not marked as completed.";

    return false;
}

void TradeStationClient::onThreadStarted() const {
}

void TradeStationClient::onReplyFinished(QNetworkReply *reply) {
    QJsonDocument doc;
    RequestInfo *info;

#ifdef UNIT_TESTING
    if (simulate_reply_network_latency) {
        qCDebug(TradeStationClientLog) << Q_FUNC_INFO <<
            " : Performing a (fetchSyncTimeoutMs + 1000) of " << (fetchSyncTimeoutMs + 1000) <<
            "ms delay in [" << QThread::currentThread()->objectName() << "] to simulate extreme network latency. ZZZZzzzzz..";

        QTest::qWait(fetchSyncTimeoutMs + 1000);
    }
#endif

    QByteArray rawData = reply->readAll();
    qsizetype bytesReceived = rawData.size();
    totalDataReceivedBytes.fetch_add(bytesReceived, std::memory_order_relaxed);  // Atomic increment

    // Broadcast the new data size (ie to update the GUI)
    emit totalDataReceivedBytesIncreased(totalDataReceivedBytes.load(std::memory_order_relaxed));

    qCDebug(TradeStationClientLog) << Q_FUNC_INFO << " : Received " << bytesReceived << " bytes, total now " << totalDataReceivedBytes.load(std::memory_order_relaxed) << " bytes";

    // Mutex is intentionally aquired after the unit test latency delay above
    QMutexLocker locker(&pendingRequestsMutex);

    if (!pendingRequests.contains(reply)) {
        qCWarning(TradeStationClientLog) << Q_FUNC_INFO <<
            " : pendingRequests did not containt the reply " << static_cast<void*>(reply) << " : Likely due to a caller's timeout.";

        goto delete_later;
    }

    info = &pendingRequests[reply];

    qCDebug(TradeStationClientLog) << Q_FUNC_INFO <<
        " : Thread [" << QThread::currentThread()->objectName() <<
        "] working on reply of request " << static_cast<void*>(reply);

    // Those are the default values, just being explicit by resetting them to default
    info->jsonArray = nullptr;
    info->completed = false;

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(TradeStationClientLog) << Q_FUNC_INFO << " : Error with the reply " << static_cast<void*>(reply) << " : " << reply->errorString();
        goto notify;
    }

    doc = QJsonDocument::fromJson(rawData);

    if (doc.isNull() || !doc.isArray()) {
        qCWarning(TradeStationClientLog) << Q_FUNC_INFO << " : JSON doc is null or not an array";
        goto notify;
    }

    if (doc.array().isEmpty()) {
        qCWarning(TradeStationClientLog) << Q_FUNC_INFO << " : Doc array is empty";
        goto notify;
    }

    // At this point, the reply is legit
    info->completed = true;

    // Only if the request is SYNC do we need to store the reply in the pendindRequests map and allocate a json array
    if (info->synchronicity == RequestSynchronicity::Sync) {
        TRACK_NEW_JSON_ARRAY(info->jsonArray = new QJsonArray(doc.array()));
    } else {
        // In the case of Async request, no need use new
    }

notify:
    if (info->synchronicity == RequestSynchronicity::Async) {
        if (info->completed == true) {
            // If the request failed, do not emit the signal. This is a design choice I guess.
            // Time will tell if the app should still receive a signal, albeit with an error flag set.
            emitSignalDemuxer(info->type, doc.array());
        }
        // Whether the request was successful or not, take it out of the map
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
        qCDebug(TradeStationClientLog) << Q_FUNC_INFO << " : Signaling onReplyFinished_sem";
        onReplyFinished_sem.release(1);
    }
#endif
}

void TradeStationClient::emitSignalDemuxer(RequestType type, const QJsonArray &doc) {
    // TODO: Implement signal demuxing when we add specific request types
}

#ifdef UNIT_TESTING
bool TradeStationClient::isCleanedUp()
{
    bool isClean = true;

    if (!pendingRequests.isEmpty()) {
        qCWarning(TradeStationClientLog) << Q_FUNC_INFO << " : pendingRequests not empty";
        isClean = false;
    }

    if (allocated_json_arrays != 0) {
        qCWarning(TradeStationClientLog) << Q_FUNC_INFO << " : allocated_json_arrays != 0 : " << allocated_json_arrays;
        isClean = false;
    }

    return isClean;
}
#endif 