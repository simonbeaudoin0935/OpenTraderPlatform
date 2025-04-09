#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#ifdef UNIT_TESTING
#include <QtTest/QtTest>
#endif

#include "RESTClient.h"
#include "TSClient/Stream/Stream.h" // TODO I dont like having to include this header here

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
    QSemaphore semaphore(0); // begin locked

    // Only external callers to TSClient thread should get here. Calling a fetch sync from within the TSClient's
    // thread would cause a deadlock to itself
    Q_ASSERT(QThread::currentThread() != thread);

    // Invoke this method in the TSClient's thread. Note we are passing a reference to *reply so that TSClient's thread
    // can give it us back to use it later. The parameters request and data are also passed by reference to avoid
    // copying them. This is safe because we are using a BlockingQueuedConnection
    QMetaObject::invokeMethod(this, [this, &semaphore, &request, &reply, method, &postData]() {
        switch (method) {
            case HttpMethod::GET:
                reply = manager->get(request);
                break;
            case HttpMethod::POST:
                // Use the provided postData if available, otherwise send empty data
                reply = manager->post(request, postData);
                break;
        }

        Q_CHECK_PTR(reply);

        qCDebug(RESTClientLog) << Q_FUNC_INFO <<
            " : Thread [" << QThread::currentThread()->objectName() <<
            "] executed the queued" << (method == HttpMethod::GET ? "GET" : "POST") << 
            "request and registered the reply " << static_cast<void*>(reply) << " for later reception.";

        pendingRequestsRWLock.lockForWrite();
        {
            pendingRequests[reply] = {.synchronicity = RequestSynchronicity::Sync,
                                      .type = RequestTypeNone,
                                      .completed = false,
                                      .jsonDocument = nullptr,
                                      .optArg = static_cast<void*>(&semaphore)}; // giving the pointer to our semaphore so
                                                                                 // TSClient knows who to wake up
        }
        pendingRequestsRWLock.unlock();

    }, Qt::BlockingQueuedConnection);

    qCDebug(RESTClientLog) << Q_FUNC_INFO <<
        " : Thread [" << QThread::currentThread()->objectName() <<
        "] invoked the queued method to" << (method == HttpMethod::GET ? "GET" : "POST") <<
        " with URL " << request.url().toString() <<
        " header : " << request.headers() <<
        " and data : " << postData;

    //TODO find the right wait mechanism
    bool aquired = semaphore.tryAcquire(1,fetchSyncTimeoutMs);

    if (false == aquired) {
        qCWarning(RESTClientLog) << Q_FUNC_INFO <<
            " : Timed out after  " << fetchSyncTimeoutMs << "ms. Removing the *reply " << static_cast<void*>(reply) << " from pendingRequests";

        // If we timed out before receiving the reply, we remove the *reply from the map.
        // This is important so that *if* the reply/error eventually arrives, its onReplyFinished()
        // processing will notice it is not in the map. This will signify to the onReplyFinished()
        // that there is nobody waiting for this reply anymore.
        pendingRequestsRWLock.lockForWrite();
        {
            bool removed = pendingRequests.remove(reply);
            Q_ASSERT(removed); // There is a logic problem if the *reply was not in the map at this point
        }
        pendingRequestsRWLock.unlock();

        return false;
    }

    // At this point, we got notified by the waitCondition. The FMPClient thread MUST have executed
    // the invoked method above and populated the reply
    Q_ASSERT(reply != nullptr);


    pendingRequestsRWLock.lockForRead();
    {
        // We were woken up, our request HAS to be in the pendingRequests container
        Q_ASSERT(pendingRequests.contains(reply));
        // If it is, the request HAS to be completed synce this is a sync request
        Q_ASSERT(pendingRequests[reply].completed == true);
        // Being paranoid, but just double checking the synchronicity of our corresponding request is still SYNC
        Q_ASSERT(pendingRequests[reply].synchronicity == RequestSynchronicity::Sync);
        // If we are still alive here, the jsonDocument pointer MUST have been populated to something
        Q_ASSERT(pendingRequests[reply].jsonDocument != nullptr);

        // Finally, retreive our precious
        jsonDocumentFromReplyToDelete = pendingRequests[reply].jsonDocument;
    }
    pendingRequestsRWLock.unlock();

    pendingRequestsRWLock.lockForWrite();
    {
        // Now that we fetched our Json document that was created by the TSClient's thread,
        // remove the request from the list. This signify this sync request has finished to the eyes
        // of the TSClient's thread.
        bool removed = pendingRequests.remove(reply);
        Q_ASSERT(removed);
    }
    pendingRequestsRWLock.unlock();

    // If we got here without asserting, everything is fine
    return true;
}

