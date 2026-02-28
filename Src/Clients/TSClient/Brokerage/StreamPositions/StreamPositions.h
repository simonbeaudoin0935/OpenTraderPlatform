#pragma once

#include "StreamBrokerage.h"
#include "Position.h"

class StreamPositions final : public StreamBrokerage
{
    Q_OBJECT

  public:
    explicit StreamPositions(const QString& accountID, QNetworkReply* reply, QObject* parent = nullptr);
    ~StreamPositions();
    Q_DISABLE_COPY_MOVE(StreamPositions)

    QString getAccountID()
    {
        return m_accountID;
    };

    /**
     * @brief Get the number of currently open position streams
     * @return Current count (should always be 0 or 1)
     * @note There should only ever be one positions stream per application instance
     */
    static size_t getNumberOfPositionStreams()
    {
        return s_numberOfPositionStreams;
    }

  signals:
    /**
     * @brief Signal emitted when a new position is received from the stream
     * Thread context: Emitted from TSClient worker thread
     */
    void newPositionReceived(Position position);

    /**
     * @brief Signal emitted when a position is deleted/closed
     * Thread context: Emitted from TSClient worker thread
     */
    void positionDeleted(QString positionID);

  private:
    void processJsonObject(const QJsonObject& jsonObj) override;

    QString m_accountID;

    /// Counter for position streams (should never exceed 1)
    static size_t s_numberOfPositionStreams;
};
