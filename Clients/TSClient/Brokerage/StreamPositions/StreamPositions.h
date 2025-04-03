#pragma once

#include "../../Clients/TSClient/Stream/Stream.h"
#include "Position.h"

class StreamPositions : public Stream
{
    Q_OBJECT
public:
    // TODO make it multiple accounts
    explicit StreamPositions(QString &account, QObject *parent = nullptr);

    ~StreamPositions();
    StreamPositions(const StreamPositions&) = delete;
    StreamPositions& operator=(const StreamPositions&) = delete;

signals :
    void receivedNewPosition(QString account, Position position);

private:
    QString account;
    virtual bool processJsonObject(const QJsonObject& jsonObj);
};
