#pragma once

#include <QUrlQuery>
#include <QUrl>
#include <QNetworkRequest>
#include <QObject>
#include <QLoggingCategory>
#include <QThread>
#include <QNetworkAccessManager>
#include <QNetworkReply>


// Define the logging category
Q_DECLARE_LOGGING_CATEGORY(RESTClientLog)

class RESTClient : public QObject
{
    Q_OBJECT

public:
    explicit RESTClient(const QUrl &baseUrl, QObject *parent = nullptr);
    virtual ~RESTClient();

    // starts the inner thread
    void start();

    // To monitor usage
    qsizetype getTotalDataReceivedBytes() const;
    bool isCleanedUp();

signals:
    // Emited at basically every new message
    void totalDataReceivedBytesIncreased(qsizetype dataSize);

protected slots:
    void onReceivedNewAmountOfData(qsizetype bytes);

private slots:
    void onReplyFinished(QNetworkReply *reply);

protected:
    // Must be called before the first getInstance() call otherwise an assert is triggered in the constructor
    void setAPIKey(const QString &apiKey);

    enum class RequestType { Async, Stream };
    typedef int RequestTypeInt; // TODO explain why
    const int RequestTypeNone = 0;
    struct RequestInfo {
        RequestType async_type;
        RequestTypeInt type;
        bool completed = false;
        void* optArg = nullptr;
    };

    enum class HttpMethod {
        GET,
        POST,
        PUT,
        DELETE
    };

    // Static method to build refresh token request
    static QNetworkRequest buildRefreshTokenRequest(const QString &clientId,
                                                    const QString &clientSecret,
                                                    const QString &refreshToken);

    // Overloaded function to build network requests
    QNetworkRequest buildRequest(const QString &endpoint, const QString &symbol = "") const;
    QNetworkRequest buildRequest(const QString &endpoint, const QUrlQuery &query) const;

    [[nodiscard]] int fetchAsync(const QNetworkRequest &request, RequestTypeInt type, HttpMethod method = HttpMethod::GET, const QByteArray &postData =  QByteArray(), void* optArg = nullptr);

    QNetworkReply *fetchStream(const QNetworkRequest &request, void *arg);
    void closeStream(void *arg);

    virtual void emitSignalDemuxer(RequestTypeInt type, const QJsonDocument &doc, bool completed, void* optArg = nullptr) = 0;

    qsizetype totalDataReceivedBytes = 0;
    QString apiKey;
    QThread *thread;
    QNetworkAccessManager *manager;
    QMap<QNetworkReply*, RequestInfo> pendingRequests;


private:
    friend class TestTSClient;
    friend class TestBarCache;

    const QUrl baseUrl;
};
