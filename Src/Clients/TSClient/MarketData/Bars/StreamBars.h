#pragma once

#include "Stream.h"
#include "Bar.h"

class StreamBars final : public Stream
{
    Q_OBJECT

  public:
    explicit StreamBars(const QString& symbol, QNetworkReply* reply, QObject* parent = nullptr);
    ~StreamBars(){};
    StreamBars(const StreamBars&) = delete;
    StreamBars& operator=(const StreamBars&) = delete;

    QString getSymbol() const
    {
        return m_symbol;
    }
  signals:
    void newBarReceived(Bar bar);

  private:
    void processJsonObject(const QJsonObject& jsonObj) override;
    QString m_symbol;
};
