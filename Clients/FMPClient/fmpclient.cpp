#include "fmpclient.h"
#include <QNetworkAccessManager>
#include <QThread>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QDebug>

const QString baseUrlFMP = "https://financialmodelingprep.com/stable/";

// Define the logging category
Q_LOGGING_CATEGORY(FMPClientLog, "FMPClient")

// Initialize static member outside class
FMPClient* FMPClient::instance = nullptr;

FMPClient& FMPClient::getInstance() {

    if (instance == nullptr) {
        qCDebug(FMPClientLog) << "Singleton instance created";

        instance = new FMPClient();
    }
    return *instance;
}

FMPClient* FMPClient::getInstancePtr() {

    if (instance == nullptr) {
        qCDebug(FMPClientLog) << "Singleton instance created";

        instance = new FMPClient();
    }
    return instance;
}

FMPClient::FMPClient() :
    RESTClient(baseUrlFMP)
{
    qCDebug(FMPClientLog) << Q_FUNC_INFO << ": FMPClient created using KEY=" << apiKey;

    thread->setObjectName("FPMClientThread");

    thread->start();
}

FMPClient::~FMPClient() {
    qCDebug(FMPClientLog) << "Singleton instance destroyed";

    thread->quit();
    thread->wait();
}

void FMPClient::fetchAsyncQuoteShort(const QString &symbol) {
    QString url = buildUrlWithEndpointSymbolAndApiKeyParam("quote-short",symbol);

    fetchAsync(url, static_cast<RequestTypeInt>(RequestType::Quote));
}

void FMPClient::fetchAsyncSharesFloat(const QString &symbol)
{
    QString url = buildUrlWithEndpointSymbolAndApiKeyParam("shares-float",symbol);

    fetchAsync(url, static_cast<RequestTypeInt>(RequestType::SharesFloat));
}

bool FMPClient::fetchSyncQuoteShort(const QString &symbol, struct QuoteShortResult &result) {
    QString url = buildUrlWithEndpointSymbolAndApiKeyParam("quote-short",symbol);
    QJsonArray *jsonArrayFromReplyToDelete = nullptr;

    bool success = fetchSync(url, jsonArrayFromReplyToDelete);

    if (success) {
        // The positive return value implies jsonArrayFromReplyToDelete has been allocated to something
        Q_ASSERT(jsonArrayFromReplyToDelete != nullptr);

        QJsonObject obj = jsonArrayFromReplyToDelete->first().toObject();

        Q_ASSERT(symbol == obj["symbol"].toString());
        result.symbol = obj["symbol"].toString();
        result.price = obj["price"].toDouble();
        result.change = obj["change"].toDouble();
        result.volume = obj["volume"].toInteger();

        // This pointer to a JSON array was allocated in the fetchSync and needs to be deleted after use
        TRACK_DELETED_JSON_ARRAY(delete jsonArrayFromReplyToDelete);
    } else {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonArrayFromReplyToDelete == nullptr);
    }

    return success;
}

bool FMPClient::fetchSyncSharesFloat(const QString &symbol, struct SharesFloatResult &result)
{
    QString url = buildUrlWithEndpointSymbolAndApiKeyParam("shares-float",symbol);
    QJsonArray *jsonArrayFromReplyToDelete = nullptr;

    bool ret = fetchSync(url, jsonArrayFromReplyToDelete);

    if (ret) {
        // The positive return value implies jsonArrayFromReplyToDelete has been allocated to something
        Q_ASSERT(jsonArrayFromReplyToDelete != nullptr);

        QJsonObject obj = jsonArrayFromReplyToDelete->first().toObject();

        Q_ASSERT(symbol == obj["symbol"].toString());

        result.symbol = obj["symbol"].toString();
        result.date = obj["date"].toString();
        result.freeFloat = obj["freeFloat"].toDouble();
        result.floatShares = obj["floatShares"].toInteger();
        result.outstandingShares = obj["outstandingShares"].toInteger();

        // This pointer to a JSON array was allocated in the fetchSync and needs to be deleted after use
        TRACK_DELETED_JSON_ARRAY(delete jsonArrayFromReplyToDelete);
    } else {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonArrayFromReplyToDelete == nullptr);
    }

    return ret;
}

bool FMPClient::fetchSyncCompanyScreener(const CompanyScreenerFilter &filter, QVector<CompanyScreenerResult> &results)
{
    QString url = buildUrlWithEndpointParamsAndApiKeyParam("company-screener", filter.getURLParameters());
    QJsonArray *jsonArrayFromReplyToDelete = nullptr;

    bool ret = fetchSync(url, jsonArrayFromReplyToDelete);

    if (ret) {
        // The positive return value implies jsonArrayFromReplyToDelete has been allocated to something
        Q_ASSERT(jsonArrayFromReplyToDelete != nullptr);

        // Resize the array in advance
        results.reserve(jsonArrayFromReplyToDelete->count());

        for (QJsonValue json: *jsonArrayFromReplyToDelete) {
            results.push_back(CompanyScreenerResult(json.toObject()));
        }

        // This pointer to a JSON array was allocated in the fetchSync and needs to be deleted after use
        TRACK_DELETED_JSON_ARRAY(delete jsonArrayFromReplyToDelete);
    } else {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonArrayFromReplyToDelete == nullptr);
    }

    return ret;
}

bool FMPClient::fetchSyncStockNews(const StockNewsFilter &filter, QVector<StockNewsResult> &results)
{
    QString url = buildUrlWithEndpointParamsAndApiKeyParam("news/stock", filter.getURLParameters());
    QJsonArray *jsonArrayFromReplyToDelete = nullptr;

    bool ret = fetchSync(url, jsonArrayFromReplyToDelete);

    if (ret) {
        // The positive return value implies jsonArrayFromReplyToDelete has been allocated to something
        Q_ASSERT(jsonArrayFromReplyToDelete != nullptr);

        // Resize the array in advance
        results.reserve(jsonArrayFromReplyToDelete->count());

        for (QJsonValue json: *jsonArrayFromReplyToDelete) {
            results.push_back(StockNewsResult(json.toObject()));
        }

        // This pointer to a JSON array was allocated in the fetchSync and needs to be deleted after use
        TRACK_DELETED_JSON_ARRAY(delete jsonArrayFromReplyToDelete);
    } else {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonArrayFromReplyToDelete == nullptr);
    }

    return ret;
}

void FMPClient::emitSignalDemuxer(RequestTypeInt type, const QJsonArray &doc) {

    QJsonObject obj = doc.first().toObject();

    RequestType requestType = static_cast<RequestType>(type);

    switch(requestType) {

        case RequestType::None:
            Q_ASSERT_X(0,"","Should not be None anymore");
            break;

        case RequestType::Quote:
            emit quoteShortReceived(QuoteShortResult{
                                        obj["symbol"].toString(),
                                        obj["price"].toDouble(),
                                        obj["change"].toDouble(),
                                        obj["volume"].toInteger()});
            break;
        case RequestType::SharesFloat:
            emit sharesFloatReceived(SharesFloatResult{
                                        obj["symbol"].toString(),
                                        obj["date"].toString(),
                                        obj["freeFloat"].toDouble(),
                                        obj["floatShares"].toInteger(),
                                        obj["outstandingShares"].toInteger()});

            break;

        default:
            Q_UNREACHABLE();
            break;
    }
}
