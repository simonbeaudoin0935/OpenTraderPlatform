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

// @note : Returned pointer dynamically allocated. Delete with closeStreamBars

class TSClient final : public QObject
{
    Q_OBJECT
public:

    class TimeoutException : public QException {
    public:
        void raise() const override { throw *this; }
        TimeoutException *clone() const override { return new TimeoutException(*this); }
    };

    class JSONErrorException : public QException {
    public:
        void raise() const override { throw *this; }
        JSONErrorException *clone() const override { return new JSONErrorException(*this); }
    };

    class OtherErrorException : public QException {
    public:
        void raise() const override { throw *this; }
        OtherErrorException *clone() const override { return new OtherErrorException(*this); }
    };

    // Singleton : Instance getter
    [[nodiscard]] static TSClient* getInstance();

    TSClient(const TSClient&) = delete; // Delete copy constructor
    TSClient(TSClient&&) = delete; // Delete move constructor
    TSClient& operator=(const TSClient&) = delete; // Delete copy assignment
    TSClient& operator=(TSClient&&) = delete; // Delete move assignment


    void start() { m_thread->start(); };



    /*
     * Get Quote Snapshots
     *
     * @doc : https://api.tradestation.com/docs/specification/#tag/MarketData/operation/GetQuoteSnapshots
     */
    [[nodiscard]] QFuture<QVector<Quote>> getQuoteSnapshots(const QStringList &symbols);

    /*
     * Get Bars asynchronously
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/MarketData/operation/GetBars
     */
    [[nodiscard]] QFuture<QVector<Bar>> getBars(const QString &symbol,
                                          unsigned int interval = 1,
                                          Bar::BarUnit unit = Bar::BarUnit::Daily,
                                          unsigned int barsback = 1,
                                          Bar::BarSessionTemplate sessionTemplate = Bar::BarSessionTemplate::Default,
                                          QDateTime firstDate = QDateTime(),
                                          QDateTime lastDate = QDateTime());


    /*
     * Get Accounts
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/GetAccounts
     */
    [[nodiscard]] QFuture<QVector<Account>> getAccounts();

    /*
     * Get Balances
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/GetBalances
     */
    [[nodiscard]] QFuture<QVector<Balance>>  getBalances(const QStringList &accounts);
 
    /*
     * Place order
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Order-Execution/operation/PlaceOrder
     */
    [[nodiscard]] QFuture<PlaceOrderResult> placeOrder(const PlaceOrderRequest &order);

    /*
     * Cancel order
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Order-Execution/operation/CancelOrder
     */
    [[nodiscard]] QFuture<CancelOrderResult> cancelOrder(const QString &orderID);

    /*
     * Creates a Bars Stream
     *
     * @doc : https://api.tradestation.com/docs/specification/#tag/MarketData/operation/StreamBars
     */
    [[nodiscard]] StreamBars* openStreamBars(const QString &symbol,
                               unsigned int interval = 1,
                               Bar::BarUnit unit = Bar::BarUnit::Daily,
                               unsigned int barsback = 1,
                               Bar::BarSessionTemplate sessionTemplate = Bar::BarSessionTemplate::Default);

    /*
     * Creates a MarketDepthQuote Stream
     *
     * @return : nullptr if the stream could not be created
     * @doc : https://api.tradestation.com/docs/specification/#tag/MarketData/operation/StreamMarketDepthQuotes
     */
    [[nodiscard]] StreamMarketDepthQuote* openStreamMarketDepthQuote(const QString &symbol, unsigned int depth = 20);

    /*
     * Creates a StreaOrders Stream
     *
     * @return : nullptr if the stream could not be created
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/StreamOrders
     */
    [[nodiscard]] StreamOrders* openStreamOrders(const QString &account);

    /*
     * Creates a StreamPositions Stream
     *
     * @return : nullptr if the stream could not be created
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/StreamPositions
     */
    [[nodiscard]] StreamPositions* openStreamPositions(const QString &account, bool changes = false);
    
    void closeStream(Stream* stream);

    [[nodiscard]] qsizetype getTotalDataReceivedBytes() const { return m_totalDataReceivedBytes; };
    [[nodiscard]] bool isCleanedUp();
    [[nodiscard]] size_t getStreamCount() const { return m_networkReplyToOpenStreams.size(); }
    [[nodiscard]] bool isAuthenticated() const { return m_authenticated; }
    [[nodiscard]] bool isAuthInProgress() const { return m_authInProgress; }

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
    void refreshAccessToken();

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
