#ifndef TRADESTATIONCLIENT_H
#define TRADESTATIONCLIENT_H

#include <QObject>
#include <QLoggingCategory>
#include <QVector>
#include "../RESTClient.h"
#include "Auth/AuthWindow.h"
#include "Auth/AuthToken.h"
#include "Brokerage/Accounts/AccountsResult.h"
#include "OrderExecution/PlaceOrder/PlaceOrder.h"

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

    // -------- Market data methods --------    

    // -------- Brokerage methods ----------

    // https://api.tradestation.com/docs/specification#tag/Brokerage/operation/GetAccounts
    bool fetchSyncAccounts(QVector<AccountsResult> &results);
    void fetchAsyncAccounts();


    // -------- Order execution methods --------
 
    // Place order
    // https://api.tradestation.com/docs/specification#tag/Order-Execution/operation/PlaceOrder    // https://api.tradestation.com/docs/specification#tag/Brokerage/operation/PostOrder
    bool placeSyncOrder(const PlaceOrderRequest &order, PlaceOrderResult &result);
    void placeAsyncOrder(const PlaceOrderRequest &order);


public slots:
    // Authentication methods
    void launchAuthProcess(QWidget* parent = nullptr);


signals:
    void authStateChanged(bool isAuthenticated, QString reason);
    void accountsAsyncReceived(QVector<AccountsResult> results);
    void placeAsyncOrderReceived(const PlaceOrderResult &result);

private slots:
    void onAuthFinished(bool success, AuthToken token, QString reason);
    void onAsyncRefreshTokenFinished(const AuthToken &newToken);
    void onAuthWindowDestroyed();

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

    void emitSignalDemuxer(RequestTypeInt type, const QJsonDocument &doc);

    // tokens
    AuthToken authToken;
    ClientToken clientToken;

    // Singleton
    static TSClient* instance;

    bool authenticated = false;  // Track authentication state
    bool authInProgress = false;  // Track if authentication process is in progress
    AuthWindow* authWindow = nullptr;  // Authentication window

    // API key placement configuration
    static constexpr ApiKeyPlacement API_KEY_PLACEMENT = ApiKeyPlacement::InHeader;

    friend class TestTSClient;
};

#endif // TRADESTATIONCLIENT_H 
