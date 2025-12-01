#pragma once

#include <QObject>
#include <QLoggingCategory>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QThread>

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

#include "Stream.h"

#ifdef GUI_ENABLED
#include "AuthWindow.h"
#endif

Q_DECLARE_LOGGING_CATEGORY(TSClientLog)

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

class TSClient final : public QObject
{
    Q_OBJECT
public:

    // Singleton : Instance getter  and delete copy and assignment
    static TSClient& getInstance();
    static TSClient* getInstancePtr();
    TSClient(const TSClient&) = delete;
    TSClient& operator=(const TSClient&) = delete;

    void start() { m_thread->start(); };

    /*
     * @brief Type alias for request ID.
     * This type is used to uniquely identify each request made by the RESTClient.
     */
    typedef quint64 AsyncRequestID_t;

    enum class AsyncRequestStatus_e {
        UNSET,
        SUCCESS,
        TIMEOUT,
        ERROR
    };


    // To monitor usage
    [[nodiscard]] qsizetype getTotalDataReceivedBytes() const { return m_totalDataReceivedBytes; };
    [[nodiscard]] bool isCleanedUp();

      // Stream count getter
    [[nodiscard]] size_t getStreamCount() const { return m_networkReplyToOpenStreams.size(); }

    // Authentication state getter
    [[nodiscard]] bool isAuthenticated() const { return m_authenticated; }
    [[nodiscard]] bool isAuthInProgress() const { return m_authInProgress; }  // Track if authentication process is in progress


  
                              // -------- Market data methods ----------
    /*
     * Get Quote Snapshots
     *
     * @doc : https://api.tradestation.com/docs/specification/#tag/MarketData/operation/GetQuoteSnapshots
     * 
     * @note : symbols parameter is a comma separated list of symbols
     * 
     * @return :
     *  > 0 : The request ID of the async request
     *  0 : Failed to start the async request
     */
    [[nodiscard]] AsyncRequestID_t getQuoteSnapshotsAsync(const QStringList &symbols);

    /*
     * Get Bars asynchronously
     *
     * @return :
     *  > 0 : The request ID of the async request
     *  0 : Failed to start the async request
     */
    [[nodiscard]] AsyncRequestID_t getBarsAsync(const QString &symbol,
                                                unsigned int interval = 1,
                                                Bar::BarUnit unit = Bar::BarUnit::Daily,
                                                unsigned int barsback = 1,
                                                Bar::BarSessionTemplate sessionTemplate = Bar::BarSessionTemplate::Default,
                                                QDateTime firstDate = QDateTime(),
                                                QDateTime lastDate = QDateTime());
    /*
     * Creates a Bars Stream
     *
     * @return : nullptr if the stream could not be created
     * @doc : https://api.tradestation.com/docs/specification/#tag/MarketData/operation/StreamBars
     *
     * @note : Returned pointer dynamically allocated. Delete with closeStreamBars
     */
    [[nodiscard]] StreamBars* openStreamBars(const QString &symbol,
                               unsigned int interval = 1,
                               Bar::BarUnit unit = Bar::BarUnit::Daily,
                               unsigned int barsback = 1,
                               Bar::BarSessionTemplate sesstionTemplate = Bar::BarSessionTemplate::Default);
    void closeStreamBars(StreamBars* stream);




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
    [[nodiscard]] StreamMarketDepthQuote* openStreamMarketDepthQuote(const QString &symbol, unsigned int depth = 20);
    void closeStreamMarketDepthQuote(StreamMarketDepthQuote* stream);

                              // -------- Brokerage methods -------------

    /*
     * Get Accounts
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/GetAccounts
     * 
     * @return :
     *  > 0 : The request ID of the async request
     *  0 : Failed to start the async request
     */
    [[nodiscard]] AsyncRequestID_t getAccountsAsync();
    /*
     * Get Balances
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/GetBalances
     * 
     * @return :
     *  > 0 : The request ID of the async request
     *  0 : Failed to start the async request
     */
    [[nodiscard]] AsyncRequestID_t getBalancesAsync(const QStringList &accounts);

    /*
     * Creates a StreaOrders Stream
     *
     * @return : nullptr if the stream could not be created
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/StreamOrders
     */
    [[nodiscard]] StreamOrders* openStreamOrders(QString &account);
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
    [[nodiscard]] StreamPositions* openStreamPositions(QString &account, bool changes = false);
    void closeStreamPositions(StreamPositions* stream);

                              // -------- Order execution methods --------
 
    /*
     * Place order
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Order-Execution/operation/PlaceOrder
     * 
     * @return :
     *   > 0 : The request ID of the async request
     *   0 : Failed to start the async request
     */
    [[nodiscard]] AsyncRequestID_t placeOrderAsync(const PlaceOrderRequest &order);

    /*
     * Cancel order
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Order-Execution/operation/CancelOrder
     * 
     * @return :
     *   > 0 : The request ID of the async request
     *   0 : Failed to start the async request
     */
    [[nodiscard]] AsyncRequestID_t cancelOrderAsync(const QString &orderID);

public slots:
    #ifdef GUI_ENABLED
    // Authentication methods
    void launchAuthProcess();
    #endif

signals:
    // Emited at basically every new message
    void totalDataReceivedBytesIncreased(qsizetype dataSize);
    
