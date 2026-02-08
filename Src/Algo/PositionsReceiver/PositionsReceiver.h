#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QMap>

#include "StreamPositions.h"
#include "StreamReceiver.h"
#include "PositionsDatabase.h"

Q_DECLARE_LOGGING_CATEGORY(PositionsReceiverLog)

class PositionsReceiver : public StreamReceiver
{
    Q_OBJECT
  public:
    explicit PositionsReceiver(const QString& account, QObject* parent = nullptr);
    ~PositionsReceiver();
    void stopStream(const QString& account);
    void stopStream(const char* account);

    QPointer<StreamPositions> getStream() const
    {
        return m_stream;
    }

  signals:
    /**
     * @brief Signal emitted when a new position is received from the stream
     * 
     * Thread Safety (Architecture_Improvements.md point 1.4):
     * - Emitted from: MainAlgo worker thread (this receiver runs on MainAlgo thread)
     * - Received on: MainAlgo thread (same thread, typically uses Qt::DirectConnection)
     * - Thread-safe: Yes (receivers run on MainAlgo thread, receive from TSClient via queued connection)
     * 
     * Data Flow:
     * 1. StreamPositions (TSClient thread) emits newPosition signal
     * 2. PositionsReceiver slot onReceivedNewPosition (MainAlgo thread) receives via Qt::QueuedConnection
     * 3. PositionsReceiver emits receivedNewPosition to MainAlgo (same MainAlgo thread)
     * 
     * @param account Account ID
     * @param position The position data
     */
    void receivedNewPosition(QString account, Position position);
    
    /**
     * @brief Signal emitted when a position is deleted/closed
     * 
     * Thread Safety (Architecture_Improvements.md point 1.4):
     * - Emitted from: MainAlgo worker thread
     * - Received on: MainAlgo thread (same thread)
     * - Thread-safe: Yes (same as receivedNewPosition)
     * 
     * @param account Account ID
     * @param positionID Position identifier
     */
    void positionDeleted(QString account, QString positionID);
    
    /**
     * @brief Signal emitted when positions are loaded from database at startup
     * 
     * Thread Safety (Architecture_Improvements.md point 1.4):
     * - Emitted from: MainAlgo worker thread
     * - Received on: MainAlgo thread (same thread)
     * - Thread-safe: Yes (database operations are async and thread-safe)
     * 
     * @param account Account ID
     * @param positions Map of position ID to Position objects
     */
    void loadedPositionsFromDatabase(QString account, QMap<QString, Position> positions);

  private slots:
    void onReceivedNewPosition(Position position);
    void onPositionDeleted(QString positionID);

  protected:
    [[nodiscard]] QPointer<Stream> getStreamBase() const override
    {
        return QPointer<Stream>(m_stream.data());
    }

  private:
    QPointer<StreamPositions> m_stream = nullptr;
    QString m_account;
    QPointer<PositionsDatabase> m_database = nullptr;
    bool m_receivedEndSnapshot = false;
    QMap<QString, QDateTime> m_positionOpenedTimes; // Track when positions were first opened

    void createPositionsStream();
};
