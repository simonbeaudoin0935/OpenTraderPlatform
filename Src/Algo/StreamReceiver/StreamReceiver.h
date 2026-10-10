#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

#include "Stream.h"

/**
 * @brief Base class for all stream receivers (bars, market depth, orders, positions)
 *
 * Provides common interface for stream heartbeat management.
 * Derived classes must implement getStreamBase() to return their specific stream.
 */
class StreamReceiver : public QObject
{
    Q_OBJECT
  public:
    explicit StreamReceiver(QObject* parent = nullptr);

    /**
     * @brief Pause the stream's heartbeat timer
     *
     * Useful when replay is paused or reaches end of data to prevent timeout.
     */
    void pauseHeartbeat()
    {
        if (auto stream = getStreamBase())
        {
            stream->pauseHeartbeat();
        }
    }

    /**
     * @brief Resume the stream's heartbeat timer
     *
     * Call this before resuming replay to re-enable timeout detection.
     */
    void resumeHeartbeat()
    {
        if (auto stream = getStreamBase())
        {
            stream->resumeHeartbeat();
        }
    }

    /**
     * @brief Check if the stream is currently active
     */
    [[nodiscard]] bool hasActiveStream() const
    {
        return getStreamBase() != nullptr;
    }

  protected:
    void beginRecovery(const QString& p_feed, const QString& p_account);
    void completeRecovery();
    void stopRecovery();
    QTimer m_streamRetryTimer;
    bool m_streamStopped = false;
    /**
     * @brief Get the underlying Stream object
     *
     * Derived classes must implement this to return their specific stream type.
     */
    [[nodiscard]] virtual QPointer<Stream> getStreamBase() const = 0;

  private:
    QTimer m_recoveryTimer;
    QString m_recoveryFeed;
    QString m_recoveryAccount;
    bool m_recovering = false;
};
