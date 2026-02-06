#pragma once

#include "StreamMarketData.h"
#include "Bar.h"

class StreamBars final : public StreamMarketData
{
    Q_OBJECT

  public:
    explicit StreamBars(const QString& symbol, QNetworkReply* reply, QObject* parent = nullptr);
    ~StreamBars() = default;
    Q_DISABLE_COPY_MOVE(StreamBars)

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
