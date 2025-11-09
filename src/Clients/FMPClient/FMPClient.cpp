#include <QNetworkAccessManager>
#include <QThread>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QDebug>
#include <QNetworkRequest>
#include <QSettings>
#include <QEventLoop>
#include <QTimer>

#include "FMPClient.h"
#include "SecureStorage.h"

#define BASE_URL_FMP_API "https://financialmodelingprep.com/stable/"

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
    RESTClient(QUrl(BASE_URL_FMP_API))
{
    // Load FMP API key from secure storage
    loadApiKey();

    thread->setObjectName("FPMClientThread");

    thread->start();
}

void FMPClient::loadApiKey()
{
    SecureStorage* storage = new SecureStorage(this);

    // Use event loop to make the async operation synchronous during initialization
    QEventLoop loop;
    QString apiKey;

    storage->retrieveValue("FMP_API", "access_token", [&](const QString& value) {
        apiKey = value;
        loop.quit();
    });

    // Wait for the async operation to complete (with timeout)
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(5000); // 5 second timeout

    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    qCDebug(FMPClientLog) << Q_FUNC_INFO << ": Waiting for API key retrieval...";
    
    loop.exec();

    if (apiKey.isEmpty()) {
        qCCritical(FMPClientLog) << "No FMP access token found. Please configure your API key.";
    }

    setAPIKey(apiKey);
    qCDebug(FMPClientLog) << Q_FUNC_INFO << ": FMPClient created using secure key storage";

    storage->deleteLater();
}

FMPClient::~FMPClient() {
    qCDebug(FMPClientLog) << "Singleton instance destroyed";

    thread->quit();
    thread->wait();
}

void FMPClient::fetchAsyncQuoteShort(const QString &symbol) {
    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, "quote-short", symbol);
    fetchAsync(request, static_cast<RequestTypeInt>(RequestType::Quote));
}

void FMPClient::fetchAsyncSharesFloat(const QString &symbol)
{
    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, "shares-float", symbol);
    fetchAsync(request, static_cast<RequestTypeInt>(RequestType::SharesFloat));
}

void FMPClient::fetchAsyncStockNews(const StockNewsFilter &filter)
{
    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, "news/stock", filter);
    fetchAsync(request, static_cast<RequestTypeInt>(RequestType::StockNews));
}

bool FMPClient::fetchSyncQuoteShort(const QString &symbol, struct QuoteShortResult &result) {
    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, "quote-short", symbol);
    QJsonDocument *jsonDocumentFromReplyToDelete = nullptr;

    bool success = fetchSync(request, jsonDocumentFromReplyToDelete);

    if (success) {
        // The positive return value implies jsonDocumentFromReplyToDelete has been allocated to something
        Q_ASSERT(jsonDocumentFromReplyToDelete != nullptr);

        QJsonObject obj = jsonDocumentFromReplyToDelete->array().first().toObject();

        Q_ASSERT(symbol == obj["symbol"].toString());
        result.symbol = obj["symbol"].toString();
        result.price = obj["price"].toDouble();
        result.change = obj["change"].toDouble();
        result.volume = obj["volume"].toInteger();

        // This pointer to a JSON array was allocated in the fetchSync and needs to be deleted after use
        TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);
    } else {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonDocumentFromReplyToDelete == nullptr);
    }

    return success;
}

bool FMPClient::fetchSyncSharesFloat(const QString &symbol, struct SharesFloatResult &result)
{
    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, "shares-float", symbol);
    QJsonDocument *jsonDocumentFromReplyToDelete = nullptr;

    bool success = fetchSync(request, jsonDocumentFromReplyToDelete);

    if (success) {
        // The positive return value implies jsonDocumentFromReplyToDelete has been allocated to something
        Q_ASSERT(jsonDocumentFromReplyToDelete != nullptr);

        QJsonObject obj = jsonDocumentFromReplyToDelete->array().first().toObject();

        Q_ASSERT(symbol == obj["symbol"].toString());

        result.symbol = obj["symbol"].toString();
        result.date = obj["date"].toString();
        result.freeFloat = obj["freeFloat"].toDouble();
        result.floatShares = obj["floatShares"].toInteger();
        result.outstandingShares = obj["outstandingShares"].toInteger();

        // This pointer to a JSON array was allocated in the fetchSync and needs to be deleted after use
        TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);
    } else {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonDocumentFromReplyToDelete == nullptr);
    }

    return success;
}

bool FMPClient::fetchSyncCompanyScreener(const CompanyScreenerFilter &filter, QVector<CompanyScreenerResult> &results)
{
    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, "company-screener", filter);
    QJsonDocument *jsonDocumentFromReplyToDelete = nullptr;

    bool success = fetchSync(request, jsonDocumentFromReplyToDelete);

    if (success) {
        // The positive return value implies jsonDocumentFromReplyToDelete has been allocated to something
        Q_ASSERT(jsonDocumentFromReplyToDelete != nullptr);

        // Resize the array in advance
        results.reserve(jsonDocumentFromReplyToDelete->array().count());

        for (QJsonValue json: jsonDocumentFromReplyToDelete->array()) {
            results.push_back(CompanyScreenerResult(json.toObject()));
        }

        // This pointer to a JSON array was allocated in the fetchSync and needs to be deleted after use
        TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);
    } else {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonDocumentFromReplyToDelete == nullptr);
    }

    return success;
}

bool FMPClient::fetchSyncStockNews(const StockNewsFilter &filter, QVector<StockNewsResult> &results)
{
    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, "news/stock", filter);
    QJsonDocument *jsonDocumentFromReplyToDelete = nullptr;

    bool success = fetchSync(request, jsonDocumentFromReplyToDelete);

    if (success) {
        // The positive return value implies jsonDocumentFromReplyToDelete has been allocated to something
        Q_ASSERT(jsonDocumentFromReplyToDelete != nullptr);

        // Resize the array in advance
        results.reserve(jsonDocumentFromReplyToDelete->array().count());

        const QJsonArray& jsonArray = jsonDocumentFromReplyToDelete->array();
        for (const QJsonValue &json: jsonArray) {
            results.push_back(StockNewsResult(json.toObject()));
        }

        // This pointer to a JSON array was allocated in the fetchSync and needs to be deleted after use
        TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);
    } else {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonDocumentFromReplyToDelete == nullptr);
    }

    return success;
}



void FMPClient::emitSignalDemuxer(RequestTypeInt type, const QJsonDocument &doc, bool completed, void *optArg) {
    Q_UNUSED(optArg)

    if (!completed) {
        qCCritical(FMPClientLog) << Q_FUNC_INFO << "Async operation not completed";
    }

    //TODO critical : handle the completed argument

    QJsonObject obj = doc.array().first().toObject();

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

        case RequestType::StockNews:
        {
            QVector<StockNewsResult> results;

            const QJsonArray& jsonArray = doc.array();
            for (const QJsonValue &json: jsonArray) {
                results.push_back(StockNewsResult(json.toObject()));
            }

            emit stockNewsReceived(results);
            break;
        }

        default:
            Q_UNREACHABLE();
            break;
    }
}
