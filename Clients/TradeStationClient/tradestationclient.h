#ifndef TRADESTATIONCLIENT_H
#define TRADESTATIONCLIENT_H

#include <QObject>
#include <QMutex>
#include <QSemaphore>
#include <QWaitCondition>
#include <QHash>
#include <QLoggingCategory>
#include <atomic>
#include <QVector>

Q_DECLARE_LOGGING_CATEGORY(TradeStationClientLog)

// This is a singleton

class QThread;
class QNetworkAccessManager;
class QNetworkReply;

class TradeStationClient : public QObject {
    Q_OBJECT
public:
    // Singleton : Instance getter
    static TradeStationClient& getInstance();
    static TradeStationClient* getInstancePtr();
    // Singleton : Delete copy constructor and assignment operator
    TradeStationClient(const TradeStationClient&) = delete;
    TradeStationClient& operator=(const TradeStationClient&) = delete;

    // Must be called before the first getInstance() call otherwise an assert is triggered in the constructor
    static void setAPIKey(const QString &apiKey);

    // To monitor usage
    qsizetype getTotalDataReceivedBytes() const;

signals:
    // Emited at basically every new message
    void totalDataReceivedBytesIncreased(qsizetype dataSize);

private slots:
    void onThreadStarted() const;
    void onReplyFinished(QNetworkReply *reply);

private:
    // Singleton : private constructor
    explicit TradeStationClient();
    ~TradeStationClient();

    enum class RequestSynchronicity { Async, Sync };
    enum class RequestType {
        None
    };

    struct RequestInfo {
        RequestSynchronicity synchronicity;
        RequestType type = RequestType::None;
        bool completed = false;
        QJsonArray *jsonArray = nullptr;
    };

    QString buildUrlWithEndpoint(const QString &endpoint) const;
    QString buildUrlWithEndpointAndSymbol(const QString &endpoint, const QString &symbol) const;
    QString buildUrlWithEndpointAndParamsList(const QString &endpoint, const QString &paramsList) const;

    bool fetchSync(const QString &url, QJsonArray *&jsonArrayFromReplyToDelete);
    void fetchAsync(const QString &url, RequestType type);

    void emitSignalDemuxer(RequestType type, const QJsonArray &doc);

    QThread *thread;
    QNetworkAccessManager *manager;
    mutable QMutex pendingRequestsMutex;
    QWaitCondition waitCondition;
    QHash<QNetworkReply*, RequestInfo> pendingRequests;
    const QString baseUrl = "https://api.tradestation.com/v3/";  // TODO verify this is the correct base URL
    std::atomic<qsizetype> totalDataReceivedBytes = 0;

    // Singleton
    static TradeStationClient* instance;
    static QString apiKey;

#ifdef UNIT_TESTING
    friend class TestTradeStationClient;
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

#endif // TRADESTATIONCLIENT_H 