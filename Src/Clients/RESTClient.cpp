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

RESTClient::RESTClient()
    : QObject(nullptr),
    m_thread(new QThread()),
    m_networkManager(new QNetworkAccessManager(this))
{
    this->moveToThread(m_thread);

    connect(m_networkManager, &QNetworkAccessManager::finished, this, &RESTClient::onReplyFinished);
}


QNetworkRequest RESTClient::buildRequest(const QString &endpoint, const QUrlQuery &query) const
{
    QUrl url(m_baseUrl);

    Q_ASSERT_X(!m_baseUrl.isEmpty(), Q_FUNC_INFO, "Base URL is not set. Did you forget to set it before using the RESTClient?");
    Q_ASSERT_X(!m_apiKey.isEmpty(), Q_FUNC_INFO, "API key is not set. Did you forget to set it before using the RESTClient?");
    Q_ASSERT_X(!endpoint.isEmpty(), Q_FUNC_INFO, "Endpoint is empty. Did you forget to provide it?");
    Q_ASSERT_X(!endpoint.contains(QRegularExpression("%\\d+")), Q_FUNC_INFO, "Endpoint contains unresolved format placeholders");

    url.setPath(url.path() + endpoint);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());

    return request;
}

size_t RESTClient::fetchAsync(const QNetworkRequest &request, RequestTypeInt type, HttpMethod method, const QByteArray &postData, void *optArg)
{
    size_t requestID;
    m_requestIDMapRWLock.lockForWrite();
    {
        requestID = ++m_requestIDSeq;
    }
    m_requestIDMapRWLock.unlock();

    // Because this is a queud method invocation, the parameters have to be passed by value
    // TODO in the future, to avoid postData especialy, we could new it where it is build and deleted inside here
    QMetaObject::invokeMethod(this, [this, request, requestID, type, method, postData, optArg]() {
        Q_ASSERT_X(QThread::currentThread() == m_thread, "lambda", "TSClient object has to be the one executing this method");

        QNetworkReply *reply = nullptr;
        
        switch (method) {
            case HttpMethod::GET:
                reply = m_networkManager->get(request);
                break;
            case HttpMethod::POST:
                // Use the provided postData if available, otherwise send empty data
                reply = m_networkManager->post(request, postData);
                break;
            case HttpMethod::PUT:
                Q_ASSERT_X(0, "fetchAsync()", "No feature require a PUT method, this is a bugS");
                break;
            case HttpMethod::DELETE:
                reply = m_networkManager->deleteResource(request);
                break;
            default:
                Q_UNREACHABLE();
                break;
        }

        Q_CHECK_PTR(reply);

        m_pendingRequests[reply] = {.isStream = false,
                                    .status = RequestStatus::SUCCESS,
                                    .requestID = requestID, 
                                    .type = type,
                                    .optArg = optArg};

        qCDebug(RESTClientLog) << "Sent request to Network Manager and registered: request_id=" << requestID << 
            " method=" << [method]() {
            switch (method) {
                case HttpMethod::GET: return "GET";
                case HttpMethod::POST: return "POST";
                case HttpMethod::PUT: return "PUT";
                case HttpMethod::DELETE: return "DELETE";
                default: return "UNKNOWN";
            }
        }() << " with reply addr=" << static_cast<void*>(reply);

    }, Qt::QueuedConnection);

    return requestID;
}

// This is all a bit hacky for now...
QNetworkReply* RESTClient::fetchStream(const QNetworkRequest &request, void* optArg)
{
    Q_ASSERT(optArg != nullptr);

    // Fetch stream is only meant to be called from TSClient's thread
    Q_ASSERT(QThread::currentThread() == m_thread);


    QNetworkReply *reply = m_networkManager->get(request);

    Q_ASSERT(reply != nullptr);



    m_pendingRequests[reply] = {.isStream = true,
                                .status = RequestStatus::SUCCESS,
                                .requestID = 0, 
                                .type = 0,
                                .optArg = optArg};


    qCDebug(RESTClientLog) << Q_FUNC_INFO <<
        "GET network reply = " << static_cast<void*>(reply) <<
        " with URL : " << request.url() <<
        " and header : "; /* << request.headers() << */

    return reply;
}

