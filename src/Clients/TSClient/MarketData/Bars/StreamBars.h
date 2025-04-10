#pragma once

#include <QObject>

#include "Stream.h"
#include "Bar.h"


class StreamBars : public Stream {
    Q_OBJECT

public:
    explicit StreamBars(QString &symbol, QObject *parent = nullptr);
    ~StreamBars();
    StreamBars(const StreamBars&) = delete;
    StreamBars& operator=(const StreamBars&) = delete;

signals:
    void receivedNewBar(QString symbol, Bar bar);

private:
    virtual bool processJsonObject(const QJsonObject& jsonObj);
    QString symbol;
};
