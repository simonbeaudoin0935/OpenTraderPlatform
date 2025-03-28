#ifndef RESTCLIENT_H
#define RESTCLIENT_H
#include <QMutex>
#include <QSemaphore>
#include <QWaitCondition>
#include <QHash>

#include <QObject>
#include <atomic>
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
    explicit RESTClient(const QString &baseUrl, QObject *parent = nullptr);
    virtual ~RESTClient();

    // Must be called before the first getInstance() call otherwise an assert is triggered in the constructor
    static void setAPIKey(const QString &apiKey);

    // To monitor usage
    qsizetype getTotalDataReceivedBytes() const;

signals:
    // Emited at basically every new message
    void totalDataReceivedBytesIncreased(qsizetype dataSize);

private slots:
    void onReplyFinished(QNetworkReply *reply);

protected:
    enum class RequestSynchronicity { Async, Sync };
    typedef int RequestTypeInt; // TODO explain why
    const int RequestTypeNone = 0;
    struct RequestInfo {
        RequestSynchronicity synchronicity;
        RequestTypeInt type;
        bool completed = false;
        QJsonArray *jsonArray = nullptr;
    };

    QString buildUrlWithEndpoint(const QString &endpoint) const;
    QString buildUrlWithEndpointAndSymbol(const QString &endpoint, const QString &symbol) const;
    QString buildUrlWithEndpointAndParamsList(const QString &endpoint, const QString &paramsList) const;

    bool fetchSync(const QString &url, QJsonArray *&jsonArrayFromReplyToDelete);
    void fetchAsync(const QString &url, RequestTypeInt type);

    virtual void emitSignalDemuxer(RequestTypeInt type, const QJsonArray &doc) = 0;

    std::atomic<qsizetype> totalDataReceivedBytes = 0; // TODO at the end, will do in private
    static QString apiKey;
    QThread *thread;
    QNetworkAccessManager *manager;
    mutable QMutex pendingRequestsMutex;
    QWaitCondition waitCondition;
    QHash<QNetworkReply*, RequestInfo> pendingRequests;

#ifdef UNIT_TESTING
    int allocated_json_arrays = 0;
#endif

private:
    friend class TestFMPClient;
    friend class TestTradeStationClient;

    const QString baseUrl;
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
