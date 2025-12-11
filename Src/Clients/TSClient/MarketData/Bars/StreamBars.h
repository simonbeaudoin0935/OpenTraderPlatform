#pragma once

#include "Stream.h"
#include "Bar.h"

class StreamBars final : public Stream
{
    Q_OBJECT

public:
    explicit StreamBars(const QString &symbol, QNetworkReply * reply, QObject *parent = nullptr);
    ~StreamBars() {};
    StreamBars(const StreamBars&) = delete;
    StreamBars& operator=(const StreamBars&) = delete;

    QFuture<Bar> future() const { return m_promise.future(); }

private:
    void processJsonObject(const QJsonObject& jsonObj) override;
    QString m_symbol;
    QPromise<Bar> m_promise;
};
