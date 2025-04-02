#ifndef RESTCLIENT_H
#define RESTCLIENT_H
#include <QReadWriteLock>
#include <QSemaphore>
#include <QHash>
#include <QUrlQuery>
#include <QUrl>
#include <QNetworkRequest>

#include <QObject>
#include <QLoggingCategory>
#include <QThread>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#ifdef UNIT_TESTING
#define TRACK_NEW_JSON_ARRAY(x) x; allocated_json_arrays++;
#define TRACK_DELETED_JSON_ARRAY(x) x; allocated_json_arrays--;
#else
#define TRACK_NEW_JSON_ARRAY(x) x;
#define TRACK_DELETED_JSON_ARRAY(x) x;
#endif

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

    // Must be called before the first getInstance() call otherwise an assert is triggered in the constructor
    void setAPIKey(const QString &apiKey);

    // To monitor usage
    qsizetype getTotalDataReceivedBytes() const;

signals:
    // Emited at basically every new message
    void totalDataReceivedBytesIncreased(qsizetype dataSize);

protected slots:
    void onReceivedNewAmountOfData(qsizetype bytes);

private slots:
    void onReplyFinished(QNetworkReply *reply);

protected:
    enum class RequestSynchronicity { Async, Sync, Stream };
    typedef int RequestTypeInt; // TODO explain why
    const int RequestTypeNone = 0;
    struct RequestInfo {
        RequestSynchronicity synchronicity;
        RequestTypeInt type;
        bool completed = false;
        QJsonDocument *jsonDocument = nullptr;
        void* optArg = nullptr;
    };

    enum class ApiKeyPlacement {
        InUrl,      // API key is sent as a URL parameter
        InHeader    // API key is sent in the Authorization header
    };

    enum class HttpMethod {
        GET,
        POST
    };

    // Static method to build refresh token request
    static QNetworkRequest buildRefreshTokenRequest(const QString &clientId,
                                                    const QString &clientSecret,
                                                    const QString &refreshToken);

    // Overloaded function to build network requests
    QNetworkRequest buildRequest(ApiKeyPlacement placement, const QString &endpoint, const QString &symbol = "") const;
    QNetworkRequest buildRequest(ApiKeyPlacement placement, const QString &endpoint, const QUrlQuery &query) const;


    bool fetchSync(const QNetworkRequest &request, QJsonDocument *&jsonDocumentFromReplyToDelete, HttpMethod method = HttpMethod::GET, const QByteArray &postData = QByteArray());
    void fetchAsync(const QNetworkRequest &request, RequestTypeInt type, HttpMethod method = HttpMethod::GET, const QByteArray &postData =  QByteArray());

    QNetworkReply *fetchStream(const QNetworkRequest &request, void *arg);
    void closeStream(void *arg);

    virtual void emitSignalDemuxer(RequestTypeInt type, const QJsonDocument &doc) = 0;

    virtual void processStreamFinished(QByteArray &rawData, void* arg); // for stream
    virtual void processStreamReadyRead(QByteArray &rawData, void* arg); // for stream

    qsizetype totalDataReceivedBytes = 0;
    QString apiKey;
    QThread *thread;
    QNetworkAccessManager *manager;
    mutable QReadWriteLock pendingRequestsRWLock;
    QMap<QNetworkReply*, RequestInfo> pendingRequests;

#ifdef UNIT_TESTING
    int allocated_json_arrays = 0;
#endif

private:
    friend class TestFMPClient;
    friend class TestTSClient;

    const QUrl baseUrl;
    QLoggingCategory *loggingCategory;

#ifdef UNIT_TESTING
    bool simulate_reply_network_latency = false;
    bool isCleanedUp();
    QSemaphore onReplyFinished_sem;
    unsigned long fetchSyncTimeoutMs = 5000;
#else
    const unsigned long fetchSyncTimeoutMs = 5000; // Const under normal operation
#endif
};

#endif // RESTCLIENT_H 
