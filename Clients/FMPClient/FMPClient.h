#ifndef FMPCLIENT_H
#define FMPCLIENT_H

#include <QObject>

#include <QLoggingCategory>
#include <QVector>

#include "../RESTClient.h"
#include "Filters/CompanyScreenerFilter.h"
#include "Filters/StockNewsFilter.h"

Q_DECLARE_LOGGING_CATEGORY(FMPClientLog)

// This is a singleton

class FMPClient : public RESTClient {
    Q_OBJECT
public:

    // Singleton : Instance getter
    static FMPClient& getInstance();
    static FMPClient* getInstancePtr();
    // Singleton : Delete copy constructor and assignment operator
    FMPClient(const FMPClient&) = delete;
    FMPClient& operator=(const FMPClient&) = delete;



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
    void fetchAsyncStockNews(const StockNewsFilter &filter);


signals:
    // API Async version signals
    void quoteShortReceived(struct FMPClient::QuoteShortResult result);
    void sharesFloatReceived(struct FMPClient::SharesFloatResult result);
    void stockNewsReceived(QVector<StockNewsResult> results);

private slots:

private:

    // Singleton : private constructor
    explicit FMPClient();
    ~FMPClient();

    enum class RequestType {
        None,
        Quote,
        SharesFloat,
        StockNews
    };

    void emitSignalDemuxer(RequestTypeInt type, const QJsonDocument &doc, void* optArg = nullptr);

    // Singleton
    static FMPClient* instance;

    // API key placement configuration
    static constexpr ApiKeyPlacement API_KEY_PLACEMENT = ApiKeyPlacement::InUrl;

    friend class TestFMPClient;
};

#endif // FMPCLIENT_H
