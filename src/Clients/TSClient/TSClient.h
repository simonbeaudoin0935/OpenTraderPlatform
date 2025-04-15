#pragma once

#include <QObject>
#include <QLoggingCategory>
#include <QVector>

#include "RESTClient.h"
#include "AuthToken.h"
#include "ClientToken.h"
#include "Account.h"
#include "StreamPositions.h"
#include "PlaceOrder.h"
#include "QuoteSnapshot.h"
#include "StreamBars.h"
#include "StreamMarketDepthQuote.h"
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
#define ENDPOINT_STREAM_POSITIONS          "brokerage/stream/accounts/%1/positions"

#define ENDPOINT_PLACE_ORDER               "orderexecution/orders"

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
                               Bar::BarSessionTemplate sesstionTemplate = Bar::BarSessionTemplate::Default);
    void closeStreamBars(StreamBars* stream);

    bool getBarsSync(QVector<Bar> &results,
                     QString &symbol,
                     unsigned int interval = 1,
                     Bar::BarUnit unit = Bar::BarUnit::Daily,
                     unsigned int barsback = 1,
                     Bar::BarSessionTemplate sessionTemplate = Bar::BarSessionTemplate::Default,
                     QDateTime firstDate = QDateTime(),
                     QDateTime lastDate = QDateTime());

    void getBarsAsync(QString &symbol,
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
    StreamMarketDepthQuote* openStreamMarketDepthQuote(QString &symbol, unsigned int depth = 20);
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

public slots:
    #ifdef GUI_ENABLED
    // Authentication methods
    void launchAuthProcess();
    #endif

signals:
    void authStateChanged(bool isAuthenticated, QString reason);
    void accountsAsyncReceived(QVector<Account> results);
    void quoteSnapshotsAsyncReceived(QVector<QuoteSnapshot> quoteSnapshots);
    void placeAsyncOrderReceived(const PlaceOrderResult &result);
    void getBarsAsyncReceived(QString symbol, QVector<Bar> bars);

private slots:
    #ifdef GUI_ENABLED
    void onAuthFinished(bool success, AuthToken token, QString reason);
    void onAuthWindowDestroyed();
    #endif
    void onAsyncRefreshTokenFinished(const AuthToken &newToken);

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
        GetBars,
        GetQuoteSnapshots,
        GetRefreshAccessToken
    };

    void emitSignalDemuxer(RequestTypeInt type, const QJsonDocument &doc, void* optArg = nullptr) override;

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

    friend class TestTSClient; // For unit testing
    friend class TestBarCache; // For unit testing

    void openStream(const QString &symbol, const QString &endpoint, const QUrlQuery &query, Stream * const stream);
    void closeStream(Stream* const stream);
};
