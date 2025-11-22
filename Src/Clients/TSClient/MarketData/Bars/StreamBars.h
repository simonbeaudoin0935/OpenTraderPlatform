#pragma once

#include <QObject>

#include "Stream.h"
#include "Bar.h"
#include "Ticker.h"


class StreamBars : public Stream {
    Q_OBJECT

public:
    explicit StreamBars(const Ticker &symbol, QObject *parent = nullptr);
    ~StreamBars();
    StreamBars(const StreamBars&) = delete;
    StreamBars& operator=(const StreamBars&) = delete;

signals:
    void receivedNewBar(Ticker symbol, Bar bar);

private:
    virtual bool processJsonObject(const QJsonObject& jsonObj);
};
