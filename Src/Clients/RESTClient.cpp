#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#ifdef UNIT_TESTING
#include <QtTest/QtTest>
#endif

#include "RESTClient.h"
#include "Stream.h" // TODO I dont like having to include this header here

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


QNetworkRequest RESTClient::buildRequest(const QString &endpoint, const QUrlQuery &query) const
{
    QUrl url = baseUrl;
    url.setPath(url.path() + endpoint);
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    url.setQuery(query);
    request.setUrl(url);
    
    return request;
}

int RESTClient::fetchAsync(const QNetworkRequest &request, RequestTypeInt type, HttpMethod method, const QByteArray &postData, void *optArg) {

    // Because this is a queud method invocation, the parameters have to be passed by value
    // TODO in the future, to avoid postData especialy, we could new it where it is build and deleted inside here
    QMetaObject::invokeMethod(this, [this, request, type, method, postData, optArg]() {
        QNetworkReply *reply = nullptr;
        
        switch (method) {
            case HttpMethod::GET:
                reply = manager->get(request);
                break;
            case HttpMethod::POST:
                // Use the provided postData if available, otherwise send empty data
                reply = manager->post(request, postData);
                break;
            case HttpMethod::PUT:
                Q_ASSERT_X(0, "fuck", "fuckkk");
                break;
            case HttpMethod::DELETE:
                reply = manager->deleteResource(request);
                break;
            default:
                Q_UNREACHABLE();
                break;
        }

        Q_CHECK_PTR(reply);

        qCDebug(RESTClientLog) << Q_FUNC_INFO <<
            " : Thread [" << QThread::currentThread()->objectName() <<
            "] executed the queued" << (method == HttpMethod::GET ? "GET" : "POST") <<
            "request and registered the reply " << static_cast<void*>(reply) << " for later reception.";


        pendingRequests[reply] = { .async_type = RequestType::Async,
                                   .type = type,
                                   .completed = false,
                                   .jsonDocument = nullptr,
                                   .optArg = optArg};
    }, Qt::QueuedConnection);


    qCDebug(RESTClientLog) << Q_FUNC_INFO <<
        " : Thread [" << QThread::currentThread()->objectName() <<
        "] invoked the queued method to" << (method == HttpMethod::GET ? "GET" : "POST") <<
        " with URL " << request.url().toString() <<
        " header : " << /*request.headers() << */
        " and data : " << (method == HttpMethod::POST ? "<redacted for security>" : postData);


    //FIXME return a proper request ID
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
        " and header : "; /* << request.headers() << */


    pendingRequests[reply] = { .async_type = RequestType::Stream,
                               .type = 0,
                               .completed = false,
                               .jsonDocument = nullptr,
                               .optArg = arg};

    return reply;
}

void RESTClient::closeStream(void *arg)
{
    QNetworkReply *replyToDelete = nullptr;

    Q_ASSERT(arg != nullptr);

    Q_ASSERT_X(QThread::currentThread() == thread, Q_FUNC_INFO, "Closing the stream is only meant to be called from RESTClient thread");


    qCDebug(RESTClientLog) << Q_FUNC_INFO << "Going through all pending replies";


    for (auto it = pendingRequests.constBegin(); it != pendingRequests.constEnd(); ++it) {
        QNetworkReply *reply = it.key();

        qCDebug(RESTClientLog) << Q_FUNC_INFO << "Checking reply " << static_cast<void*>(reply);

        const RequestInfo &info = it.value();
        if (info.optArg == arg) {

            // Paranoia. If we found the pendingRequest for which the stream arg corresponds, it has to have
            // its type to stream
            Q_ASSERT(info.async_type == RequestType::Stream);

            replyToDelete = reply;

            qCDebug(RESTClientLog) << Q_FUNC_INFO << "Found that this reply has stream  " << static_cast<void*>(arg) << " attached";

            break;
        }
    }

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


        bool removed = pendingRequests.remove(replyToDelete);
        if (removed) {
            qCDebug(RESTClientLog) << Q_FUNC_INFO << " normal";
        } else {
            qCDebug(RESTClientLog) << Q_FUNC_INFO << " request not removed because it appears that the stream was manually closed before";
        }
    }
}

