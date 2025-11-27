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

#define DEBUG    qCDebug(RESTClientLog)
#define INFO     qCInfo(RESTClientLog)
#define WARNING  qCWarning(RESTClientLog)
#define CRITICAL qCCritical(RESTClientLog)



RESTClient::RESTClient()
    : QObject(nullptr),
    m_thread(new QThread()),
    m_networkManager(new QNetworkAccessManager(this))
{
    this->moveToThread(m_thread);

    /*
    connect(m_networkManager, &QNetworkAccessManager::authenticationRequired, this, [](QNetworkReply *reply, QAuthenticator *authenticator) {
        qCCritical(RESTClientLog) << " Network Manager authentication required for reply : " << static_cast<void*>(reply);
        Q_ASSERT(false);
    });

    connect(m_networkManager, &QNetworkAccessManager::sslErrors, this, [](QNetworkReply *reply, const QList<QSslError> &errors) {
        qCCritical(RESTClientLog) << " Network Manager finished reply : " << static_cast<void*>(reply);
        for (const QSslError &error : errors) {
            qCCritical(RESTClientLog) << " SSL Error: " << error.errorString(); 
        }
        Q_ASSERT(false);
    });
    */

    connect(m_networkManager, &QNetworkAccessManager::finished, this, [](QNetworkReply *reply) {
        qCDebug(RESTClientLog) << " Network Manager finished reply : " << static_cast<void*>(reply);
    });
}


QNetworkRequest RESTClient::buildNetworkRequest(const QString &endpoint, const QUrlQuery &query) const
{
    Q_ASSERT_X(!m_baseUrl.isEmpty(), Q_FUNC_INFO, "Base URL is not set. Did you forget to set it before using the RESTClient?");
    Q_ASSERT_X(!m_apiKey.isEmpty(), Q_FUNC_INFO, "API key is not set. Did you forget to set it before using the RESTClient?");
    Q_ASSERT_X(!endpoint.isEmpty(), Q_FUNC_INFO, "Endpoint is empty. Did you forget to provide it?");
    Q_ASSERT_X(!endpoint.contains(QRegularExpression("%\\d+")), Q_FUNC_INFO, "Endpoint contains unresolved format placeholders");

    QUrl url(m_baseUrl);

    url.setPath(url.path() + endpoint);
    url.setQuery(query);

    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());

    return request;
}

/*
 * @brief Fetches data asynchronously.
 *
 * This function sends an asynchronous network request using the specified HTTP method and parameters.
 * By design, the thread executint this function will never be the Client thread itself, except for the token refresh
 */
RESTClient::requestID_t RESTClient::sendAsyncRequest(const QNetworkRequest &request, RequestTypeBase_t type, HttpMethod method, const QByteArray &postData)
{
    requestID_t requestID;

    m_requestIDMapRWLock.lockForWrite();
    {
        // TODO use atomic
        requestID = ++m_requestIDSeq;
    }
    m_requestIDMapRWLock.unlock();

    // Because this is a queud method invocation, the parameters have to be passed by value
    // TODO in the future, to avoid postData especialy, we could new it where it is build and deleted inside here
    QMetaObject::invokeMethod(this, [this, request, requestID, type, method, postData]() {
        Q_ASSERT_X(QThread::currentThread() == m_thread, "lambda", "The RESTClient thread has to be the one executing this method");

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

        Q_ASSERT(!m_networkReplyToPendingAsyncRequests.contains(reply)); // Paranoia

        auto a = connect(reply, &QNetworkReply::readyRead, this, &RESTClient::onReplyAsyncRequestReadyRead);
        Q_ASSERT(a);
        auto b = connect(reply, &QNetworkReply::finished, this, &RESTClient::onReplyAsyncRequestFinished);
        Q_ASSERT(b);
        auto c = connect(reply, &QNetworkReply::errorOccurred, this, &RESTClient::onReplyAsyncRequestErrorOccurred);
        Q_ASSERT(c);


        m_networkReplyToPendingAsyncRequests[reply] =
            {
                .status = RequestStatus::UNSET,
                .requestID = requestID, 
                .type = type
            };

        emit pendingAsyncRequestsCountChanges(m_networkReplyToOpenStreams.size());

        qCDebug(RESTClientLog) << "Sent request to Network Manager and registered: request_id=" << requestID << " with reply addr=" << static_cast<void*>(reply);

    }, Qt::QueuedConnection);

    return requestID;
}


