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

class RESTClient : public QObject
{
    Q_OBJECT

public:
    enum RequestStatus {
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

signals:
    // Emited at basically every new message
    void totalDataReceivedBytesIncreased(qsizetype dataSize);

protected slots:
    void onReceivedNewAmountOfData(qsizetype bytes);

private slots:
    void onReplyReadyRead();
    void onReplyFinished();
    void onReplyErrorOccurred(QNetworkReply::NetworkError code, QNetworkReply *reply);

protected:

    typedef int RequestTypeInt; // TODO explain why
    struct RequestInfo {
        bool isStream = false;
        RequestStatus status = RequestStatus::ERROR;
        size_t requestID = 0;
        RequestTypeInt type = 0;
        void* optArg = nullptr;
    };
    enum class HttpMethod {
        GET,
        POST,
        PUT,
        DELETE
    };


    // Must be called before the first getInstance() call otherwise an assert is triggered in the constructor
    void setAPIKey(const QString &apiKey) { m_apiKey = apiKey; }

    // Static method to build refresh token request
    [[nodiscard]] static QNetworkRequest buildRefreshTokenRequest(const QString &clientId,
                                                                  const QString &clientSecret,
                                                                  const QString &refreshToken);

    [[nodiscard]] QNetworkRequest buildRequest(const QString &endpoint, const QUrlQuery &query = QUrlQuery()) const;

    /**
     * @brief Fetches data asynchronously.
     * @param request The network request.
     * @param type The request type integer.
     * @param method The HTTP method (default GET).
     * @param postData The POST data (default empty).
     * @param optArg Optional argument to hold in the request.
     * @return The request ID.
     */
    [[nodiscard]] size_t fetchAsync(const QNetworkRequest &request, RequestTypeInt type, HttpMethod method = HttpMethod::GET, const QByteArray &postData =  QByteArray(), void* optArg = nullptr);

    [[nodiscard]] QNetworkReply *fetchStream(const QNetworkRequest &request, void *arg);
    void closeStream(void *arg);

    virtual void emitSignalDemuxer(RequestTypeInt type, const QJsonDocument &doc, size_t requestID, RequestStatus status, void* optArg = nullptr) = 0;

    QUrl m_baseUrl;
    size_t m_requestIDSeq = 0;
    mutable QReadWriteLock m_requestIDMapRWLock; // To protect m_requestIDSeq
    qsizetype m_totalDataReceivedBytes = 0;
    QString m_apiKey;
    QThread *m_thread;
    QNetworkAccessManager *m_networkManager;
    QMap<QNetworkReply*, RequestInfo> m_pendingRequests;
    QMap<QNetworkReply*, Stream*> m_streams;


private:
    friend class TestTSClient;
    friend class TestBarCache;
};