void RESTClient::onReplyFinished(QNetworkReply *reply) {
    QJsonDocument doc;
    RequestInfo *requestInfo;
    QJsonParseError parseError;

    Q_ASSERT(QThread::currentThread() == thread); // Paranoia

    QByteArray rawData = reply->readAll();
    qsizetype bytesReceived = rawData.size();

    totalDataReceivedBytes += bytesReceived;

    emit totalDataReceivedBytesIncreased(totalDataReceivedBytes);

    qCDebug(RESTClientLog) << Q_FUNC_INFO << " : Received " << bytesReceived << " bytes, total now " << totalDataReceivedBytes << " bytes";

    Q_ASSERT_X(pendingRequests.contains(reply), Q_FUNC_INFO, "The reply must be present in the pendingRequests map");

    // copying the struct so we can use it outside the RW lock
    requestInfo = &pendingRequests[reply];

    qCDebug(RESTClientLog) << Q_FUNC_INFO <<
        " : Thread [" << QThread::currentThread()->objectName() <<
        "] working on reply of request " << static_cast<void*>(reply);

    // Those are the default values, just being paranoid that we are consistent and nothing else corrupted those before us
    Q_ASSERT(requestInfo->jsonDocument == nullptr);
    Q_ASSERT(requestInfo->completed == false);

    doc = QJsonDocument::fromJson(rawData, &parseError);

    if (reply->error() != QNetworkReply::NoError) {
        qCCritical(RESTClientLog) << Q_FUNC_INFO <<
            " : Error with the reply " << static_cast<void*>(reply) << " : " << reply->errorString() << " : " << reply->error();
        qCCritical(RESTClientLog).noquote() << Q_FUNC_INFO <<
            " : Content of the reply : \n" << doc.toJson(QJsonDocument::Indented);

        if (reply->error() == QNetworkReply::ContentNotFoundError) {
            // Its possible in the case of getBars for example to receive this, as its possible to ask for a range of bars
            // in the after market for instance where there just isnt any bars
        } else {
            goto notify;
        }
    }

    if (parseError.error != QJsonParseError::NoError) {
        qCWarning(RESTClientLog) << Q_FUNC_INFO << "Failed to parse JSON:" << parseError.errorString();
        qCWarning(RESTClientLog) << Q_FUNC_INFO << "Content of the bad data : " << rawData;
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

notify:
    if (requestInfo->async_type == RequestType::Async) {
        // The request might have failed, this info is passed along
        emitSignalDemuxer(requestInfo->type, doc, requestInfo->completed, requestInfo->optArg);

        // Whether the request was successful or not, take it out of the map
        bool removed = pendingRequests.remove(reply);
        Q_ASSERT(removed);

    } else if (requestInfo->async_type == RequestType::Stream) {
        qCDebug(RESTClientLog) << Q_FUNC_INFO << "Removing network reply " << static_cast<void*>(reply) << " for stream " << requestInfo->optArg;

        bool removed = pendingRequests.remove(reply);
        Q_ASSERT(removed);
    } else {
        Q_UNREACHABLE();
    }

    reply->deleteLater();
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

    //pendingRequestsRWLock.lockForRead();
    {
        if (!pendingRequests.isEmpty()) {
            qCWarning(RESTClientLog) << Q_FUNC_INFO << " : ********************* pendingRequests not empty ****************";
            isClean = false;

            size_t i = 0;
            for (const auto& request : pendingRequests) {
                qDebug() << "Request info #" << i;
                qDebug() << "  Syncronicity : " << ((request.async_type == RequestType::Async) ? "ASYNC" : "SYNC");
                qDebug() << "  RequestType  : " << request.type;
                qDebug() << "  Completed    : " << ((request.completed) ? "TRUE" : "FALSE");
                qDebug() << "  Docptr       : " << static_cast<void*>(request.jsonDocument);
            }
        }
    }
    //pendingRequestsRWLock.unlock();

    return isClean;
}
#endif
