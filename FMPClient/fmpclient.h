#ifndef FMPCLIENT_H
#define FMPCLIENT_H

#include <QObject>
#include <QThread>
#include <QNetworkAccessManager>
#include <QMutex>
#include <QWaitCondition>
#include <QHash>

// This is a singleton

class FMPClient : public QObject {
    Q_OBJECT
public:
    // Declare TestFMPClient as a friend class so it inspect its variables during the unit tests
    friend class TestFMPClient;

    // Singleton : instance getter
    static FMPClient& getInstance();

    static void setAPIKey(const QString &apiKey);

    // Singleton : Delete copy constructor and assignment operator
    FMPClient(const FMPClient&) = delete;
    FMPClient& operator=(const FMPClient&) = delete;

    // API data fetchers
    void fetchQuoteAsync(const QString &symbol);
    bool fetchQuoteSync(const QString &symbol, double &price, double &bid, double &ask);
    bool fetchSharesFloatSync(const QString &symbol, QString &date, double &freeFloat, double &floatShares, double &outstandingShares);

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

};

#endif // FMPCLIENT_H
