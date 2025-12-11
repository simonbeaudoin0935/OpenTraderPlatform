#pragma once

#include <QObject>

#include "StreamPositions.h"

Q_DECLARE_LOGGING_CATEGORY(PositionsReceiverLog)

class PositionsReceiver : public QObject
{
    Q_OBJECT
public:
    explicit PositionsReceiver(const QString &account, QObject *parent = nullptr);

    void stopStream(const QString &account);
    void stopStream(const char* account);

signals:
    void receivedNewPosition(QString account, Position position);

private slots:
    void onReceivedNewPosition(QString account, Position position);

private:
    StreamPositions* m_stream = nullptr;
    QString m_account;
};
