#include "qtestsupport_core.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#ifdef UNIT_TESTING
#include <QtTest/QtTest>
#endif

#include "RESTClient.h"


Q_LOGGING_CATEGORY(RESTClientLog, "RESTClient");

RESTClient::RESTClient(const QUrl &baseUrl, QObject *parent)
    : QObject(parent),
    thread(new QThread()),
    manager(new QNetworkAccessManager(this)),
    baseUrl(baseUrl)
{
    this->moveToThread(thread);

    connect(manager, &QNetworkAccessManager::finished, this, &RESTClient::onReplyFinished);
}

RESTClient::~RESTClient() {

}

void RESTClient::start()
{
    thread->start();
}

void RESTClient::setAPIKey(const QString &apiKey)
{
    this->apiKey = apiKey;
}

qsizetype RESTClient::getTotalDataReceivedBytes() const
{
    return totalDataReceivedBytes;
}

QNetworkRequest RESTClient::buildRequest(ApiKeyPlacement placement, const QString &endpoint, const QString &symbol) const {
    QUrl url = baseUrl;
    url.setPath(url.path() + endpoint);
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    if (placement == ApiKeyPlacement::InUrl) {
        QUrlQuery query;
        if (!symbol.isEmpty()) {
            query.addQueryItem("symbol", symbol);
        }
        query.addQueryItem("apikey", apiKey);
        url.setQuery(query);
        request.setUrl(url);
    } else {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
        if (!symbol.isEmpty()) {
            QUrlQuery query;
            query.addQueryItem("symbol", symbol);
            url.setQuery(query);
            request.setUrl(url);
        }
    }
    
    return request;
}

QNetworkRequest RESTClient::buildRequest(ApiKeyPlacement placement, const QString &endpoint, const QUrlQuery &query) const
{
    QUrl url = baseUrl;
    url.setPath(url.path() + endpoint);
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    if (placement == ApiKeyPlacement::InUrl) {
        QUrlQuery finalQuery(query);
        finalQuery.addQueryItem("apikey", apiKey);
        url.setQuery(finalQuery);
        request.setUrl(url);
    } else {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
        url.setQuery(query);
        request.setUrl(url);
    }
    
    return request;
}

/*
 * Underlying function doing the fetching of the JSON common to all fetchXSync methods
 */