    void openStreamCountChanged(size_t count);
    void pendingAsyncRequestsCountChanges(size_t count);

    void authStateChanged(bool isAuthenticated, QString reason);

    void receivedAsyncGetBars          (AsyncRequestID_t requestID, AsyncRequestStatus_e status, QVector<Bar> bars);
    void receivedAsyncGetAccounts      (AsyncRequestID_t requestID, AsyncRequestStatus_e status, QVector<Account> results);
    void receivedAsyncGetBalances      (AsyncRequestID_t requestID, AsyncRequestStatus_e status, QVector<Balance> results);
    void receivedAsyncGetQuoteSnapshots(AsyncRequestID_t requestID, AsyncRequestStatus_e status, QVector<QuoteSnapshot> quoteSnapshots);
    void receivedAsyncPlaceOrder       (AsyncRequestID_t requestID, AsyncRequestStatus_e status, PlaceOrderResult result);
    void receivedAsyncCancelOrder      (AsyncRequestID_t requestID, AsyncRequestStatus_e status, CancelOrderResult result);
    
private slots:

    void onReplyAsyncRequestReadyRead();
    void onReplyAsyncRequestFinished();
    void onReplyAsyncRequestErrorOccurred(QNetworkReply::NetworkError code);


    #ifdef GUI_ENABLED
    void onAuthFinished(bool success, AuthToken token, QString reason);
    void onAuthWindowDestroyed();
    #endif

private:
    static TSClient* m_instance;
     explicit TSClient(); // Singleton : private constructor
    ~TSClient();

    enum class HttpMethod {
        GET,
        POST,
        PUT,
        DELETE
    };

    enum class AsyncRequestType_t {
        None,
        GetRefreshAccessToken,
        GetBars,
        GetAccounts,
        GetBalances,
        GetQuoteSnapshots,
        PlaceOrder,
        CancelOrder,
        MAX_REQUEST_TYPE
    };

    enum class StreamType_t {
        None,
        Bars,
        MarketDepthQuote,
        Orders,
        Positions,
        MAX_STREAM_TYPE
    };

    struct RequestInfo {
        AsyncRequestID_t requestID = 0;
        AsyncRequestType_t type = AsyncRequestType_t::None;
    };

   

    void processNewAmountOfDataReceived(size_t bytesReceived);
    [[nodiscard]] QNetworkRequest buildNetworkRequest(const QString &endpoint, const QUrlQuery &query = QUrlQuery()) const;


    // Auth and refresh stuff implemented in TSClientRefreshToken.cpp
    [[nodiscard]] static QNetworkRequest buildRefreshTokenRequest();
    [[nodiscard]] static QByteArray buildRefreshTokenQuery(const QString &clientId,
                                                           const QString &clientSecret,
                                                           const QString &refreshToken);                                       
    void refreshAsyncAccessToken();
    void processAsyncRefreshTokenFinished(AsyncRequestID_t requestID, AsyncRequestStatus_e status, AuthToken newToken);




    [[nodiscard]] AsyncRequestID_t sendAsyncRequest(const QNetworkRequest &request, AsyncRequestType_t type, HttpMethod method = HttpMethod::GET, const QByteArray &postData = QByteArray());
    void demuxReceivedAsyncRequestReply(AsyncRequestType_t asyncRequestType, const QJsonDocument &doc, AsyncRequestID_t requestID, AsyncRequestStatus_e status);

    void openStream(const QNetworkRequest &request, Stream *stream);

    void closeStream(Stream * const stream);

    void demuxReceivedStreamReply(StreamType_t streamType, const QJsonDocument &doc, AsyncRequestID_t requestID, AsyncRequestStatus_e status);
 
    //********* members ********/



    // tokens
    AuthToken   m_authToken;
    ClientToken m_clientToken;

    bool m_authenticated     = false;  // Track authentication state
    bool m_refreshInProgress = false;  // Track if authentication process is in progress
    bool m_authInProgress    = false;  // Track if authentication process is in progress

    AsyncRequestID_t m_asyncTokenRefreshRequestId = 0; // Store the request ID of the ongoing token refresh request

    QUrl m_baseUrl;
    QAtomicInteger<AsyncRequestID_t> m_asyncRequestIDCurrentSequence{0};   // starts at 0
    //mutable QReadWriteLock m_requestIDMapRWLock; // To protect m_requestIDSeq
    qsizetype m_totalDataReceivedBytes = 0;
    QString m_apiKey;
    QThread *m_thread;
    QNetworkAccessManager *m_networkManager;
    QMap<QNetworkReply*, RequestInfo> m_networkReplyToPendingAsyncRequests;
    QMap<QNetworkReply*, Stream*> m_networkReplyToOpenStreams;


#ifdef GUI_ENABLED
    AuthWindow* m_authWindow = nullptr;  // Authentication window
#endif

    friend class TestTSClient;
    friend class TestBarCache;
};
