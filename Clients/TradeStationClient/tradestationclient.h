#ifndef TRADESTATIONCLIENT_H
#define TRADESTATIONCLIENT_H

#include <QObject>
#include <QLoggingCategory>
#include <QVector>
#include <atomic>
#include "../restclient.h"
#include "Auth/AuthWindow.h"
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

    // Account methods
    // https://api.tradestation.com/docs/specification#tag/Brokerage/operation/GetAccounts
    bool fetchSyncAccounts(QVector<AccountResult> &results);

signals:
    void authenticationStateChanged(bool isAuthenticated);
    void authenticationError(const QString& error);

private slots:
    void handleAuthCompleted(bool success);
    void handleAuthFailed(const QString error);
    void handleAuthWindowDestroyed();

private:
    // Singleton : private constructor
    explicit TradeStationClient();
    ~TradeStationClient();

    enum class RequestType {
        None
    };

    void emitSignalDemuxer(RequestTypeInt type, const QJsonDocument &doc);

    // Singleton
    static TradeStationClient* instance;
    bool authenticated = false;  // Track authentication state
    bool authInProgress = false;  // Track if authentication process is in progress
    AuthWindow* authWindow = nullptr;  // Authentication window

    friend class TestTradeStationClient;
};

#endif // TRADESTATIONCLIENT_H 
