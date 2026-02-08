#pragma once

#include <optional>

#include <QObject>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QThread>
#include <QTimer>
#include <QNetworkReply>

Q_DECLARE_LOGGING_CATEGORY(StreamLog)


class Stream : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Stream closure reasons.
     *
     * MarketData streams (Bars, MarketDepthQuote) use: BadRequest, DualLogon, GoAway, InternalServerError
     * Brokerage streams (Orders, Positions) use: Forbidden, InternalServerError, ServiceUnavailable, GatewayTimeout, Failed
     * All streams use: Timeout, Closed
     */
    enum class StreamError : quint8
    {
        // Common
        Closed,  ///< Intentional close via TSClient::closeStream(). Not an error.
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
    /**
     * @brief Signal emitted when new data is received from the network
     * Thread context: Emitted from TSClient worker thread
     */
    void newAmountOfDataReceived(size_t bytes);
    
    /**
     * @brief Signal emitted when raw data is received (before JSON parsing)
     * Thread context: Emitted from TSClient worker thread
     */
    void receivedNewRawData(const QByteArray& rawData);

    /**
     * @brief Emitted when the server sends an EndSnapshot status (Brokerage streams only).
     * Indicates the initial data snapshot is complete and subsequent messages are live updates.
     * 
     * Thread context: Emitted from TSClient worker thread
     */
    void endSnapshotReceived();

    /**
     * @brief Emitted just before the stream is destroyed, indicating why it closed.
     *
     * - StreamError::Closed: Intentional close via TSClient::closeStream() — no action needed.
     * - Any other value: The stream ended due to an error — consumer should reconnect.
     * 
     * Thread context: Emitted from TSClient worker thread
     */
    void streamClosed(Stream::StreamError reason, QString description);

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
    /// Used by onReplyFinished() to determine the error type for the streamClosed signal.
    std::optional<StreamError> m_streamError;

  private slots:
    void onHeartbeatTimerTimeout();

  private:
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
