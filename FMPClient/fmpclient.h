#ifndef FMPCLIENT_H
#define FMPCLIENT_H

#include <QObject>
#include <QMutex>
#include <QSemaphore>
#include <QWaitCondition>
#include <QHash>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(FMPClientLog)

// This is a singleton

class QThread;
class QNetworkAccessManager;
class QNetworkReply;

class FMPClient : public QObject {
    Q_OBJECT
public:

    // Singleton : instance getter
    static FMPClient& getInstance();

    // Must be called before the first getInstance() call
    static void setAPIKey(const QString &apiKey);

    // API data fetchers
    void fetchAsyncQuote(const QString &symbol);
    bool fetchSyncQuote(const QString &symbol, double &price, double &bid, double &ask);

    void fetchAsyncSharesFloat(const QString &symbol);
    bool fetchSyncSharesFloat(const QString &symbol, QString &date, double &freeFloat, double &floatShares, double &outstandingShares);

    // Singleton : Delete copy constructor and assignment operator
    FMPClient(const FMPClient&) = delete;
    FMPClient& operator=(const FMPClient&) = delete;

signals:
    // API Async version signals
    void quoteReceived(QString symbol, double price, double bid, double ask);
    void sharesFloatReceived(QString symbol, QString date, double freeFloat, double floatShares, double outstandingShares);

private slots:
    void onThreadStarted() const;
    void onReplyFinished(QNetworkReply *reply);

private:
    // Singleton : private constructor
    explicit FMPClient();
    ~FMPClient();

    enum class RequestSynchronicity { Async, Sync };
    enum class RequestType {
        None,
        Quote,
        SharesFloat
    };

    struct RequestInfo {
        RequestSynchronicity synchronicity;
        RequestType type = RequestType::None;
        bool completed = false;
        QJsonArray *jsonArray = nullptr;
    };

    QString buildUrlWithEndpoint(const QString &endpoint) const;
    QString buildUrlWithEndpointAndSymbol(const QString &endpoint, const QString &symbol) const;

    bool fetchSync(const QString &url, QJsonArray *&jsonArrayFromReplyToDelete);
    void fetchAsync(const QString &url, RequestType type);

    void emitSignalDemuxer(RequestType type, const QJsonArray &doc);

    QThread *thread;
    QNetworkAccessManager *manager;
    mutable QMutex pendingRequestsMutex;
    QWaitCondition waitCondition;
    QHash<QNetworkReply*, RequestInfo> pendingRequests;
    const QString baseUrl = "https://financialmodelingprep.com/stable/";

    // Singleton
    static FMPClient* instance;
    static QString apiKey;


#ifdef UNIT_TESTING
    friend class TestFMPClient;
    bool simulate_reply_network_latency = false;
    int allocated_json_arrays = 0;
    bool isCleanedUp();
    QSemaphore onReplyFinished_sem;
    unsigned long fetchSyncTimeoutMs = 5000;
    #define TRACK_NEW_JSON_ARRAY(x) x; allocated_json_arrays++;
    #define TRACK_DELETED_JSON_ARRAY(x) x; allocated_json_arrays--;
#else
    const unsigned long fetchSyncTimeoutMs = 5000; // Const under normal operation
    #define TRACK_NEW_JSON_ARRAY(x) x;
    #define TRACK_DELETED_JSON_ARRAY(x) x;
#endif


};

#endif // FMPCLIENT_H
