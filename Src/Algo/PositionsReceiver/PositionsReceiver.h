#pragma once

#include <QObject>
#include <QFutureWatcher>

#include "StreamPositions.h"

Q_DECLARE_LOGGING_CATEGORY(PositionsReceiverLog)

class PositionsReceiver : public QObject
{
    Q_OBJECT
public:
    explicit PositionsReceiver(const QString &account, QObject *parent = nullptr);
    ~PositionsReceiver();
    void stopStream(const QString &account);
    void stopStream(const char* account);

signals:
    void receivedNewPosition(QString account, Position position);
    void positionDeleted(QString account, QString positionID);

private slots:
    void onReceivedNewPosition(Position position);
    void onPositionDeleted(QString positionID);

private:
    StreamPositions* m_stream = nullptr;
    QString m_account;
};
