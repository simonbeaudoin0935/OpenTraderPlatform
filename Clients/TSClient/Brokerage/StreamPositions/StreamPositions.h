#pragma once

#include "../../Clients/TSClient/Stream/Stream.h"
#include "Position.h"

class StreamPositions : public Stream
{
public:
    explicit StreamPositions(QObject *parent = nullptr);

    ~StreamPositions();
    StreamPositions(const StreamPositions&) = delete;
    StreamPositions& operator=(const StreamPositions&) = delete;

signals :
    void receivedNewPosition(Position position);

private:
    virtual bool processJsonObject(const QJsonObject& jsonObj);
};
