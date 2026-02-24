#pragma once

#include <expected>
#include <memory>

#include <QMap>
#include <QObject>
#include <QLoggingCategory>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QThread>
#include <QFuture>
#include <QPromise>
#include <QQueue>
#include <QJsonObject>
#include <QDate>
#include <deque>

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
#include "StreamQuote.h"
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

// @note : Returned pointer dynamically allocated. Delete with closeStreamBars

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

    /**
     * @brief Pending market depth stream request
     *
     * Stores information needed to open a queued market depth stream
     * when a slot becomes available (count < MAX_CONCURRENT_STREAMS).
     */
    struct PendingMarketDepthRequest
    {
        QString symbol;                                     ///< Stock ticker symbol
        unsigned int depth;                                 ///< Number of depth levels (1-20)
        QPromise<QPointer<StreamMarketDepthQuote>> promise; ///< Promise to fulfill when stream opens

        // Constructor that moves the promise
        PendingMarketDepthRequest(QString p_symbol,
                                  unsigned int p_depth,
                                  QPromise<QPointer<StreamMarketDepthQuote>>&& p_promise)
            : symbol(std::move(p_symbol)), depth(p_depth), promise(std::move(p_promise))
        {
        }

        // Make movable but not copyable (QPromise is move-only)
        PendingMarketDepthRequest() = delete;
        PendingMarketDepthRequest(const PendingMarketDepthRequest&) = delete;
        PendingMarketDepthRequest& operator=(const PendingMarketDepthRequest&) = delete;
        PendingMarketDepthRequest(PendingMarketDepthRequest&&) = default;
        PendingMarketDepthRequest& operator=(PendingMarketDepthRequest&&) = default;
    };


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
     * @param symbol Stock ticker symbol
     * @param depth Number of market depth levels to request (1-20, default 20)
     * @return If count < MAX_CONCURRENT_STREAMS: std::expected with QPointer to stream
     *         If count >= MAX_CONCURRENT_STREAMS: std::expected with QFuture that completes when slot available
     *
     * @note TradeStation API enforces maximum of 10 concurrent market depth streams
     * @note If limit reached, request is queued (FIFO) and QFuture will complete after ~1000ms delay
     * @note Caller MUST handle QFuture - cannot cancel queued requests
     *
     * @doc : https://api.tradestation.com/docs/specification/#tag/MarketData/operation/StreamMarketDepthQuotes
     */
    [[nodiscard]] std::expected<QPointer<StreamMarketDepthQuote>, QFuture<QPointer<StreamMarketDepthQuote>>>
    openStreamMarketDepthQuote(const QString& symbol, unsigned int depth = 20);

    /*
     * Creates a Quote Stream (Level 1 bid/ask stream)
     *
     * @param symbols Comma-delimited symbol list (max 100 symbols per stream request)
     * @doc : https://api.tradestation.com/docs/specification/#tag/MarketData/operation/GetQuoteChangeStream
     */
    [[nodiscard]] QPointer<StreamQuote> openStreamQuote(const QStringList& symbols);

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

    /**
     * @brief Get total number of open streams (all types combined)
     * @return Sum of all stream type counters
     */
    [[nodiscard]] static size_t getStreamCount()
    {
        return StreamBars::getNumberOfBarsStreams() + StreamMarketDepthQuote::getNumberOfMarketDepthStreams() +
               StreamQuote::getNumberOfQuoteStreams() + StreamPositions::getNumberOfPositionStreams() +
               StreamOrders::getNumberOfOrderStreams();
    }
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
     * @brief Check if a bar stream exists for the given symbol
     * @param p_symbol Stock ticker symbol
     * @return true if a stream (real or mock) exists
     */
    [[nodiscard]] bool hasOpenBarStream(const QString& p_symbol) const;

    /**
     * @brief Check if a market depth stream exists for the given symbol
     * @param p_symbol Stock ticker symbol
     * @return true if a stream (real or mock) exists
     */
    [[nodiscard]] bool hasOpenMarketDepthStream(const QString& p_symbol) const;

    /**
     * @brief Check if a quote stream exists for the given symbol
     * @param p_symbol Stock ticker symbol
     * @return true if a stream (real or mock) exists
     */
    [[nodiscard]] bool hasOpenQuoteStream(const QString& p_symbol) const;

    /**
     * @brief Check if any quote stream is currently open
     * @return true if at least one quote stream exists
     */
    [[nodiscard]] bool hasOpenQuoteStream() const;

    /**
     * @brief Get the MockNetworkReply for a bar stream (replay mode only)
     * @param p_symbol Stock ticker symbol
     * @return Pointer to MockNetworkReply, or nullptr if not found
     */
    [[nodiscard]] MockNetworkReply* getBarReplyForSymbol(const QString& p_symbol) const;

    /**
     * @brief Get the MockNetworkReply for a market depth stream (replay mode only)
     * @param p_symbol Stock ticker symbol
     * @return Pointer to MockNetworkReply, or nullptr if not found
     */
    [[nodiscard]] MockNetworkReply* getMarketDepthReplyForSymbol(const QString& p_symbol) const;

    /**
     * @brief Get the MockNetworkReply for a quote stream (replay mode only)
     * @param p_symbol Stock ticker symbol
     * @return Pointer to MockNetworkReply, or nullptr if not found
     */
    [[nodiscard]] MockNetworkReply* getQuoteReplyForSymbol(const QString& p_symbol) const;

    /**
     * @brief Check if the market depth queue is empty
     * @return true if no pending requests, false otherwise
     * @note Thread-safe when called from TSClient thread
     */
    [[nodiscard]] bool isMarketDepthQueueEmpty() const
    {
        return m_marketDepthQueue.empty();
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

    /**
     * @brief Inject bar data into the appropriate MockNetworkReply (replay mode)
     * @param p_symbol Stock ticker symbol
     * @param p_data Shared pointer to JSON data
     *
     * Called via QueuedConnection from ReplayEngine (cross-thread).
     * Safely executes in TSClient's thread context.
     */
    void onInjectBarData(const QString& p_symbol, std::shared_ptr<const QByteArray> p_data);

    /**
     * @brief Inject market depth data into the appropriate MockNetworkReply (replay mode)
     * @param p_symbol Stock ticker symbol
     * @param p_data Shared pointer to JSON data
     *
     * Called via QueuedConnection from ReplayEngine (cross-thread).
     * Safely executes in TSClient's thread context.
     */
    void onInjectDepthData(const QString& p_symbol, std::shared_ptr<const QByteArray> p_data);

    /**
     * @brief Inject quote data into the appropriate MockNetworkReply (replay mode)
     * @param p_symbol Stock ticker symbol
     * @param p_data Shared pointer to JSON data
     */
    void onInjectQuoteData(const QString& p_symbol, std::shared_ptr<const QByteArray> p_data);

    /**
     * @brief Pre-roll quote state from start-of-day to p_startEpochMs (replay mode).
     *
     * The TradeStation Quote Stream is differential: the very first message per symbol
     * is a full snapshot. When the replay starts mid-day, all snapshots have already
     * passed. This method reads every QuoteStream record recorded before p_startEpochMs,
     * merges them per-symbol into m_replayQuoteState (for the OrderEmulator), and injects
     * the final merged state into any open MockNetworkReply so StreamQuote is also primed.
     *
     * Must be called via BlockingQueuedConnection from MainAlgo's thread before starting
     * the ReplayEngine, so that the state is fully built before the first delta arrives.
     */
    void preRollQuoteState(QDate p_date, qint64 p_startEpochMs);

    /**
     * @brief Process the next queued market depth stream request
     *
     * Called after a market depth stream is destroyed and the queue is not empty.
     * Delayed by QUEUE_PROCESS_DELAY_MS to allow TCP FIN to propagate to server.
     *
     * @note Must be called from TSClient thread
     * @note Automatically stops processing if queue is empty or shutdown in progress
     */
    void processMarketDepthQueue();

  signals:
    /**
     * @brief Signal emitted whenever network data is received
     * Thread context: Emitted from TSClient worker thread
     */
    void totalDataReceivedBytesIncreased(qsizetype dataSize);

    /**
     * @brief Emitted when a new Level 1 quote is received (live or replay)
     * @param symbol The stock symbol
     * @param quote The parsed Quote object with bid/ask and market flags
     */
    void newQuoteReceived(const QString& symbol, const Quote& quote);

    /**
     * @brief Emitted when stream counts change
     * @param barsCount Number of currently open bar streams
     * @param marketDepthCount Number of currently open market depth streams
     * @note Does not track Positions/Orders streams (always 0 or 1)
     */
    void streamCountsChanged(size_t barsCount, size_t marketDepthCount);

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
    QMap<QString, QPointer<MockNetworkReply>> m_replayBarReplies;   // symbol -> MockNetworkReply for bars
    QMap<QString, QPointer<MockNetworkReply>> m_replayDepthReplies; // symbol -> MockNetworkReply for depth
    QMap<QString, QPointer<MockNetworkReply>> m_replayQuoteReplies; // symbol -> MockNetworkReply for quotes
    QMap<QString, QJsonObject> m_replayQuoteState;     // Accumulated quote state per symbol (for delta merging)
    QPointer<MockNetworkReply> m_replayOrdersReply;    // MockNetworkReply for orders stream
    QPointer<MockNetworkReply> m_replayPositionsReply; // MockNetworkReply for positions stream

    // Order emulation for replay mode
    OrderEmulator* m_orderEmulator = nullptr;                 // Created when entering replay mode
    MockNetworkAccessManager* m_mockNetworkManager = nullptr; // Created when entering replay mode
    QString m_replaySessionTimestamp;                         // Timestamp when replay mode was entered

    // Market depth queue for handling concurrent stream limit
    std::deque<PendingMarketDepthRequest> m_marketDepthQueue;

#ifdef GUI_ENABLED
    GUIAuthHandler* m_authHandler = nullptr; // GUI authentication handler
#else
    TUIAuthHandler* m_authHandler = nullptr; // TUI authentication handler
#endif
};