void RESTClient::closeStream(void *arg)
{
    QNetworkReply *replyToDelete = nullptr;

    Q_ASSERT(arg != nullptr);

    Q_ASSERT_X(QThread::currentThread() == m_thread, Q_FUNC_INFO, "Closing the stream is only meant to be called from RESTClient thread");

    
    qCDebug(RESTClientLog) << Q_FUNC_INFO << "Going through all pending replies";


    for (auto it = m_pendingRequests.constBegin(); it != m_pendingRequests.constEnd(); ++it) {
        QNetworkReply *reply = it.key();

        qCDebug(RESTClientLog) << Q_FUNC_INFO << "Checking reply " << static_cast<void*>(reply);

        const RequestInfo &info = it.value();
        if (info.optArg == arg) {

            // Paranoia. If we found the pendingRequest for which the stream arg corresponds, it has to have
            // its type to stream
            Q_ASSERT(info.isStream == true);

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


        bool removed = m_pendingRequests.remove(replyToDelete);
        if (removed) {
            qCDebug(RESTClientLog) << Q_FUNC_INFO << " normal";
        } else {
            qCDebug(RESTClientLog) << Q_FUNC_INFO << " request not removed because it appears that the stream was manually closed before";
        }
    }
}

void RESTClient::onReplyFinished(QNetworkReply *reply) {
    QJsonDocument doc;
    QJsonParseError parseError;

    Q_ASSERT(QThread::currentThread() == m_thread); // Paranoia
    Q_ASSERT_X(m_pendingRequests.contains(reply), Q_FUNC_INFO, "The reply must be present in the pendingRequests map");

    qCDebug(RESTClientLog) << Q_FUNC_INFO << " : Thread [" << QThread::currentThread()->objectName() << "] working on reply of request " << static_cast<void*>(reply);

    QByteArray rawData = reply->readAll();

    {
        qsizetype bytesReceived = rawData.size();

        m_totalDataReceivedBytes += bytesReceived;

        emit totalDataReceivedBytesIncreased(m_totalDataReceivedBytes);

        qCDebug(RESTClientLog) << Q_FUNC_INFO << " : Received " << bytesReceived << " bytes, total now " << m_totalDataReceivedBytes << " bytes";
    }
    

    // copying the struct so we can use it outside the RW lock
    RequestInfo *requestInfo = &m_pendingRequests[reply];

    // Those are the default values, just being paranoid that we are consistent and nothing else corrupted those before us
    //Q_ASSERT(requestInfo->completed == false);

    doc = QJsonDocument::fromJson(rawData, &parseError);

    if (reply->error() != QNetworkReply::NoError) {
        qCCritical(RESTClientLog) << Q_FUNC_INFO << " : Error with the reply " << static_cast<void*>(reply) << " : " << reply->errorString() << " : " << reply->error();
        qCCritical(RESTClientLog).noquote() << Q_FUNC_INFO << " : Content of the reply : \n" << doc.toJson(QJsonDocument::Indented);

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
    //requestInfo->completed = true;

notify:
    if (requestInfo->isStream) {
        qCCritical(RESTClientLog) << Q_FUNC_INFO << "Removing network reply " << static_cast<void*>(reply) << " for stream " << requestInfo->optArg;
    } else {
        // The request might have failed, this info is passed along
        emitSignalDemuxer(requestInfo->type, doc, requestInfo->requestID, requestInfo->status, requestInfo->optArg);
    }

    // Whether the request was successful or not, take it out of the map
    bool removed = m_pendingRequests.remove(reply);
    Q_ASSERT(removed);

    reply->deleteLater();
}

void RESTClient::onReceivedNewAmountOfData(qsizetype bytes)
{
    m_totalDataReceivedBytes += bytes;

    emit totalDataReceivedBytesIncreased(m_totalDataReceivedBytes);
}

#ifdef UNIT_TESTING
bool RESTClient::isCleanedUp()
{
    bool isClean = true;

    //pendingRequestsRWLock.lockForRead();
    {
        if (!m_pendingRequests.isEmpty()) {
            qCWarning(RESTClientLog) << Q_FUNC_INFO << " : ********************* pendingRequests not empty ****************";
            isClean = false;

            size_t i = 0;
            for (const auto& request : m_pendingRequests) {
                qDebug() << "Request info #" << i;
                qDebug() << "  RequestType  : " << request.type;
                //qDebug() << "  Completed    : " << ((request.completed) ? "TRUE" : "FALSE");
            }
        }
    }
    //pendingRequestsRWLock.unlock();

    return isClean;
}
#endif
