#pragma once

#include <QObject>
#include <QLoggingCategory>

#include "RESTClient.h"
#include "AuthToken.h"
#include "ClientToken.h"
#include "Account.h"
#include "StreamOrders.h"
#include "StreamPositions.h"
#include "PlaceOrder.h"
#include "CancelOrder.h"
#include "QuoteSnapshot.h"
#include "StreamBars.h"
#include "StreamMarketDepthQuote.h"
#include "Balance.h"

#ifdef GUI_ENABLED
#include "AuthWindow.h"
#endif

Q_DECLARE_LOGGING_CATEGORY(TSClientLog)

#define BASE_URL_TS_API_SIMULATION         "https://sim-api.tradestation.com/v3/"

#define ENDPOINT_GET_QUOTE_SNAPSHOTS       "marketdata/quotes/%1"
#define ENDPOINT_GET_BARS                  "marketdata/barcharts/%1"
#define ENDPOINT_STREAM_BARS               "marketdata/stream/barcharts"
#define ENDPOINT_STREAM_MARKET_DEPTH_QUOTE "marketdata/stream/marketdepth/quotes"

#define ENDPOINT_GET_ACCOUNTS              "brokerage/accounts"
#define ENDPOINT_GET_BALANCES              "brokerage/accounts/%1/balances"
#define ENDPOINT_STREAM_ORDERS             "brokerage/stream/accounts/%1/orders"
#define ENDPOINT_STREAM_POSITIONS          "brokerage/stream/accounts/%1/positions"

#define ENDPOINT_PLACE_ORDER               "orderexecution/orders"
#define ENDPOINT_CANCEL_ORDER              "orderexecution/orders/%1"


// This is a singleton

class TSClient : public RESTClient {
    Q_OBJECT
public:
    // Singleton : Instance getter  and delete copy and assignment
    static TSClient& getInstance();
    static TSClient* getInstancePtr();
    TSClient(const TSClient&) = delete;
    TSClient& operator=(const TSClient&) = delete;

    // Authentication state getter
    bool isAuthenticated() const { return authenticated; }
    bool isAuthInProgress() const { return authInProgress; }

    void activateMockStreamCreation(bool activate) { activateMockStream = activate; };

    // Stream count getter
    int getStreamCount() const { return streams.size(); }

                              // -------- Market data methods ----------
    /*
     * Get Quote Snapshots
     *
     * @doc : https://api.tradestation.com/docs/specification/#tag/MarketData/operation/GetQuoteSnapshots
     */
    bool getQuoteSnapshotsSync(QString &symbols, QVector<QuoteSnapshot> &quoteSnapshots);
    void getQuoteSnapshotsAsync(QString &symbols);

    /*
     * Creates a Bars Stream
     *
     * @return : nullptr if the stream could not be created
     * @doc : https://api.tradestation.com/docs/specification/#tag/MarketData/operation/StreamBars
     *
     * @note : Returned pointer dynamically allocated. Delete with closeStreamBars
     */
    StreamBars* openStreamBars(const QString &symbol,
                               unsigned int interval = 1,
                               Bar::BarUnit unit = Bar::BarUnit::Daily,
                               unsigned int barsback = 1,
                               Bar::BarSessionTemplate sesstionTemplate = Bar::BarSessionTemplate::Default,
                               bool mock = false);
    void closeStreamBars(StreamBars* stream);

    bool getBarsSync(QVector<Bar> &results,
                     const QString &symbol,
                     unsigned int interval = 1,
                     Bar::BarUnit unit = Bar::BarUnit::Daily,
                     unsigned int barsback = 1,
                     Bar::BarSessionTemplate sessionTemplate = Bar::BarSessionTemplate::Default,
                     QDateTime firstDate = QDateTime(),
                     QDateTime lastDate = QDateTime());

    void getBarsAsync(const QString &symbol,
                      unsigned int interval = 1,
                      Bar::BarUnit unit = Bar::BarUnit::Daily,
                      unsigned int barsback = 1,
                      Bar::BarSessionTemplate sessionTemplate = Bar::BarSessionTemplate::Default,
                      QDateTime firstDate = QDateTime(),
                      QDateTime lastDate = QDateTime());

    /*
     * Creates a MarketDepthQuote Stream
     *
     * @return : nullptr if the stream could not be created
     * @doc : https://api.tradestation.com/docs/specification/#tag/MarketData/operation/StreamMarketDepthQuotes
     *
     * @note : Object dynamically allocated and returned. TSClient owns this object and it lives
     *         in the thread of the client and shares the same network access manager. Later
     *         calling closeStreamMarketDepthQuote will delete it. Do not delete outside.
     */
    StreamMarketDepthQuote* openStreamMarketDepthQuote(const QString &symbol, unsigned int depth = 20);
    void closeStreamMarketDepthQuote(StreamMarketDepthQuote* stream);

                              // -------- Brokerage methods -------------

    /*
     * Get Accounts
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/GetAccounts
     */
    bool getAccountsSync(QVector<Account> &results);
    void getAccountsAsync();

    /*
     * Get Balances
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/GetBalances
     */
    bool getBalancesSync(const QString accounts, QVector<Balance> &results);
    void getBalancesAsync(const QString accounts);

    /*
     * Creates a StreaOrders Stream
     *
     * @return : nullptr if the stream could not be created
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/StreamOrders
     */
    StreamOrders* openStreamOrders(QString &account);
    void closeStreamOrders(StreamOrders* stream);

