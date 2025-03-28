#ifndef FMPCLIENT_H
#define FMPCLIENT_H

#include <QObject>
#include <QMutex>
#include <QSemaphore>
#include <QWaitCondition>
#include <QHash>
#include <QLoggingCategory>
#include <atomic>
#include <QVector>

#include "companyscreenerfilter.h"
#include "stocknewsfilter.h"

Q_DECLARE_LOGGING_CATEGORY(FMPClientLog)

// This is a singleton

class QThread;
class QNetworkAccessManager;
class QNetworkReply;


class FMPClient : public QObject {
    Q_OBJECT
public:

    // Singleton : Instance getter
    static FMPClient& getInstance();
    static FMPClient* getInstancePtr();
    // Singleton : Delete copy constructor and assignment operator
    FMPClient(const FMPClient&) = delete;
    FMPClient& operator=(const FMPClient&) = delete;

    // Must be called before the first getInstance() call otherwise an assert is triggered in the constructor
    static void setAPIKey(const QString &apiKey);

    // To monitor usage
    qsizetype getTotalDataReceivedBytes() const;

    // API data fetchers

    // https://site.financialmodelingprep.com/developer/docs/stable/quote-short
    struct QuoteShortResult{
        QString symbol;
        double price;
        double change;
        qsizetype volume;
    };
    void fetchAsyncQuoteShort(const QString &symbol);
    bool fetchSyncQuoteShort(const QString &symbol, struct QuoteShortResult &result);

    // https://site.financialmodelingprep.com/developer/docs/stable/shares-float
    struct SharesFloatResult{
        QString symbol;
        QString date;
        double freeFloat;
        qsizetype floatShares;
        qsizetype outstandingShares;
    };
    void fetchAsyncSharesFloat(const QString &symbol);
    bool fetchSyncSharesFloat(const QString &symbol, struct SharesFloatResult &result);

    // https://site.financialmodelingprep.com/developer/docs/stable/search-company-screener
    bool fetchSyncCompanyScreener(const CompanyScreenerFilter &filter, QVector<CompanyScreenerResult> &results);

    // https://site.financialmodelingprep.com/developer/docs/stable/stock-news
    bool fetchSyncStockNews(const StockNewsFilter &filter, QVector<StockNewsResult> &results);

signals:
    // API Async version signals
    void quoteShortReceived(struct QuoteShortResult result);
    void sharesFloatReceived(struct SharesFloatResult result);

    // Emited at basically every new message
    void totalDataReceivedBytesIncreased(qsizetype dataSize);

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
    QString buildUrlWithEndpointAndParamsList(const QString &endpoint, const QString &paramsList) const;


    bool fetchSync(const QString &url, QJsonArray *&jsonArrayFromReplyToDelete);
    void fetchAsync(const QString &url, RequestType type);

    void emitSignalDemuxer(RequestType type, const QJsonArray &doc);

    QThread *thread;
    QNetworkAccessManager *manager;
    mutable QMutex pendingRequestsMutex;
    QWaitCondition waitCondition;
    QHash<QNetworkReply*, RequestInfo> pendingRequests;
    const QString baseUrl = "https://financialmodelingprep.com/stable/";
    std::atomic<qsizetype> totalDataReceivedBytes = 0;

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
