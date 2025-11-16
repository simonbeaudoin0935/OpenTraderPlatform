#pragma once

#include <QObject>

#include "Stream.h"
#include "Bar.h"


class StreamBars : public Stream {
    Q_OBJECT

public:
    explicit StreamBars(const QString &symbol, QObject *parent = nullptr);
    ~StreamBars();
    StreamBars(const StreamBars&) = delete;
    StreamBars& operator=(const StreamBars&) = delete;

signals:
    void receivedNewBar(QString symbol, Bar bar);

    // This exist so that RecorderLogic can record the received json to replay later
    void receivedNewJson(QString symbol, const QJsonObject& jsonObj);

private:
    virtual bool processJsonObject(const QJsonObject& jsonObj);
    QString symbol;
};
