#pragma once

#include <QObject>

#include "Stream.h"
#include "Bar.h"


class StreamBars final : public Stream {
    Q_OBJECT

public:
    explicit StreamBars(const QString &symbol, QObject *parent = nullptr);
    ~StreamBars();
    StreamBars(const StreamBars&) = delete;
    StreamBars& operator=(const StreamBars&) = delete;

signals:
    void receivedNewBar(QString symbol, Bar bar);

private:
    bool processJsonObject(const QJsonObject& jsonObj) override;
    QString m_symbol;
};