Stream* RESTClient::openStream(const QNetworkRequest &request, StreamTypeBase_t streamType)
{
    Q_ASSERT_X(QThread::currentThread() != m_thread, "lambda", "The RESTClient thread by design shall never be the one opening a stream, always another thread");

    Stream * stream = nullptr;

    QMetaObject::invokeMethod(this,
        [this, &request, &stream]()
        {
            stream = new Stream(this);
            Q_CHECK_PTR(stream);

            QNetworkReply *reply = m_networkManager->get(request);
            Q_CHECK_PTR(reply);

            Q_ASSERT(!m_networkReplyToOpenStreams.contains(reply)); // Paranoia

            auto a = connect(reply, &QNetworkReply::readyRead, this, &RESTClient::onReplyStreamReadyRead);
            Q_ASSERT(a);
            auto b = connect(reply, &QNetworkReply::finished, this, &RESTClient::onReplyAsyncRequestFinished);
            Q_ASSERT(b);
            auto c = connect(reply, &QNetworkReply::errorOccurred, this, &RESTClient::onReplyStreamErrorOccurred);
            Q_ASSERT(c);

            m_networkReplyToOpenStreams[reply] = stream;
                
            // Emit signal that stream count has changed
            emit openStreamCountChanged(m_networkReplyToOpenStreams.size());
        },
    Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
                                   // finishes executing this lambda so that a valid pointer is returned

    qCDebug(RESTClientLog) << Q_FUNC_INFO << "Opened stream = " << static_cast<void*>(stream) << "with URL : " << request.url();

    return stream;
}

void RESTClient::closeStream(Stream * const stream)
{
    Q_ASSERT(stream != nullptr);

    QMetaObject::invokeMethod(this,
        [this, &stream]()
        {
            Q_ASSERT_X(QThread::currentThread() == m_thread, Q_FUNC_INFO, "Closing the stream is only meant to be called from RESTClient thread");

            QNetworkReply *replyToDelete = nullptr;

            qCDebug(RESTClientLog) << Q_FUNC_INFO << "Going through all pending replies";


            for (auto it = m_networkReplyToOpenStreams.constBegin(); it != m_networkReplyToOpenStreams.constEnd(); ++it) {
                QNetworkReply *reply = it.key();

                qCDebug(RESTClientLog) << Q_FUNC_INFO << "Checking reply " << static_cast<void*>(reply);

                if (stream == it.value()) {

                    replyToDelete = reply;

                    qCDebug(RESTClientLog) << Q_FUNC_INFO << "Found thre reply for stream :" << static_cast<void*>(reply);

                    break;
               }
            }

            Q_ASSERT_X(replyToDelete != nullptr, Q_FUNC_INFO, "Double close on stream");

            qCDebug(RESTClientLog) << Q_FUNC_INFO << "Aborting request";
     
            replyToDelete->abort();
            replyToDelete->deleteLater();

            bool removed = m_networkReplyToOpenStreams.remove(replyToDelete);
            Q_ASSERT_X(removed, "Q_FUNC_INFO", "Corruption if not removed");
     
            delete stream;

            emit openStreamCountChanged(m_networkReplyToOpenStreams.size());
        },
    Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
                                   // finishes executing this lambda so that a valid pointer is returned
}


void RESTClient::onReplyAsyncRequestReadyRead()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_CHECK_PTR(reply);

    Q_ASSERT(false);
}

void RESTClient::onReplyAsyncRequestFinished()
{
    QJsonParseError parseError;
    QJsonDocument doc;

    Q_ASSERT(QThread::currentThread() == m_thread); // Paranoia

    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_CHECK_PTR(reply);

    Q_ASSERT(m_networkReplyToPendingAsyncRequests.contains(reply));

    DEBUG << " reply of request " << static_cast<void*>(reply);

    QByteArray rawData = reply->readAll();
    {
        qsizetype bytesReceived = rawData.size();

        m_totalDataReceivedBytes += bytesReceived;

        emit totalDataReceivedBytesIncreased(m_totalDataReceivedBytes);

        qCDebug(RESTClientLog) << Q_FUNC_INFO << " : Received " << bytesReceived << " bytes, total now " << m_totalDataReceivedBytes << " bytes";
    }
    

    RequestInfo * const requestInfo = &m_pendingRequests[reply];

    #warning fuck this
    Q_ASSERT(requestInfo->isStream == false);

    requestInfo->status = RequestStatus::ERROR;

    switch (reply->error())
    {
        case QNetworkReply::NoError:
            requestInfo->status = RequestStatus::SUCCESS;
            break;

        case QNetworkReply::HostNotFoundError:
        case QNetworkReply::UnknownNetworkError:
            requestInfo->status = RequestStatus::TIMEOUT;
            qCCritical(RESTClientLog) << Q_FUNC_INFO << " : Error with the reply " << static_cast<void*>(reply) << " : " << reply->errorString() << " : " << reply->error();
            goto notify;
            break;

        case QNetworkReply::ContentNotFoundError:
            if (requestInfo->type == 3) {
                // IMPORTANT EDGE CASE
                // Its possible in the case of getBars for example to receive this, as its possible to ask for a range of bars
                // in the after market for instance where there just isnt any bars
                requestInfo->status = RequestStatus::SUCCESS;
            } else {
                requestInfo->status = RequestStatus::ERROR;
                qCCritical(RESTClientLog) << Q_FUNC_INFO << " : Error with the reply " << static_cast<void*>(reply) << " : " << reply->errorString() << " : " << reply->error();
                goto notify;
            }
            break;

        default:
            requestInfo->status = RequestStatus::ERROR;

            qCCritical(RESTClientLog) << Q_FUNC_INFO << " : Error with the reply " << static_cast<void*>(reply) << " : " << reply->errorString() << " : " << reply->error();
            goto notify;
            break;
    };

    doc = QJsonDocument::fromJson(rawData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        requestInfo->status = RequestStatus::ERROR;
        qCWarning(RESTClientLog) << Q_FUNC_INFO << "Failed to parse JSON:" << parseError.errorString();
        qCWarning(RESTClientLog) << Q_FUNC_INFO << "Content of the bad data : " << rawData;
        goto notify;
    }

    if (doc.isNull()){
        requestInfo->status = RequestStatus::ERROR;
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

void RESTClient::onReplyAsyncRequestErrorOccurred(QNetworkReply::NetworkError code, QNetworkReply *reply)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_CHECK_PTR(reply);

    Q_ASSERT_X(false, Q_FUNC_INFO, "Not supposed to happen");
}






void RESTClient::onReplyStreamReadyRead()
{
   Q_ASSERT(QThread::currentThread() == m_thread); // Paranoia

    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_CHECK_PTR(reply);

    Q_ASSERT_X(m_pendingRequests.contains(reply), Q_FUNC_INFO, "The reply must be present in the pendingRequests map");

    RequestInfo * const requestInfo = &m_pendingRequests[reply];

    Q_ASSERT_X(requestInfo->isStream == true, Q_FUNC_INFO, "This method is only meant to be called for stream requests");


    QByteArray rawData = reply->readAll();
    {
        qsizetype bytesReceived = rawData.size();

        m_totalDataReceivedBytes += bytesReceived;

        emit totalDataReceivedBytesIncreased(m_totalDataReceivedBytes);

        qCDebug(RESTClientLog) << Q_FUNC_INFO << " : Received " << bytesReceived << " bytes, total now " << m_totalDataReceivedBytes << " bytes";
    }

    Stream * const stream = m_streams[reply];
    Q_CHECK_PTR(stream);
    const QString streamName = stream->objectName();

    while(true) {
        int delimiterPos = stream->accumulatedData.indexOf('\n');
        if (delimiterPos == -1) {
            // No complete object yet, wait for more data
            //TODO implement metric
            break;
        }

        QByteArray jsonData = stream->accumulatedData.left(delimiterPos + 1);
        stream->accumulatedData.remove(0, delimiterPos + 1); // Remove extracted data from buffer

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

        // Write raw data to recording file if recording is active

        if (parseError.error != QJsonParseError::NoError) {
            qCCritical(StreamLog) << streamName << "Failed to parse JSON:" << parseError.errorString();
            qCDebug(StreamLog) << streamName << "Raw data : " << jsonData;
            continue;
        }

        QJsonObject jsonObj = doc.object();

        if (jsonObj.contains("Heartbeat") && jsonObj.contains("Timestamp")) {
            heartbeatTimer.start(timeoutMS);
            qCDebug(StreamLog) << streamName << "received heartbeat";
        }
        else if (jsonObj.contains("Error") && jsonObj.contains("Message")) {
            streamIsInError = true;
            StreamError error;
            QString errorStr = jsonObj["Error"].toString();

            if (errorStr == "BadRequest"){
                error = StreamError::BadRequest;
            } else if (errorStr == "DualLogon") {
                error = StreamError::DualLogon;
            } else if (errorStr == "GoAway") {
                error = StreamError::GoAway;
            } else if (errorStr == "InternalServerError") {
                error = StreamError::InternalServerError;
            } else if (errorStr == "InvalidSymbol"){
                error = StreamError::InvalidSymbol;
            } else {
                error = StreamError::Unknown;
                qCCritical(StreamLog) << streamName << "received unknown error string: " << errorStr;
            }

            qCCritical(StreamLog) << streamName << " received an error : " << jsonObj["Message"].toString();

            emit streamErrorOccurred(error, jsonObj["Message"].toString());
        }
        else { // Happy path, process the object
            if (processJsonObject(jsonObj)) {
                heartbeatTimer.start(timeoutMS);
            } else {
                qCCritical(StreamLog) << "The stream " << streamName << " failed to process Json object";
            }
        }
    }
}

void RESTClient::onReplyStreamFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_CHECK_PTR(reply);

    Q_ASSERT_X(false, Q_FUNC_INFO, "Not supposed to happen");
}

void RESTClient::onReplyStreamErrorOccurred(QNetworkReply::NetworkError code, QNetworkReply *reply)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_CHECK_PTR(reply);

    Q_ASSERT_X(false, Q_FUNC_INFO, "Not supposed to happen");
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
