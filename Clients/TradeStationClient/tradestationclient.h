#ifndef TRADESTATIONCLIENT_H
#define TRADESTATIONCLIENT_H

#include <QObject>
#include <QLoggingCategory>
#include <QVector>
#include "../restclient.h"
#include "Auth/AuthWindow.h"
#include "Auth/authtoken.h"
#include "Account/accountresult.h"

Q_DECLARE_LOGGING_CATEGORY(TradeStationClientLog)

// This is a singleton

class TradeStationClient : public RESTClient {
    Q_OBJECT
public:
    // Singleton : Instance getter
    static TradeStationClient& getInstance();
    static TradeStationClient* getInstancePtr();
    // Singleton : Delete copy constructor and assignment operator
    TradeStationClient(const TradeStationClient&) = delete;
    TradeStationClient& operator=(const TradeStationClient&) = delete;

    // Authentication state getter
    bool isAuthenticated() const { return authenticated; }
    bool isAuthInProgress() const { return authInProgress; }

    // Authentication methods
    void launchAuthProcess(QWidget* parent = nullptr);

    // Static helper methods for authentication
    static QNetworkRequest buildRefreshTokenRequest();
    static QByteArray buildRefreshTokenQuery(const QString &clientId,
                                           const QString &clientSecret,
                                           const QString &refreshToken);

    // Account methods
    // https://api.tradestation.com/docs/specification#tag/Brokerage/operation/GetAccounts
    bool fetchSyncAccounts(QVector<AccountResult> &results);
    void fetchAsyncAccounts();

signals:
    void authStateChanged(bool isAuthenticated, QString reason);
    void accountsReceived(QVector<AccountResult> results);

private slots:
    void onAuthFinished(bool success, AuthToken token, QString reason);
    void onAsyncRefreshTokenFinished(const AuthToken &newToken);
    void onAuthWindowDestroyed();

private:
    // Singleton : private constructor
    explicit TradeStationClient();
    ~TradeStationClient();

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
    static TradeStationClient* instance;

    bool authenticated = false;  // Track authentication state
    bool authInProgress = false;  // Track if authentication process is in progress
    AuthWindow* authWindow = nullptr;  // Authentication window

    // API key placement configuration
    static constexpr ApiKeyPlacement API_KEY_PLACEMENT = ApiKeyPlacement::InHeader;

    friend class TestTradeStationClient;
};

#endif // TRADESTATIONCLIENT_H 
