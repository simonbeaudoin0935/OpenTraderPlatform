#pragma once

#include <QObject>
#include <QLoggingCategory>
#include <QVector>

#include "../RESTClient.h"
#include "Auth/AuthToken.h"
#include "Auth/ClientToken.h"
#include "Brokerage/GetAccounts/Account.h"
#include "OrderExecution/PlaceOrder/PlaceOrder.h"
#include "MarketData/StreamBars/StreamBars.h"
#include "MarketData/StreamMarketDepthQuote/StreamMarketDepthQuote.h"
#ifdef GUI_ENABLED
#include "Auth/AuthWindow.h"
#endif

Q_DECLARE_LOGGING_CATEGORY(TSClientLog)

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
     * Creates a Bars Stream
     *
     * @return : nullptr if the stream could not be created
     * @doc : https://api.tradestation.com/docs/specification/#tag/MarketData/operation/StreamBars
     *
     * @note : Returned pointer dynamically allocated. Delete with closeStreamBars
     *
     * @note : ->startStream() needs to be called in order to start the stream. This gives time to the
     *         caller to setup signal/slot connections
     */
    enum class StreamBarsUnit { Minute, Daily, Weekly, Monthly };
    enum class StreamBarsSessionTemplate { USEQPre, USEQPost, USEPreAndPost,USEQ24Hour, Default };

    StreamBars* openStreamBars(QString &symbol,
                               unsigned int interval = 1,
                               StreamBarsUnit unit = StreamBarsUnit::Daily,
                               unsigned int barsback = 1,
                               StreamBarsSessionTemplate sesstionTemplate = StreamBarsSessionTemplate::Default);
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
     *
     * @note : ->startStream() needs to be called in order to start the stream. This gives time to the
     *         caller to setup signal/slot connections
     */
    StreamMarketDepthQuote* openStreamMarketDepthQuote(QString &symbol, unsigned int depth = 20);
    void closeStreamMarketDepthQuote(StreamMarketDepthQuote* stream);

                              // -------- Brokerage methods -------------

    /*
     * Get Accounts
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Brokerage/operation/GetAccounts
     */
    bool fetchSyncAccounts(QVector<Account> &results);
    void fetchAsyncAccounts();


                              // -------- Order execution methods --------
 
    /*
     * Place order
     *
     * @doc : https://api.tradestation.com/docs/specification#tag/Order-Execution/operation/PlaceOrder
     */
    bool placeSyncOrder(const PlaceOrderRequest &order, PlaceOrderResult &result);
    void placeAsyncOrder(const PlaceOrderRequest &order);

public slots:
    #ifdef GUI_ENABLED
    // Authentication methods
    void launchAuthProcess(QWidget* parent = nullptr);
    #endif

signals:
    void authStateChanged(bool isAuthenticated, QString reason);
    void accountsAsyncReceived(QVector<Account> results);
    void placeAsyncOrderReceived(const PlaceOrderResult &result);

    void marketDepthNotAvailable();

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
        Accounts,
        RefreshAccessToken
    };

    void emitSignalDemuxer(RequestTypeInt type, const QJsonDocument &doc) override;

    void processStreamFinished(QByteArray &rawData, void *arg) override;

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


    void openStream(const QString &symbol, const QString &endpoint, const QUrlQuery &query, Stream * const stream);
    void closeStream(Stream* const stream);
};
