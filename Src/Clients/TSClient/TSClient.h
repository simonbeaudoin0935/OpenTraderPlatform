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

#define ASYNC_REQUEST_TIMEOUT_MS_DEFAULT 5000

// This is a singleton

class TSClient final : public RESTClient {
    Q_OBJECT
public:

    // Singleton : Instance getter  and delete copy and assignment
    static TSClient& getInstance();
    static TSClient* getInstancePtr();
    TSClient(const TSClient&) = delete;
    TSClient& operator=(const TSClient&) = delete;


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
    #warning TODO replace symbol QString for QStringList
    [[nodiscard]] RESTClient::requestID_t getQuoteSnapshotsAsync(const QString &symbols);

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
                               Bar::BarSessionTemplate sesstionTemplate = Bar::BarSessionTemplate::Default,
                               bool mock = false);
    void closeStreamBars(StreamBars* stream);


    /*
     * Get Bars asynchronously
     *
     * @return :
     *  > 0 : The request ID of the async request
     *  0 : Failed to start the async request
     */
    [[nodiscard]] RESTClient::requestID_t getBarsAsync(const QString &symbol,
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
    [[nodiscard]] RESTClient::requestID_t getAccountsAsync();
    /*
     * Get Balances
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/GetBalances
     * 
     * @return :
     *  > 0 : The request ID of the async request
     *  0 : Failed to start the async request
     */
    [[nodiscard]] RESTClient::requestID_t getBalancesAsync(const QString &account);

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
    [[nodiscard]] RESTClient::requestID_t placeOrderAsync(const PlaceOrderRequest &order);

    /*
     * Cancel order
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Order-Execution/operation/CancelOrder
     * 
     * @return :
     *   > 0 : The request ID of the async request
     *   0 : Failed to start the async request
     */
    [[nodiscard]] RESTClient::requestID_t cancelOrderAsync(const QString &orderID);

public slots:
    #ifdef GUI_ENABLED
    // Authentication methods
    void launchAuthProcess();
    #endif

signals:
    void authStateChanged(bool isAuthenticated, QString reason);

    void receivedAsyncGetAccounts      (RESTClient::requestID_t requestID, RequestStatus status, QVector<Account> results);
    void receivedAsyncGetBalances      (RESTClient::requestID_t requestID, RequestStatus status, QVector<Balance> results);
    void receivedAsyncGetQuoteSnapshots(RESTClient::requestID_t requestID, RequestStatus status, QVector<QuoteSnapshot> quoteSnapshots);
    void receivedAsyncPlaceOrder       (RESTClient::requestID_t requestID, RequestStatus status, PlaceOrderResult result);
    void receivedAsyncCancelOrder      (RESTClient::requestID_t requestID, RequestStatus status, CancelOrderResult result);
    // TODO: allocate the vector before emitting the signal to avoid copies and delete later in the receiver
    void receivedAsyncGetBars          (RESTClient::requestID_t requestID, RequestStatus status, QString symbol, QVector<Bar> bars);
    
private slots:

    void onAsyncRefreshTokenFinished(RESTClient::requestID_t requestID, RequestStatus status, AuthToken newToken);
    #ifdef GUI_ENABLED
    void onAuthFinished(bool success, AuthToken token, QString reason);
    void onAuthWindowDestroyed();
    #endif

private:

    enum class RequestType_t {
        None,
        GetAccounts,
        GetBalances,
        GetBars,
        GetQuoteSnapshots,
        GetRefreshAccessToken,
        PlaceOrder,
        CancelOrder,
        MAX_REQUEST_TYPE
    };

    // Singleton : private constructor
    explicit TSClient();
    ~TSClient();

    // Static helper methods for authentication
    [[nodiscard]] static QNetworkRequest buildRefreshTokenRequest();
    [[nodiscard]] static QByteArray buildRefreshTokenQuery(const QString &clientId,
                                                           const QString &clientSecret,
                                                           const QString &refreshToken);
                                           
    void refreshAsyncAccessToken();

    void finalClassDemuxReceivedAsyncRequestReply(RequestTypeBase_t type, const QJsonDocument &doc, requestID_t requestID, RequestStatus status) override;

    void finalClassDemuxReceivedStreamReply(StreamTypeBase_t streamType, const QJsonDocument &doc, requestID_t requestID, RequestStatus status) = 0;


  
    //********* members ********/

    // Singleton
    static TSClient* m_instance;

    // tokens
    AuthToken m_authToken;
    ClientToken m_clientToken;


    bool m_authenticated = false;  // Track authentication state
    bool m_refreshInProgress = false;  // Track if authentication process is in progress
    bool m_authInProgress = false;  // Track if authentication process is in progress

    RESTClient::requestID_t m_asyncTokenRefreshRequestId = 0; // Store the request ID of the ongoing token refresh request

#ifdef GUI_ENABLED
    AuthWindow* m_authWindow = nullptr;  // Authentication window
#endif
};
