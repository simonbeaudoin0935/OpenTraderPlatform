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

class TSClient : public RESTClient {
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


    // Stream count getter
    [[nodiscard]] int getStreamCount() const { return m_streams.size(); }
                              // -------- Market data methods ----------
    /*
     * Get Quote Snapshots
     *
     * @doc : https://api.tradestation.com/docs/specification/#tag/MarketData/operation/GetQuoteSnapshots
     * 
     * @return :
     *  > 0 : The request ID of the async request
     *  0 : Failed to start the async request
     */
    [[nodiscard]] size_t getQuoteSnapshotsAsync(QString &symbols, const std::chrono::milliseconds &timeout = std::chrono::milliseconds(5000));

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
    [[nodiscard]] size_t getBarsAsync(const QString &symbol,
                                      unsigned int interval = 1,
                                      Bar::BarUnit unit = Bar::BarUnit::Daily,
                                      unsigned int barsback = 1,
                                      Bar::BarSessionTemplate sessionTemplate = Bar::BarSessionTemplate::Default,
                                      QDateTime firstDate = QDateTime(),
                                      QDateTime lastDate = QDateTime(),
                                      const std::chrono::milliseconds &timeout = std::chrono::milliseconds(ASYNC_REQUEST_TIMEOUT_MS_DEFAULT));

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
    [[nodiscard]] size_t getAccountsAsync(const std::chrono::milliseconds &timeout = std::chrono::milliseconds(5000));
    /*
     * Get Balances
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/GetBalances
     * 
     * @return :
     *  > 0 : The request ID of the async request
     *  0 : Failed to start the async request
     */
    [[nodiscard]] size_t getBalancesAsync(const QString &account, const std::chrono::milliseconds &timeout = std::chrono::milliseconds(ASYNC_REQUEST_TIMEOUT_MS_DEFAULT));

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
    [[nodiscard]] size_t placeOrderAsync(const PlaceOrderRequest &order, const std::chrono::milliseconds &timeout = std::chrono::milliseconds(ASYNC_REQUEST_TIMEOUT_MS_DEFAULT));

    /*
     * Cancel order
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Order-Execution/operation/CancelOrder
     * 
     * @return :
     *   > 0 : The request ID of the async request
     *   0 : Failed to start the async request
     */
    [[nodiscard]] size_t cancelOrderAsync(const QString &orderID, const std::chrono::milliseconds &timeout = std::chrono::milliseconds(ASYNC_REQUEST_TIMEOUT_MS_DEFAULT));

public slots:
    #ifdef GUI_ENABLED
    // Authentication methods
    void launchAuthProcess();
    #endif

signals:
    void authStateChanged(bool isAuthenticated, QString reason);
    void streamCountChanged(int count);

    void receivedAsyncGetAccounts      (size_t requestID, RequestStatus status, QVector<Account> results);
    void receivedAsyncGetBalances      (size_t requestID, RequestStatus status, QVector<Balance> results);
    void receivedAsyncGetQuoteSnapshots(size_t requestID, RequestStatus status, QVector<QuoteSnapshot> quoteSnapshots);
    void receivedAsyncPlaceOrder       (size_t requestID, RequestStatus status, PlaceOrderResult result);
    void receivedAsyncCancelOrder      (size_t requestID, RequestStatus status, CancelOrderResult result);

    // TODO: allocate the vector before emitting the signal to avoid copies and delete later in the receiver
    void receivedAsyncGetBars          (size_t requestID, RequestStatus status, QString symbol, QVector<Bar> bars);
    
private slots:

    void onAsyncRefreshTokenFinished(size_t requestID, RequestStatus status, AuthToken newToken);
    #ifdef GUI_ENABLED
    void onAuthFinished(bool success, AuthToken token, QString reason);
    void onAuthWindowDestroyed();
    #endif

private:

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

    // Singleton : private constructor
    explicit TSClient();
    ~TSClient();

    // Static helper methods for authentication
    static QNetworkRequest buildRefreshTokenRequest();
    static QByteArray buildRefreshTokenQuery(const QString &clientId,
                                             const QString &clientSecret,
                                             const QString &refreshToken);
                                           
    void refreshAsyncAccessToken();

    void emitSignalDemuxer(RequestTypeInt type, const QJsonDocument &doc, size_t requestID, RequestStatus status, void* optArg = nullptr) override;

    #warning FIX THIS SHIT
    template<typename T, typename... Args>
    T* openStream(const QString &symbol, const QString &endpoint, const QUrlQuery &query, Args&&... args) {
        Q_ASSERT(!symbol.isEmpty());
        if (symbol != "NOSYMBOL") Q_ASSERT_X(symbol.length() >= 1 && symbol.length() <= 8, symbol.toStdString().c_str(), "Symbol length must be between 1 and 8 characters");
        Q_ASSERT(!symbol.contains(','));
        Q_ASSERT(symbol.isUpper());
        Q_ASSERT(!endpoint.isEmpty());

        // Only external callers to TSClient thread should get here. Calling a fetch sync from within the TSClient's
        // thread would cause a deadlock to itself
        Q_ASSERT_X(QThread::currentThread() != m_thread, Q_FUNC_INFO, "TSClient object cannot call this function itself");

        T* stream = nullptr;

        // FIXME fix this NOSYMBOL shit
        QMetaObject::invokeMethod(this,
            [this, &symbol, &endpoint, &query, &stream, args...]() mutable
            {
                stream = new T(std::forward<Args>(args)..., nullptr);

                QUrl url(m_baseUrl);
                url.setPath(m_baseUrl.path() + endpoint + ((symbol=="NOSYMBOL") ? "" : ("/" + symbol)));
                url.setQuery(query);

                QNetworkRequest request(url);
                request.setRawHeader("Authorization", QString("Bearer %1").arg(m_authToken.getAccessToken()).toUtf8());

                QNetworkReply *reply = fetchStream(request, static_cast<void*>(stream));


                // The stream is now created in the TSClient thread
                stream->setParent(this);

                m_streams[reply] = stream;

                // The readyRead, finished and errorOccured are connected internaly here.
                // This call starts the timeout timer as well
                stream->setNetworkReply(reply);

                connect(stream, &Stream::receivedAmountOfData, this, &TSClient::onReceivedNewAmountOfData);
                
                // Emit signal that stream count has changed
                emit streamCountChanged(m_streams.size());
            },
        Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
                                       // finishes executing this lambda so that a valid pointer is returned

        qCDebug(TSClientLog) << Q_FUNC_INFO << "Opened Stream " << static_cast<void*>(stream);

        return stream;
    }
    void closeStream(Stream* const stream);

  
    //********* members ********/

    // Singleton
    static TSClient* m_instance;

    // tokens
    AuthToken m_authToken;
    ClientToken m_clientToken;


    bool m_authenticated = false;  // Track authentication state
    bool m_refreshInProgress = false;  // Track if authentication process is in progress
    bool m_authInProgress = false;  // Track if authentication process is in progress

    size_t m_asyncTokenRefreshRequestId = 0; // Store the request ID of the ongoing token refresh request

#ifdef GUI_ENABLED
    AuthWindow* m_authWindow = nullptr;  // Authentication window
#endif
};
