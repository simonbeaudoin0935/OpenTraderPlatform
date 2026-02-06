#pragma once

#include "StreamBrokerage.h"
#include "Order.h"

class StreamOrders final : public StreamBrokerage
{
    Q_OBJECT

  public:
    // TODO make it multiple accounts
    explicit StreamOrders(const QString& account, QNetworkReply* reply, QObject* parent = nullptr);
    ~StreamOrders() = default;
    Q_DISABLE_COPY_MOVE(StreamOrders)

    QString getAccountID()
    {
        return m_accountID;
    };

  signals:
    void newOrderReceived(Order order);

  private:
    void processJsonObject(const QJsonObject& jsonObj) override;

    QString m_accountID;
};
