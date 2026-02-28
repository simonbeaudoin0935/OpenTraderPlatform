#pragma once

#include <expected>
#include <memory>

#include <QMap>
#include <QObject>
#include <QLoggingCategory>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QThread>
#include <QFuture>

#include "AuthToken.h"
#include "ClientToken.h"
#include "Account.h"
#include "StreamOrders.h"
#include "StreamPositions.h"
#include "PlaceOrder.h"
#include "CancelOrder.h"
#include "Balance.h"

#include "Stream.h"
#include "Assume.h"
#include "CONSTANTS.h"

#ifdef GUI_ENABLED
#include "GUIAuthHandler.h"
#else
#include "TUIAuthHandler.h"
#endif

Q_DECLARE_LOGGING_CATEGORY(TSClientLog)

// This is a singleton

class MockNetworkReply;
class MockNetworkAccessManager;
class OrderEmulator;

class TSClient final : public QObject
{
    Q_OBJECT

  public:
    enum class Error : quint8
    {
        Timeout,
        JSONError,
        RejectedByValidator, // This error is never returned by TSClient directly, this would be used by the MainAlgo validator if it rejects the order before going out the door.
        Other
    };
    Q_ENUM(Error)

    /**
     * @brief Authentication state reason
     *
     * Describes why authStateChanged signal was emitted.
     * Allows receivers to distinguish between transient states (e.g., token refresh)
     * and permanent failures (e.g., expired token).
     */
    enum class AuthStateReason : quint8
    {
        // Success states
        ValidToken,        ///< Token is valid and not expired (startup)
        RefreshSuccessful, ///< Token refresh completed successfully

        // Transient states (auth in progress)
        Connecting, ///< Authentication or token refresh in progress

        // Failure states
        TokenExpired,   ///< Refresh token has expired, manual re-auth required
        AuthFailed,     ///< Authentication process failed
        StartupNoToken, ///< No token available at startup (commented out in code)
    };
    Q_ENUM(AuthStateReason)

    /**
     * @brief Client operating mode
     */
    enum class Mode : quint8
    {
        Live,  ///< Normal operation - real network requests
        Replay ///< Replay mode - uses MockNetworkReply for injected data
    };
    Q_ENUM(Mode)

    // Singleton : Instance getter
    [[nodiscard]] static TSClient* getInstance();

    // Singleton : Destroy instance (for cleanup)
    static void destroyInstance();

    Q_DISABLE_COPY_MOVE(TSClient) // Delete copy and move constructors/operators

    void start()
    {
        m_thread.start();
    };


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

    [[nodiscard]] bool isAuthenticated() const
    {
        return m_authenticated;
    }
    [[nodiscard]] bool isAuthInProgress() const
    {
        return m_authInProgress;
    }

    /*
     * Replay Mode Support
     */
    /**
     * @brief Set the operating mode (Live or Replay)
     * @param p_mode New mode
     */
    void setMode(Mode p_mode);

    /**
     * @brief Get the current operating mode
     */
    [[nodiscard]] Mode getMode() const
    {
        return m_mode;
    }

    /**
     * @brief Get the OrderEmulator (replay mode only)
     * @return Pointer to OrderEmulator, or nullptr if not in replay mode
     */
    [[nodiscard]] OrderEmulator* getOrderEmulator() const
    {
        return m_orderEmulator;
    }

    /**
     * @brief Get the replay session timestamp
     * @return Timestamp string (YYYY-MM-DD_HHMMSS), empty if not in replay mode
     */
    [[nodiscard]] QString getReplaySessionTimestamp() const
    {
        return m_replaySessionTimestamp;
    }

  public slots:
    // Authentication methods
    void launchAuthProcess();

  signals:
    /**
     * @brief Signal emitted whenever network data is received
     * Thread context: Emitted from TSClient worker thread
     */
    void totalDataReceivedBytesIncreased(qsizetype dataSize);

    /**
     * @brief Emitted when authentication state changes
     * @param isAuthenticated True if currently authenticated, false otherwise
     * @param reason Enum describing why the state changed
     * @param message Human-readable description (for logging/UI)
     *
     * Common scenarios:
     * - Startup: (true, ValidToken, "Auth token valid and not expired")
     * - Token refresh start: (false, Connecting, "Connecting...")
     * - Token refresh success: (true, RefreshSuccessful, "Auth token refresh successful")
     * - Token expired: (false, TokenExpired, "Token expired")
     */
    void authStateChanged(bool isAuthenticated, AuthStateReason reason, QString message);

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
    QThread m_thread;
    QNetworkAccessManager* m_networkManager;

    // Replay mode support
    Mode m_mode = Mode::Live;
    QPointer<MockNetworkReply> m_replayOrdersReply;    // MockNetworkReply for orders stream
    QPointer<MockNetworkReply> m_replayPositionsReply; // MockNetworkReply for positions stream

    // Order emulation for replay mode
    OrderEmulator* m_orderEmulator = nullptr;                 // Created when entering replay mode
    MockNetworkAccessManager* m_mockNetworkManager = nullptr; // Created when entering replay mode
    QString m_replaySessionTimestamp;                         // Timestamp when replay mode was entered

#ifdef GUI_ENABLED
    GUIAuthHandler* m_authHandler = nullptr; // GUI authentication handler
#else
    TUIAuthHandler* m_authHandler = nullptr; // TUI authentication handler
#endif
};
