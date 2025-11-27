#pragma once

#include <QReadWriteLock>
#include <QUrlQuery>
#include <QUrl>
#include <QNetworkRequest>
#include <QObject>
#include <QLoggingCategory>
#include <QThread>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "Stream.h"

// Define the logging category
Q_DECLARE_LOGGING_CATEGORY(RESTClientLog)

#warning TODO offer method to set the networkmanager timeout time
/*
 * @brief A REST client for asynchronous HTTP requests.
 *
 * @note Definition :
 *   - \ref request: is an asynchronous HTTP operation that returns a single response
 *   - \ref stream: is a long-lived HTTP connection that continuously receives data over time
 * 
 * This class provides functionality to perform asynchronous HTTP requests using Qt's networking classes.
 * It supports GET, POST, PUT, and DELETE methods, and manages request IDs for tracking
 * requests. It also handles streams and emits signals for data reception and request completion.
 * This is a pure virtual class and must be inherited to be used.
 * 
 * In order to use this class, create a subclass that implements the emitDemuxer method
 */
class RESTClient : public QObject
{
    Q_OBJECT

public:
    /*
     * @brief Type alias for request ID.
     * This type is used to uniquely identify each request made by the RESTClient.
     */
    typedef size_t requestID_t;

    enum RequestStatus {
        UNSET,
        SUCCESS,
        TIMEOUT,
        ERROR
    };

    explicit RESTClient();
    ~RESTClient() = default; // TODO evaluate if default constriuctor is ok

    // starts the inner thread
    void start() { m_thread->start(); };

    // To monitor usage
    [[nodiscard]] qsizetype getTotalDataReceivedBytes() const { return m_totalDataReceivedBytes; };
    [[nodiscard]] bool isCleanedUp();

      // Stream count getter
    #warning todo : guard this
    [[nodiscard]] size_t getStreamCount() const { return m_networkReplyToOpenStreams.size(); }

signals:
    // Emited at basically every new message
    void totalDataReceivedBytesIncreased(qsizetype dataSize);

    void openStreamCountChanged(size_t count);
    void pendingAsyncRequestsCountChanges(size_t count);

private slots:
    void onReplyAsyncRequestReadyRead();
    void onReplyAsyncRequestFinished();
    void onReplyAsyncRequestErrorOccurred(QNetworkReply::NetworkError code, QNetworkReply *reply);

    void onReplyStreamReadyRead();
    void onReplyStreamFinished();
    void onReplyStreamErrorOccurred(QNetworkReply::NetworkError code, QNetworkReply *reply);

protected:

    // TODO explain why
    typedef size_t RequestTypeBase_t;
    typedef size_t StreamTypeBase_t;

    struct RequestInfo {
        RequestStatus status = RequestStatus::UNSET;
        requestID_t requestID = 0;
        RequestTypeBase_t type = 0;
    };

    enum class HttpMethod {
        GET,
        POST,
        PUT,
        DELETE
    };


    // Must be called before the first getInstance() call otherwise an assert is triggered in the constructor
    void setAPIKey(const QString &apiKey) { m_apiKey = apiKey; }

    [[nodiscard]] QNetworkRequest buildNetworkRequest(const QString &endpoint, const QUrlQuery &query = QUrlQuery()) const;

    /**
     * @brief Fetches data asynchronously.
     * @param request The network request.
     * @param type The request type integer.
     * @param method The HTTP method (default GET).
     * @param postData The POST data (default empty).
     * @param optArg Optional argument to hold in the request.
     * @return The request ID.
     */
    [[nodiscard]] requestID_t sendAsyncRequest(const QNetworkRequest &request, RequestTypeBase_t type, HttpMethod method = HttpMethod::GET, const QByteArray &postData = QByteArray());
    virtual void finalClassDemuxReceivedAsyncRequestReply(RequestTypeBase_t asyncRequestType, const QJsonDocument &doc, requestID_t requestID, RequestStatus status) = 0;

    [[nodiscard]] Stream* openStream(const QNetworkRequest &request, StreamTypeBase_t streamType);
    void closeStream(Stream * const stream);
    virtual void finalClassDemuxReceivedStreamReply(StreamTypeBase_t streamType, const QJsonDocument &doc, requestID_t requestID, RequestStatus status) = 0;



    QUrl m_baseUrl;
    requestID_t m_requestIDSeq = 0;
    mutable QReadWriteLock m_requestIDMapRWLock; // To protect m_requestIDSeq
    qsizetype m_totalDataReceivedBytes = 0;
    QString m_apiKey;
    QThread *m_thread;
    QNetworkAccessManager *m_networkManager;
    QMap<QNetworkReply*, RequestInfo> m_networkReplyToPendingAsyncRequests;
    QMap<QNetworkReply*, Stream*> m_networkReplyToOpenStreams;


private:
    friend class TestTSClient;
    friend class TestBarCache;
};
