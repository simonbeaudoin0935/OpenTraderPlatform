#pragma once

#include <optional>

#include <QObject>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QTimer>
#include <QNetworkReply>
#include <QPromise>
#include <QFuture>

Q_DECLARE_LOGGING_CATEGORY(StreamLog)


class Stream : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Stream error types that can be received from the TradeStation API.
     *
     * MarketData streams (Bars, MarketDepthQuote) use: BadRequest, DualLogon, GoAway, InternalServerError
     * Brokerage streams (Orders, Positions) use: Forbidden, InternalServerError, ServiceUnavailable, GatewayTimeout, Failed
     */
    enum class StreamError : quint8
    {
        // Common
        Timeout, ///< Heartbeat timeout - no data received. Recoverable.

        // MarketData category errors
        BadRequest,          ///< Our code error - malformed request. Assert.
        DualLogon,           ///< Server error - another session logged in. Recoverable.
        GoAway,              ///< Normal server shutdown. Recoverable.
        InternalServerError, ///< Server error. Recoverable.

        // Brokerage category errors
        Forbidden,          ///< Our code error - bad account/permissions. Assert.
        ServiceUnavailable, ///< Our code error - bad request format. Assert.
        GatewayTimeout,     ///< Unknown cause. Recoverable.
        Failed,             ///< Generic server failure. Recoverable.
    };
    Q_ENUM(StreamError)

    Stream(QNetworkReply* reply, QObject* parent);
    ~Stream();
    Q_DISABLE_COPY_MOVE(Stream)

    /**
     * @brief Returns a future that resolves when the stream ends.
     *
     * The future carries an optional pair of (StreamError, description):
     * - **Has value**: The stream ended due to an error (network timeout, server error, etc.).
     *   The StreamError enum identifies the error type, and the QString provides a human-readable description.
     * - **No value** (std::nullopt): The stream was intentionally closed via TSClient::closeStream().
     *
     * Consumers should use this to detect stream closure and decide whether to reconnect:
     * @code
     * stream->future().then(this, [](std::optional<std::pair<Stream::StreamError, QString>> error) {
     *     if (error.has_value()) {
     *         // Stream ended with error - consider reconnecting
     *         auto [errorType, message] = error.value();
     *     } else {
     *         // Stream was intentionally closed - no action needed
     *     }
     * });
     * @endcode
     */
    [[nodiscard]] QFuture<std::optional<std::pair<StreamError, QString>>> future() const
    {
        return m_future;
    }

    static size_t getNumberOpenStream()
    {
        return s_numberOfStream;
    }

    // Called by MainApp before starting shutdown sequence
    static void setShuttingDown(bool shuttingDown)
    {
        s_isShuttingDown = shuttingDown;
    }

    /**
     * @brief Pause the heartbeat timer (e.g., when replay is paused)
     * Thread-safe: uses blocking queued invocation to stop timer on the correct thread
     */
    void pauseHeartbeat()
    {
        m_heartbeatPaused = true;
        // Timer must be stopped from the thread where it was created
        if (QThread::currentThread() == this->thread())
        {
            m_heartbeatTimer.stop();
        }
        else
        {
            QMetaObject::invokeMethod(&m_heartbeatTimer, &QTimer::stop, Qt::BlockingQueuedConnection);
        }
    }

    /**
     * @brief Resume the heartbeat timer (e.g., when replay resumes)
     * Thread-safe: uses blocking queued invocation to start timer on the correct thread
     */
    void resumeHeartbeat()
    {
        m_heartbeatPaused = false;
        // Timer must be started from the thread where it was created
        if (QThread::currentThread() == this->thread())
        {
            m_heartbeatTimer.start(m_heartbeatTimeoutMS);
        }
        else
        {
            QMetaObject::invokeMethod(
                &m_heartbeatTimer,
                [this]() { m_heartbeatTimer.start(m_heartbeatTimeoutMS); },
                Qt::BlockingQueuedConnection);
        }
    }

  signals:
    void newAmountOfDataReceived(size_t bytes);
    void receivedNewRawData(const QByteArray& rawData);

    /**
     * @brief Emitted when the server sends an EndSnapshot status (Brokerage streams only).
     * Indicates the initial data snapshot is complete and subsequent messages are live updates.
     */
    void endSnapshotReceived();

  public slots:
    void onReplyReadyRead();
    void onReplyFinished();

  protected:
    // Each derived class must implement how to process a json object (data only, no error handling)
    virtual void processJsonObject(const QJsonObject& doc) = 0;

    /**
     * @brief Detect and handle error/status messages in the JSON object.
     * Implemented by intermediate classes (StreamMarketData, StreamBrokerage) to handle
     * category-specific error formats.
     * @return true if the JSON object was an error/status message and was handled, false if it's normal data
     */
    virtual bool handleErrorOrStatus(const QJsonObject& jsonObj) = 0;

    bool m_receivedTimeoutError = false;

    QString m_jsonErrorString;

    /// Set by intermediate classes when a server error is detected in the stream JSON data.
    /// Used by onReplyFinished() to determine the error type for the promise result.
    std::optional<StreamError> m_streamError;

  private slots:
    void onHeartbeatTimerTimeout();

  private:
    QPromise<std::optional<std::pair<StreamError, QString>>> m_promise;
    QFuture<std::optional<std::pair<StreamError, QString>>> m_future;

    QByteArray m_accumulatedData;
    void processRawData(const QByteArray& rawData);
    void processJsonDoc(const QJsonDocument& doc);

    // This is the QNetworkReply that this stream gets attributed to when issuing the http->get request
    QNetworkReply* m_networkReply = nullptr;

    const size_t m_heartbeatTimeoutMS = 10000;
    QTimer m_heartbeatTimer;
    bool m_heartbeatPaused = false; // When true, don't restart timer on data reception

    static size_t s_numberOfStream;
    static bool s_isShuttingDown;
};
