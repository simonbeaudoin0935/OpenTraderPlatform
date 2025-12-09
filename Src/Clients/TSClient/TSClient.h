#pragma once

#include <QObject>
#include <QLoggingCategory>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QThread>
#include <QFuture>

#include "AuthToken.h"
#include "ClientToken.h"
#include "Account.h"
#include "StreamOrders.h"
#include "StreamPositions.h"
#include "PlaceOrder.h"
#include "CancelOrder.h"
#include "Quote.h"
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
#define ENDPOINT_STREAM_BARS               "marketdata/stream/barcharts/%1"
#define ENDPOINT_STREAM_MARKET_DEPTH_QUOTE "marketdata/stream/marketdepth/quotes/%1"

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

    // Singleton : Instance getter
    [[nodiscard]] static TSClient* getInstance();

    TSClient(const TSClient&) = delete; // Delete copy constructor
    TSClient(TSClient&&) = delete; // Delete move constructor
    TSClient& operator=(const TSClient&) = delete; // Delete copy assignment
    TSClient& operator=(TSClient&&) = delete; // Delete move assignment


    void start() { m_thread->start(); };


    // To monitor usage
    [[nodiscard]] qsizetype getTotalDataReceivedBytes() const { return m_totalDataReceivedBytes; };
    [[nodiscard]] bool isCleanedUp();

      // Stream count getter
    [[nodiscard]] size_t getStreamCount() const { return m_networkReplyToOpenStreams.size(); }

    // Authentication state getter
    [[nodiscard]] bool isAuthenticated() const { return m_authenticated; }
    [[nodiscard]] bool isAuthInProgress() const { return m_authInProgress; }  // Track if authentication process is in progress



    enum class AsyncRequestError_e {
        TIMEOUT,
        ERROR
    };
    

    // -------- Market data methods ----------

    /*
     * Get Quote Snapshots
     *
     * @doc : https://api.tradestation.com/docs/specification/#tag/MarketData/operation/GetQuoteSnapshots
     */
    typedef std::variant<QVector<Quote>, AsyncRequestError_e> GetQuoteSnapshotResult_t;
    typedef QFuture<GetQuoteSnapshotResult_t> GetQuoteSnapshotFuture_t;

    [[nodiscard]] GetQuoteSnapshotFuture_t getQuoteSnapshots(const QStringList &symbols);

    /*
     * Get Bars asynchronously
     */
    typedef std::variant<QVector<Bar>, AsyncRequestError_e> GetBarsResult_t;
    typedef QFuture<GetBarsResult_t> GetBarsFuture_t;

    [[nodiscard]] GetBarsFuture_t getBars(const QString &symbol,
                                          unsigned int interval = 1,
                                          Bar::BarUnit unit = Bar::BarUnit::Daily,
                                          unsigned int barsback = 1,
                                          Bar::BarSessionTemplate sessionTemplate = Bar::BarSessionTemplate::Default,
                                          QDateTime firstDate = QDateTime(),
                                          QDateTime lastDate = QDateTime());
    /*
     * Creates a Bars Stream
     *
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
     */
    typedef std::variant<QVector<Account>, AsyncRequestError_e> GetAccountsResult_t;
    typedef QFuture<GetAccountsResult_t> GetAccountsFuture_t;

    [[nodiscard]] GetAccountsFuture_t getAccounts();

    /*
     * Get Balances
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/GetBalances
     */
    typedef std::variant<QVector<Balance>, AsyncRequestError_e> GetBalancesResult_t;
    typedef QFuture<GetBalancesResult_t> GetBalancesFuture_t;

    [[nodiscard]] GetBalancesFuture_t getBalances(const QStringList &accounts);

    /*
     * Creates a StreaOrders Stream
     *
     * @return : nullptr if the stream could not be created
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/StreamOrders
     */
    [[nodiscard]] StreamOrders* openStreamOrders(const QString &account);
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
    [[nodiscard]] StreamPositions* openStreamPositions(const QString &account, bool changes = false);
    void closeStreamPositions(StreamPositions* stream);

                              // -------- Order execution methods --------
 
    /*
     * Place order
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Order-Execution/operation/PlaceOrder
     */
    typedef std::variant<PlaceOrderResult, AsyncRequestError_e> PlaceOrderResult_t;
    typedef QFuture<PlaceOrderResult_t> PlaceOrderFuture_t;

    [[nodiscard]] PlaceOrderFuture_t placeOrder(const PlaceOrderRequest &order);

    /*
     * Cancel order
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Order-Execution/operation/CancelOrder
     */
    typedef std::variant<CancelOrderResult, AsyncRequestError_e> CancelOrderResult_t;
    typedef QFuture<CancelOrderResult_t> CancelOrderFuture_t;

    [[nodiscard]] CancelOrderFuture_t cancelOrder(const QString &orderID);

public slots:
    #ifdef GUI_ENABLED
    // Authentication methods
    void launchAuthProcess();
    #endif

signals:
    // Emited at basically every new message
    void totalDataReceivedBytesIncreased(qsizetype dataSize);
    
    void openStreamCountChanged(size_t count);

    void authStateChanged(bool isAuthenticated, QString reason);

private slots:

    #ifdef GUI_ENABLED
    void onAuthFinished(bool success, AuthToken token, QString reason);
    void onAuthWindowDestroyed();
    #endif

private:
    static TSClient* m_instance; // Singleton instance

    virtual ~TSClient(); // Delete destructor

    explicit TSClient(); // Singleton : private constructor

    void processNewAmountOfDataReceived(size_t bytesReceived);
    [[nodiscard]] QNetworkRequest buildNetworkRequest(const QString &endpoint, const QUrlQuery &query = QUrlQuery()) const;


    // Auth and refresh stuff implemented in TSClientRefreshToken.cpp
    [[nodiscard]] static QNetworkRequest buildRefreshTokenRequest();
    [[nodiscard]] static QByteArray buildRefreshTokenQuery(const QString &clientId, const QString &clientSecret, const QString &refreshToken);                                       
    void refreshAsyncAccessToken();


    void openStream(const QNetworkRequest &request, Stream *stream);
    void closeStream(Stream * const stream);



    // tokens
    AuthToken   m_authToken;
    ClientToken m_clientToken;

    bool m_authenticated     = false;  // Track authentication state
    bool m_refreshInProgress = false;  // Track if authentication process is in progress
    bool m_authInProgress    = false;  // Track if authentication process is in progress

    QUrl m_baseUrl;
    
    qsizetype m_totalDataReceivedBytes = 0;
    QString m_apiKey;
    QThread *m_thread;
    QNetworkAccessManager *m_networkManager;
    QMap<QNetworkReply*, Stream*> m_networkReplyToOpenStreams;

#ifdef GUI_ENABLED
    AuthWindow* m_authWindow = nullptr;  // Authentication window
#endif
};
