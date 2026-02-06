#pragma once

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
    Stream(QNetworkReply* reply, QObject* parent);
    ~Stream();
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

    QFuture<std::optional<QString>> future() const
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

  public slots:
    void onReplyReadyRead();
    void onReplyFinished();

  protected:
    // Each derived class must implement how to process a json object
    virtual void processJsonObject(const QJsonObject& doc) = 0;

    bool m_receivedTimeoutError = false;

    QString m_jsonErrorString;

  private slots:
    void onHeartbeatTimerTimeout();

  private:
    QPromise<std::optional<QString>> m_promise;
    QFuture<std::optional<QString>> m_future;

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