bool RESTClient::fetchSync(const QNetworkRequest &request, QJsonDocument *&jsonDocumentFromReplyToDelete, HttpMethod method, const QByteArray &postData) {
    QNetworkReply *reply = nullptr;

    // Invoke this method in the distinct thread of the FMPClient singleton.
    // Having ->moveToThread() the FMPClient to a dedicated thread makes that calling to 'this'
    // is actually invoking this method NOT in the caller's thread.
    QMetaObject::invokeMethod(this, [this, request, &reply, method, postData]() {
        switch (method) {
            case HttpMethod::GET:
                reply = manager->get(request);
                break;
            case HttpMethod::POST:
                // Use the provided postData if available, otherwise send empty data
                reply = manager->post(request, postData);
                break;
        }

        qCDebug(RESTClientLog) << Q_FUNC_INFO <<
            " : Thread [" << QThread::currentThread()->objectName() <<
            "] executed the queued" << (method == HttpMethod::GET ? "GET" : "POST") << 
            "request and registered the reply " << static_cast<void*>(reply) << " for later reception.";

        QMutexLocker locker(&pendingRequestsMutex);
        {
            pendingRequests[reply] = {.synchronicity = RequestSynchronicity::Sync,
                                      .type = RequestTypeNone,
                                      .completed = false,
                                      .jsonDocument = nullptr};
        }

    }, Qt::QueuedConnection);

    qCDebug(RESTClientLog) << Q_FUNC_INFO <<
        " : Thread [" << QThread::currentThread()->objectName() <<
        "] invoked the queued method to" << (method == HttpMethod::GET ? "GET" : "POST") <<
        " with URL " << request.url().toString() <<
        " header : " << request.headers() <<
        " and data : " << postData;

    QMutexLocker locker(&pendingRequestsMutex);

    // Mutex is released while we wait
    if (!waitCondition.wait(&pendingRequestsMutex, fetchSyncTimeoutMs)) {
        // Mutex is re-aquired

        qCWarning(RESTClientLog) << Q_FUNC_INFO <<
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

    //#warning TODO asserts on the container here toooo... maybe?

    if (pendingRequests.contains(reply) && pendingRequests[reply].completed) {
        jsonDocumentFromReplyToDelete = pendingRequests[reply].jsonDocument;

        qCDebug(RESTClientLog) << Q_FUNC_INFO << " : The *reply " << static_cast<void*>(reply) << " successfuly completed.";

        bool was_removed = pendingRequests.remove(reply);

        // There is a logic problem if the *reply was not in the map at this point
        Q_ASSERT(was_removed);

        return true;
    }

    jsonDocumentFromReplyToDelete = nullptr;

    pendingRequests.remove(reply);

    qWarning() << Q_FUNC_INFO << " : The pending requests did not contain the reply, or was not marked as completed.";

    return false;
}

void RESTClient::fetchAsync(const QNetworkRequest &request, RequestTypeInt type, HttpMethod method, const QByteArray &postData) {
    // Invoke this method in the distinct thread of the FMPClient singleton.
    // Having ->moveToThread() the FMPClient to a dedicated thread makes that calling to 'this'
    // is actually invoking this method NOT in the caller's thread.
    QMetaObject::invokeMethod(this, [this, request, type, method, postData]() {
        QNetworkReply *reply = nullptr;
        
        switch (method) {
            case HttpMethod::GET:
                reply = manager->get(request);
                break;
            case HttpMethod::POST:
                // Use the provided postData if available, otherwise send empty data
                reply = manager->post(request, postData);
                break;
        }

        qCDebug(RESTClientLog) << Q_FUNC_INFO <<
            " : Thread [" << QThread::currentThread()->objectName() <<
            "] executed the queued" << (method == HttpMethod::GET ? "GET" : "POST") <<
            "request and registered the reply " << static_cast<void*>(reply) << " for later reception.";

        Q_ASSERT(reply != nullptr);

        QMutexLocker locker(&pendingRequestsMutex);
        pendingRequests[reply] = { .synchronicity = RequestSynchronicity::Async,
                                  .type = type,
                                  .completed = false,
                                  .jsonDocument = nullptr};
    }, Qt::QueuedConnection);


    qCDebug(RESTClientLog) << Q_FUNC_INFO <<
        " : Thread [" << QThread::currentThread()->objectName() <<
        "] invoked the queued method to" << (method == HttpMethod::GET ? "GET" : "POST") <<
        " with URL " << request.url().toString() <<
        " header : " << request.headers() <<
        " and data : " << postData;
}

// This is all a bit hacky for now...
void RESTClient::fetchStream(const QNetworkRequest &request, void* arg)
{
    Q_ASSERT(arg != nullptr);

    // Fetch stream is only meant to be called from TSClient, and from context of the thread itself
    Q_ASSERT(QThread::currentThread() == this->thread);

    QNetworkReply *reply = manager->get(request);

    Q_ASSERT(reply != nullptr);

    QMutexLocker locker(&pendingRequestsMutex);

    pendingRequests[reply] = { .synchronicity = RequestSynchronicity::Stream,
                              .type = 0,
                              .completed = false,
                              .jsonDocument = nullptr,
                              .optArg = arg};
}

void RESTClient::closeStream(void *arg)
{
    Q_ASSERT(arg != nullptr);

    // Fetch stream is only meant to be called from TSClient, and from context of the thread itself
    Q_ASSERT(QThread::currentThread() == this->thread);

    QMutexLocker locker(&pendingRequestsMutex);

    QNetworkReply *replyToDelete = nullptr;

    for (auto it = pendingRequests.constBegin(); it != pendingRequests.constEnd(); ++it) {
        QNetworkReply *reply = it.key();
        const RequestInfo &info = it.value();
        if (info.optArg == arg) {
            Q_ASSERT(info.synchronicity == RequestSynchronicity::Stream);
            replyToDelete = reply;
            break;
        }
    }

    Q_ASSERT(replyToDelete != nullptr);

    replyToDelete->abort();

    replyToDelete->deleteLater();
}

void RESTClient::processStreamFinished(QNetworkReply *reply, QByteArray &rawData, void *arg)
{
    // Default implementation when not TSClient
    Q_UNUSED(reply);
    Q_UNUSED(rawData);
    Q_UNUSED(arg);
    Q_ASSERT(0);
}

void RESTClient::processStreamReadyRead(QNetworkReply *reply, QByteArray &rawData, void *arg)
{
    // Default implementation when not TSClient
    Q_UNUSED(reply);
    Q_UNUSED(rawData);
    Q_UNUSED(arg);
    Q_ASSERT(0);
}

/*
void RESTClient::onError(QNetworkReply *reply) {

}
*/

void RESTClient::onReplyReadyRead(QNetworkReply *reply){
    QMutexLocker locker(&pendingRequestsMutex);  // TODO this works, but rethink this mutex as it would be held for a while

    Q_ASSERT(pendingRequests.contains(reply));

    if (pendingRequests[reply].synchronicity != RequestSynchronicity::Stream) {
        // Only for https streams that dont finish do we expect to continue.
        // All the other requests are finite and handled in onFinished()
        return;
    }

    QByteArray rawData = reply->readAll();
    qsizetype bytesReceived = rawData.size();

    totalDataReceivedBytes += bytesReceived;

    // Broadcast the new data size (ie to update the GUI)
    emit totalDataReceivedBytesIncreased(totalDataReceivedBytes);

    qCDebug(RESTClientLog) << Q_FUNC_INFO << " : Received " << bytesReceived << " bytes, total now " << totalDataReceivedBytes << " bytes";

    processStreamReadyRead(reply, rawData, pendingRequests[reply].optArg);

}

void RESTClient::onReplyFinished(QNetworkReply *reply) {
    QJsonDocument doc;
    RequestInfo *info;

#ifdef UNIT_TESTING
    if (simulate_reply_network_latency) {
        qCDebug(RESTClientLog) << Q_FUNC_INFO <<
            " : Performing a (fetchSyncTimeoutMs + 1000) of " << (fetchSyncTimeoutMs + 1000) <<
            "ms delay in [" << QThread::currentThread()->objectName() << "] to simulate extreme network latency. ZZZZzzzzz..";

        QTest::qWait(fetchSyncTimeoutMs + 1000);
    }
#endif

    QByteArray rawData = reply->readAll();
    qsizetype bytesReceived = rawData.size();

    totalDataReceivedBytes += bytesReceived;

    // Broadcast the new data size (ie to update the GUI)
    emit totalDataReceivedBytesIncreased(totalDataReceivedBytes);

    qCDebug(RESTClientLog) << Q_FUNC_INFO << " : Received " << bytesReceived << " bytes, total now " << totalDataReceivedBytes << " bytes";

    // Mutex is intentionally aquired after the unit test latency delay above
    QMutexLocker locker(&pendingRequestsMutex);

    if (!pendingRequests.contains(reply)) {

        qCWarning(RESTClientLog) << Q_FUNC_INFO <<
            " : pendingRequests did not containt the reply " << static_cast<void*>(reply) << " : Likely due to a caller's timeout.";

        Q_ASSERT(0); // Should NOT happen
        goto delete_later;
    }

    info = &pendingRequests[reply];

    qCDebug(RESTClientLog) << Q_FUNC_INFO <<
        " : Thread [" << QThread::currentThread()->objectName() <<
        "] working on reply of request " << static_cast<void*>(reply);


    // Those are the default values, just being explicit by resetting them to default
    info->jsonDocument = nullptr;
    info->completed = false;

    doc = QJsonDocument::fromJson(rawData);

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(RESTClientLog) << Q_FUNC_INFO <<
            " : Error with the reply " << static_cast<void*>(reply) << " : " << reply->errorString() << " : " << reply->error();
        qCWarning(RESTClientLog).noquote() << Q_FUNC_INFO <<
            " : Content of the reply : \n" << doc.toJson(QJsonDocument::Indented);
        goto notify;
    }

    if (doc.isNull()){
        qCWarning(RESTClientLog) << Q_FUNC_INFO << " : JSON doc is null";
        goto notify;
    }

    if (doc.isArray() && doc.array().isEmpty()) {
        qCWarning(RESTClientLog) << Q_FUNC_INFO << " : Doc array is empty";
        //goto notify; <- I ended up accepting that an array can be null. FMP returns that for some stocks when they have no news AT ALL
    }

    if (doc.isObject() && doc.object().isEmpty()) {
        qCWarning(RESTClientLog) << Q_FUNC_INFO << " : Doc object is empty";
        goto notify;
#warning should probably do not do the goto line the one above
    }   

    // At this point, the reply is legit
    info->completed = true;

    // Only if the request is SYNC do we need to store the reply in the pendindRequests map and allocate a json array
    if (info->synchronicity == RequestSynchronicity::Sync) {
        TRACK_NEW_JSON_ARRAY(info->jsonDocument = new QJsonDocument(doc));
    } else {
        // In the case of Async or Stream request, no need use new
    }

notify:
    if (info->synchronicity == RequestSynchronicity::Async) {
        if (info->completed == true) {
            // If the request failed, do not emit the signal. This is a design choice I guess.
            // Time will tell if the app should still receive a signal, albeit with an error flag set.
            emitSignalDemuxer(info->type, doc);
        }
        // Whether the request was successful or not, take it out of the map
        bool was_removed = pendingRequests.remove(reply);

        // There is a logic problem if the *reply was not in the map at this point / not successfuly removed
        Q_ASSERT(was_removed);

    } else if (info->synchronicity == RequestSynchronicity::Sync) {
        waitCondition.wakeOne();
    } else if (info->synchronicity == RequestSynchronicity::Stream) {
        processStreamFinished(reply, rawData, info->optArg);
        bool was_removed = pendingRequests.remove(reply);
        Q_ASSERT(was_removed);
    } else {
        Q_UNREACHABLE();
    }

delete_later:

    reply->deleteLater();

#ifdef UNIT_TESTING
    if (simulate_reply_network_latency) {
        qCDebug(RESTClientLog) << Q_FUNC_INFO << " : Signaling onReplyFinished_sem";
        onReplyFinished_sem.release(1);
    }
#endif
}

#ifdef UNIT_TESTING
bool RESTClient::isCleanedUp()
{
    bool isClean = true;

    if (!pendingRequests.isEmpty()) {
        qCWarning(RESTClientLog) << Q_FUNC_INFO << " : ********************* pendingRequests not empty ****************";
        isClean = false;

        size_t i = 0;
        for (const auto& request : pendingRequests) {
            qDebug() << "Request info #" << i;
            qDebug() << "  Syncronicity : " << ((request.synchronicity == RequestSynchronicity::Async) ? "ASYNC" : "SYNC");
            qDebug() << "  RequestType  : " << request.type;
            qDebug() << "  Completed    : " << ((request.completed) ? "TRUE" : "FALSE");
            qDebug() << "  Docptr       : " << static_cast<void*>(request.jsonDocument);
        }
    }

    if (allocated_json_arrays != 0) {
        qCWarning(RESTClientLog) << Q_FUNC_INFO << " : allocated_json_arrays != 0 : " << allocated_json_arrays;
        isClean = false;
    }

    return isClean;
}
#endif
