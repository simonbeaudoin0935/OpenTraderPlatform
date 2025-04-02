#pragma once

#include <QObject>

#include "../Stream.h"
#include "Bar.h"


class StreamBars : public Stream {
    Q_OBJECT

public:
    explicit StreamBars();
    ~StreamBars();
    StreamBars(const StreamBars&) = delete;
    StreamBars& operator=(const StreamBars&) = delete;

signals:
    void receivedNewBar(Bar bar);

private:
    virtual bool processJsonObject(const QJsonObject& jsonObj);
};
