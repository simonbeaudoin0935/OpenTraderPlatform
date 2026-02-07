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
    void receivedNewPosition(QString account, Position position);
    void positionDeleted(QString account, QString positionID);
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
