#pragma once

#include "StreamBrokerage.h"
#include "Order.h"

class StreamOrders final : public StreamBrokerage
{
    Q_OBJECT

  public:
    // TODO make it multiple accounts
    explicit StreamOrders(const QString& account, QNetworkReply* reply, QObject* parent = nullptr);
    ~StreamOrders();
    Q_DISABLE_COPY_MOVE(StreamOrders)

    QString getAccountID()
    {
        return m_accountID;
    };

    /**
     * @brief Get the number of currently open order streams
     * @return Current count (should always be 0 or 1)
     * @note There should only ever be one orders stream per application instance
     */
    static size_t getNumberOfOrderStreams()
    {
        return s_numberOfOrderStreams;
    }

  signals:
    void newOrderReceived(Order order);

  private:
    void processJsonObject(const QJsonObject& jsonObj) override;

    QString m_accountID;

    /// Counter for order streams (should never exceed 1)
    static size_t s_numberOfOrderStreams;
};