void RESTClient::fetchAsync(const QNetworkRequest &request, RequestTypeInt type, HttpMethod method, const QByteArray &postData) {

    // Because this is a queud method invocation, the parameters have to be passed by value
    // TODO in the future, to avoid postData especialy, we could new it where it is build and deleted inside here
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

        Q_CHECK_PTR(reply);

        qCDebug(RESTClientLog) << Q_FUNC_INFO <<
            " : Thread [" << QThread::currentThread()->objectName() <<
            "] executed the queued" << (method == HttpMethod::GET ? "GET" : "POST") <<
            "request and registered the reply " << static_cast<void*>(reply) << " for later reception.";


        pendingRequestsRWLock.lockForWrite();
        {
            pendingRequests[reply] = { .synchronicity = RequestSynchronicity::Async,
                                       .type = type,
                                       .completed = false,
                                       .jsonDocument = nullptr};
        }
        pendingRequestsRWLock.unlock();
    }, Qt::QueuedConnection);


    qCDebug(RESTClientLog) << Q_FUNC_INFO <<
        " : Thread [" << QThread::currentThread()->objectName() <<
        "] invoked the queued method to" << (method == HttpMethod::GET ? "GET" : "POST") <<
        " with URL " << request.url().toString() <<
        " header : " << request.headers() <<
        " and data : " << postData;
}

// This is all a bit hacky for now...
QNetworkReply* RESTClient::fetchStream(const QNetworkRequest &request, void* arg)
{
    Q_ASSERT(arg != nullptr);

    // Fetch stream is only meant to be called from TSClient's thread
    Q_ASSERT(QThread::currentThread() == this->thread);


    QNetworkReply *reply = manager->get(request);

    Q_ASSERT(reply != nullptr);

    qCDebug(RESTClientLog) << Q_FUNC_INFO <<
        "GET network reply = " << static_cast<void*>(reply) <<
        " with URL : " << request.url() <<
        " and header : " << request.headers();


    pendingRequestsRWLock.lockForWrite();
    {
        pendingRequests[reply] = { .synchronicity = RequestSynchronicity::Stream,
                                   .type = 0,
                                   .completed = false,
                                   .jsonDocument = nullptr,
                                   .optArg = arg};
    }
    pendingRequestsRWLock.unlock();

    return reply;
}

void RESTClient::closeStream(void *arg)
{
    QNetworkReply *replyToDelete = nullptr;

    Q_ASSERT(arg != nullptr);

    // Closing the stream is only meant to be called from TSClient's
    Q_ASSERT(QThread::currentThread() == this->thread);

    qCDebug(RESTClientLog) << Q_FUNC_INFO << "Going through all pending replies";

    pendingRequestsRWLock.lockForRead();
    {
        for (auto it = pendingRequests.constBegin(); it != pendingRequests.constEnd(); ++it) {
            QNetworkReply *reply = it.key();

            qCDebug(RESTClientLog) << Q_FUNC_INFO << "Checking reply " << static_cast<void*>(reply);

            const RequestInfo &info = it.value();
            if (info.optArg == arg) {

                // Paranoia. If we found the pendingRequest for which the stream arg corresponds, it has to have
                // its type to stream
                Q_ASSERT(info.synchronicity == RequestSynchronicity::Stream);

                replyToDelete = reply;

                qCDebug(RESTClientLog) << Q_FUNC_INFO << "Found that this reply has stream  " << static_cast<void*>(arg) << " attached";

                break;
            }
        }
    }
    pendingRequestsRWLock.unlock();

    Stream* stream = static_cast<Stream*>(arg);

    if (stream->isFinished()) {
        // We are closing a Stream that previously received a finishing event from the remote server.
        // This means that the TSClient already did some cleanup and removed its pendingRequest from the
        // container. If the Stream was marked finished, it means it MUST have been removed from the list
        Q_ASSERT(replyToDelete == nullptr);
    } else {
        // On the other hand, we are closing a stream that is stil ongoing, it MUST still be present in the container
        Q_ASSERT(replyToDelete != nullptr);

        qCDebug(RESTClientLog) << Q_FUNC_INFO << "Aborting request";
        // Close the connection and mark it
        replyToDelete->abort();
        replyToDelete->deleteLater();

        pendingRequestsRWLock.lockForWrite();
        {
            bool removed = pendingRequests.remove(replyToDelete);
            if (removed) {
                qCDebug(RESTClientLog) << Q_FUNC_INFO << " normal";
            } else {
                qCDebug(RESTClientLog) << Q_FUNC_INFO << " request not removed because it appears that the stream was manually closed before";
            }
        }
        pendingRequestsRWLock.unlock();
    }

    return;
}

