#ifndef FMPCLIENT_H
#define FMPCLIENT_H

#include <QObject>
#include <QMutex>
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
    void fetchQuoteAsync(const QString &symbol);
    bool fetchQuoteSync(const QString &symbol, double &price, double &bid, double &ask);
    bool fetchSharesFloatSync(const QString &symbol, QString &date, double &freeFloat, double &floatShares, double &outstandingShares);

    // Singleton : Delete copy constructor and assignment operator
    FMPClient(const FMPClient&) = delete;
    FMPClient& operator=(const FMPClient&) = delete;

    // Declare TestFMPClient as a friend class so it inspect its variables during the unit tests
    friend class TestFMPClient;

signals:
    // API Async version signals
    void quoteReceived(double price, double bid, double ask);

private slots:
    void onThreadStarted() const;
    void onReplyFinished(QNetworkReply *reply);

private:
    // Singleton : private constructor
    explicit FMPClient();
    ~FMPClient();

    enum class RequestType { QuoteAsync, QuoteSync };

    struct RequestInfo {
        RequestType type;
        bool completed = false;
        QJsonArray *jsonArray = nullptr;
    };

    QString buildUrlWithEndpoint(const QString &endpoint) const;
    QString buildUrlWithEndpointAndSymbol(const QString &endpoint, const QString &symbol) const;
    bool fetchSync(const QString &url, QJsonArray *&jsonArrayFromReplyToDelete);

    QThread *thread;
    QNetworkAccessManager *manager;
    mutable QMutex mutex;
    QWaitCondition waitCondition;
    QHash<QNetworkReply*, RequestInfo> pendingRequests;
    const QString baseUrl = "https://financialmodelingprep.com/stable/";

    // Singleton
    static FMPClient* instance;
    static QString apiKey;


    // *** Test knobs only used by friend test class TestFMPClient
#ifdef UNIT_TESTING
    bool introduce_6s_network_latency = false;
#endif
};

#endif // FMPCLIENT_H
