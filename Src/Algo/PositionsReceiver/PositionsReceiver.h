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
     * Thread context: Emitted from MainAlgo worker thread
     * Data flow: StreamPositions (TSClient thread) → PositionsReceiver slot (MainAlgo thread, queued) → this signal
     */
    void receivedNewPosition(QString account, Position position);

    /**
     * @brief Signal emitted when a position is deleted/closed
     *
     * Thread context: Emitted from MainAlgo worker thread
     */
    void positionDeleted(QString account, QString positionID);

    /**
     * @brief Signal emitted when positions are loaded from database at startup
     *
     * Thread context: Emitted from MainAlgo worker thread
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

    // Track last structural state per position to suppress pure mark-to-market DB writes.
    // A "structural" change is a change in quantity or average price (open/add/reduce/close).
    struct PositionStructure
    {
        double quantity = 0.0;
        double averagePrice = 0.0;
    };
    QMap<QString, PositionStructure> m_lastKnownStructure; // positionID → last written state

    void createPositionsStream();
};