    /*
     * Creates a StreamPositions Stream
     *
     * @return : nullptr if the stream could not be created
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/StreamPositions
     *
     * @note : Object dynamically allocated and returned. TSClient owns this object and it lives
     *         in the thread of the client and shares the same network access manager. Later
     *         calling closeStreamMarketDepthQuote will delete it. Do not delete outside.
     */
    StreamPositions* openStreamPositions(QString &account, bool changes = false);
    void closeStreamPositions(StreamPositions* stream);

                              // -------- Order execution methods --------
 
    /*
     * Place order
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Order-Execution/operation/PlaceOrder
     */
    bool placeOrderSync(const PlaceOrderRequest &order, PlaceOrderResult &result);
    void placeOrderAsync(const PlaceOrderRequest &order);

    /*
     * Cancel order
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Order-Execution/operation/CancelOrder
     */
    bool cancelOrderSync(const QString &orderID, CancelOrderResult &result);
    void cancelOrderAsync(const QString &orderID);

public slots:
    #ifdef GUI_ENABLED
    // Authentication methods
    void launchAuthProcess();
    #endif

signals:
    void authStateChanged(bool isAuthenticated, QString reason);
    void getAccountsAsyncReceived(QVector<Account> results);
    void getBalancesAsyncReceived(QVector<Balance> results);
    void getQuoteSnapshotsAsyncReceived(QVector<QuoteSnapshot> quoteSnapshots);
    void placeOrderAsyncReceived(PlaceOrderResult result);
    void cancelOrderAsyncReceived(CancelOrderResult result);
    void getBarsAsyncReceived(QString symbol, QVector<Bar> bars);
    void streamCountChanged(int count);

private slots:
    #ifdef GUI_ENABLED
    void onAuthFinished(bool success, AuthToken token, QString reason);
    void onAuthWindowDestroyed();
    #endif
    void onAsyncRefreshTokenFinished(bool completed, const AuthToken &newToken);

private:
    // Singleton : private constructor
    explicit TSClient();
    ~TSClient();


    // Static helper methods for authentication
    static QNetworkRequest buildRefreshTokenRequest();
    static QByteArray buildRefreshTokenQuery(const QString &clientId,
                                             const QString &clientSecret,
                                             const QString &refreshToken);
                                           
    bool refreshSyncAccessToken(); // TODO remove or think about something because this causes a deadlocl when called within TSClient itself
    void refreshAsyncAccessToken();

    enum class RequestType {
        None,
        GetAccounts,
        GetBalances,
        GetBars,
        GetQuoteSnapshots,
        GetRefreshAccessToken,
        PlaceOrder,
        CancelOrder
    };

    void emitSignalDemuxer(RequestTypeInt type, const QJsonDocument &doc, bool completed, void* optArg = nullptr) override;

    // tokens
    AuthToken authToken;
    ClientToken clientToken;

    // Singleton
    static TSClient* instance;

    bool authenticated = false;  // Track authentication state
    bool authInProgress = false;  // Track if authentication process is in progress

#ifdef GUI_ENABLED
    AuthWindow* authWindow = nullptr;  // Authentication window
#endif
    QVector<Stream*> streams;

    // API key placement configuration
    static constexpr ApiKeyPlacement API_KEY_PLACEMENT = ApiKeyPlacement::InHeader;

    bool activateMockStream = false;

    template<typename T, typename... Args>
    T* openStream(const QString &symbol, const QString &endpoint, const QUrlQuery &query, Args&&... args) {
        Q_ASSERT(!symbol.isEmpty());
        if (symbol != "NOSYMBOL") Q_ASSERT_X(symbol.length() >= 1 && symbol.length() <= 8, symbol.toStdString().c_str(), "Symbol length must be between 1 and 8 characters");
        Q_ASSERT(!symbol.contains(','));
        Q_ASSERT(symbol.isUpper());
        Q_ASSERT(!endpoint.isEmpty());

        // Only external callers to TSClient thread should get here. Calling a fetch sync from within the TSClient's
        // thread would cause a deadlock to itself
        Q_ASSERT_X(QThread::currentThread() != thread, Q_FUNC_INFO, "TSClient object cannot call this function itself");

        T* stream = nullptr;

        // FIXME fix this NOSYMBOL shit
        QMetaObject::invokeMethod(this,
            [this, &symbol, &endpoint, &query, &stream, args...]() mutable
            {
                stream = new T(std::forward<Args>(args)..., nullptr);

                QUrl url(QString(BASE_URL_TS_API_SIMULATION) + endpoint + ((symbol=="NOSYMBOL") ? "" : ("/" + symbol)));
                url.setQuery(query);

                QNetworkRequest request(url);
                request.setRawHeader("Authorization", QString("Bearer %1").arg(authToken.getAccessToken()).toUtf8());

                // The stream is now created in the TSClient thread
                stream->setParent(this);
                streams.push_back(stream);

                QNetworkReply *reply = fetchStream(request, static_cast<void*>(stream));

                // The readyRead, finished and errorOccured are connected internaly here.
                // This call starts the timeout timer as well
                stream->setNetworkReply(reply);

                connect(stream, &Stream::receivedAmountOfData, this, &TSClient::onReceivedNewAmountOfData);
                
                // Emit signal that stream count has changed
                emit streamCountChanged(streams.size());
            },
        Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
                                       // finishes executing this lambda so that a valid pointer is returned

        qCDebug(TSClientLog) << Q_FUNC_INFO << "Opened Stream " << static_cast<void*>(stream);

        return stream;
    }
    void closeStream(Stream* const stream);
};
