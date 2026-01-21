#pragma once

#include <expected>

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
#include "Assume.h"

#ifdef GUI_ENABLED
#include "GUIAuthHandler.h"
#else
#include "TUIAuthHandler.h"
#endif

Q_DECLARE_LOGGING_CATEGORY(TSClientLog)

#define ENDPOINT_GET_QUOTE_SNAPSHOTS "marketdata/quotes/%1"
#define ENDPOINT_GET_BARS "marketdata/barcharts/%1"
#define ENDPOINT_STREAM_BARS "marketdata/stream/barcharts/%1"
#define ENDPOINT_STREAM_MARKET_DEPTH_QUOTE "marketdata/stream/marketdepth/quotes/%1"

#define ENDPOINT_GET_ACCOUNTS "brokerage/accounts"
#define ENDPOINT_GET_BALANCES "brokerage/accounts/%1/balances"
#define ENDPOINT_STREAM_ORDERS "brokerage/stream/accounts/%1/orders"
#define ENDPOINT_STREAM_POSITIONS "brokerage/stream/accounts/%1/positions"

#define ENDPOINT_PLACE_ORDER "orderexecution/orders"
#define ENDPOINT_CANCEL_ORDER "orderexecution/orders/%1"

// This is a singleton

// @note : Returned pointer dynamically allocated. Delete with closeStreamBars

class TSClient final : public QObject
{
    Q_OBJECT
  public:
    enum class Error : quint8
    {
        Timeout,
        JSONError,
        Other
    };
    Q_ENUM(Error)


    // Singleton : Instance getter
    [[nodiscard]] static TSClient* getInstance();

    Q_DISABLE_COPY_MOVE(TSClient) // Delete copy and move constructors/operators

    void start()
    {
        m_thread->start();
    };


    /*
     * Get Quote Snapshots
     *
     * @doc : https://api.tradestation.com/docs/specification/#tag/MarketData/operation/GetQuoteSnapshots
     */
    [[nodiscard]] QFuture<std::expected<QVector<Quote>, Error>> getQuoteSnapshots(const QStringList& symbols);

    /*
     * Get Bars asynchronously
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/MarketData/operation/GetBars
     */
    [[nodiscard]] QFuture<std::expected<std::shared_ptr<QVector<Bar>>, Error>>
    getBars(const QString& symbol,
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
    [[nodiscard]] QFuture<std::expected<QVector<Account>, Error>> getAccounts();

    /*
     * Get Balances
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/GetBalances
     */
    [[nodiscard]] QFuture<std::expected<QVector<Balance>, Error>> getBalances(const QStringList& accounts);

    /*
     * Place order
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Order-Execution/operation/PlaceOrder
     */
    [[nodiscard]] QFuture<std::expected<PlaceOrderResult, Error>> placeOrder(const PlaceOrderRequest& order);

    /*
     * Cancel order
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Order-Execution/operation/CancelOrder
     */
    [[nodiscard]] QFuture<std::expected<CancelOrderResult, Error>> cancelOrder(const QString& orderID);

    /*
     * Creates a Bars Stream
     *
     * @doc : https://api.tradestation.com/docs/specification/#tag/MarketData/operation/StreamBars
     */
    [[nodiscard]] QPointer<StreamBars>
    openStreamBars(const QString& symbol,
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
    [[nodiscard]] QPointer<StreamMarketDepthQuote> openStreamMarketDepthQuote(const QString& symbol,
                                                                              unsigned int depth = 20);

    /*
     * Creates a StreaOrders Stream
     *
     * @return : nullptr if the stream could not be created
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/StreamOrders
     */
    [[nodiscard]] QPointer<StreamOrders> openStreamOrders(const QString& account);

    /*
     * Creates a StreamPositions Stream
     *
     * @return : nullptr if the stream could not be created
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/StreamPositions
     */
    [[nodiscard]] QPointer<StreamPositions> openStreamPositions(const QString& account, bool changes = false);

    void closeStream(Stream* stream);

    [[nodiscard]] qsizetype getTotalDataReceivedBytes() const
    {
        return m_totalDataReceivedBytes;
    };
    [[nodiscard]] bool isCleanedUp();
    [[nodiscard]] static size_t getStreamCount()
    {
        return Stream::getNumberOpenStream();
    }
    [[nodiscard]] bool isAuthenticated() const
    {
        return m_authenticated;
    }
    [[nodiscard]] bool isAuthInProgress() const
    {
        return m_authInProgress;
    }

  public slots:
    // Authentication methods
    void launchAuthProcess();

  signals:
    // Emited at basically every new message
    void totalDataReceivedBytesIncreased(qsizetype dataSize);

    void openStreamCountChanged(size_t count);

    void authStateChanged(bool isAuthenticated, QString reason);

  private slots:
    void onAuthFinished(bool success, AuthToken token, QString reason);
    void onAuthHandlerDestroyed();

  private:
    static TSClient* m_instance; // Singleton instance

    virtual ~TSClient(); // Delete destructor

    explicit TSClient(); // Singleton : private constructor

    void processNewAmountOfDataReceived(size_t bytesReceived);
    [[nodiscard]] QNetworkRequest buildNetworkRequest(const QString& endpoint,
                                                      const QUrlQuery& query = QUrlQuery()) const;


    // Auth and refresh stuff implemented in TSClientRefreshToken.cpp
    [[nodiscard]] static QNetworkRequest buildRefreshTokenRequest();
    [[nodiscard]] static QByteArray
    buildRefreshTokenQuery(const QString& clientId, const QString& clientSecret, const QString& refreshToken);
    void refreshAccessToken();

    // tokens
    AuthToken m_authToken;
    ClientToken m_clientToken;

    bool m_authenticated = false;     // Track authentication state
    bool m_refreshInProgress = false; // Track if authentication process is in progress
    bool m_authInProgress = false;    // Track if authentication process is in progress

    QUrl m_baseUrl;

    qsizetype m_totalDataReceivedBytes = 0;
    QString m_apiKey;
    QThread* m_thread;
    QNetworkAccessManager* m_networkManager;

#ifdef GUI_ENABLED
    GUIAuthHandler* m_authHandler = nullptr; // GUI authentication handler
#else
    TUIAuthHandler* m_authHandler = nullptr; // TUI authentication handler
#endif
};