void RESTClient::onReplyFinished(QNetworkReply *reply) {
    QJsonDocument doc;
    RequestInfo *requestInfo;
    QJsonParseError parseError;

    Q_ASSERT(QThread::currentThread() == thread); // Paranoia

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

    emit totalDataReceivedBytesIncreased(totalDataReceivedBytes);

    qCDebug(RESTClientLog) << Q_FUNC_INFO << " : Received " << bytesReceived << " bytes, total now " << totalDataReceivedBytes << " bytes";

    // lock for write for pretty much the rest of this function, since we modify a struct pointed in the contained
#warning If we deadlock again, it might be due to side effect of holding this lock for too long (and calling the process* functions at the end of this block)
    pendingRequestsRWLock.lockForWrite();

    if (!pendingRequests.contains(reply)) {

        qCWarning(RESTClientLog) << Q_FUNC_INFO <<
            " : pendingRequests did not containt the reply " << static_cast<void*>(reply) << " : Likely due to a SYNC caller timing out and bailed.";

        goto delete_later;
    }

    // copying the struct so we can use it outside the RW lock
    requestInfo = &pendingRequests[reply];

    qCDebug(RESTClientLog) << Q_FUNC_INFO <<
        " : Thread [" << QThread::currentThread()->objectName() <<
        "] working on reply of request " << static_cast<void*>(reply);

    // Those are the default values, just being paranoid that we are consistent and nothing else corrupted those before us
    Q_ASSERT(requestInfo->jsonDocument == nullptr);
    Q_ASSERT(requestInfo->completed == false);

    doc = QJsonDocument::fromJson(rawData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qCWarning(RESTClientLog) << Q_FUNC_INFO << "Failed to parse JSON:" << parseError.errorString();
        qCWarning(RESTClientLog) << Q_FUNC_INFO << "Content of the bad data : " << rawData;
        goto notify;
    }

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
        //Ccontinue, this is legal
    }

    if (doc.isObject() && doc.object().isEmpty()) {
        qCWarning(RESTClientLog) << Q_FUNC_INFO << " : Doc object is empty";
        // Continue, this is legal
    }   

    // At this point, the reply is legit
    requestInfo->completed = true;

    if (requestInfo->synchronicity == RequestSynchronicity::Sync) {
        // Only if the request is SYNC do we need to store the reply in the pendindRequests map and allocate a json array
        TRACK_NEW_JSON_ARRAY(requestInfo->jsonDocument = new QJsonDocument(doc));
        Q_CHECK_PTR(requestInfo->jsonDocument);
    } else {
        // In the case of Async or Stream request, no need use new
    }

notify:
    if (requestInfo->synchronicity == RequestSynchronicity::Async) {
        if (requestInfo->completed == true) {
            emitSignalDemuxer(requestInfo->type, doc);
        } else {
            // If the request failed, do not emit the signal. This is a design choice I guess.
            // Time will tell if the app should still receive a signal, albeit with an error flag set.
        }

        // Whether the request was successful or not, take it out of the map
        bool removed = pendingRequests.remove(reply);
        Q_ASSERT(removed);

    } else if (requestInfo->synchronicity == RequestSynchronicity::Sync) {
        Q_ASSERT(requestInfo->optArg != nullptr);

        QSemaphore *semaphore = static_cast<QSemaphore*>(requestInfo->optArg);
        semaphore->release();

        // Here we left the request in the container. The caller will remove it in its thread

    } else if (requestInfo->synchronicity == RequestSynchronicity::Stream) {
        qCDebug(RESTClientLog) << Q_FUNC_INFO << "Removing network reply " << static_cast<void*>(reply) << " for stream " << requestInfo->optArg;

        bool removed = pendingRequests.remove(reply);
        Q_ASSERT(removed);
    } else {
        Q_UNREACHABLE();
    }

delete_later:

    pendingRequestsRWLock.unlock();

    reply->deleteLater();

#ifdef UNIT_TESTING
    if (simulate_reply_network_latency) {
        qCDebug(RESTClientLog) << Q_FUNC_INFO << " : Signaling onReplyFinished_sem";
        onReplyFinished_sem.release(1);
    }
#endif
}

void RESTClient::onReceivedNewAmountOfData(qsizetype bytes)
{
    totalDataReceivedBytes += bytes;

    emit totalDataReceivedBytesIncreased(totalDataReceivedBytes);
}

#ifdef UNIT_TESTING
bool RESTClient::isCleanedUp()
{
    bool isClean = true;

    pendingRequestsRWLock.lockForRead();
    {
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
    }
    pendingRequestsRWLock.unlock();

    if (allocated_json_arrays != 0) {
        qCWarning(RESTClientLog) << Q_FUNC_INFO << " : allocated_json_arrays != 0 : " << allocated_json_arrays;
        isClean = false;
    }

    return isClean;
}
#endif
