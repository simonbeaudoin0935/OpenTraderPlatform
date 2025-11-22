#pragma once

#include <QObject>

#include "StreamPositions.h"

Q_DECLARE_LOGGING_CATEGORY(PositionsReceiverLog)

class PositionsReceiver : public QObject
{
    Q_OBJECT
public:
    explicit PositionsReceiver(QObject *parent = nullptr);

    void startStream(QString &account);
    void startStream(const char* account);

    void stopStream(QString &account);
    void stopStream(const char* account);

signals:
    void receivedNewPosition(QString account, Position position);

private slots:
    void onReceivedNewPosition(QString account, Position position);
    void onStreamError(Stream::StreamError error, QString errorMessage);

private:
    QMap<QString, StreamPositions*> streams;
};
