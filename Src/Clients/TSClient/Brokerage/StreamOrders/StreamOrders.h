#pragma once

#include "Stream.h"
#include "Order.h"

class StreamOrders final : public Stream
{
    Q_OBJECT

public:
    // TODO make it multiple accounts
    explicit StreamOrders(const QString &account, QNetworkReply * reply, QObject *parent = nullptr);
    ~StreamOrders() {};
    StreamOrders(const StreamOrders&) = delete;
    StreamOrders& operator=(const StreamOrders&) = delete;

    QString getAccountID() {return m_accountID; };
    
signals:
    void newOrderReceived(Order order);
    void endSnapshotReceived();

private:
    void processJsonObject(const QJsonObject& jsonObj) override;

    QString m_accountID;
    bool m_receivedEndSnapshot = false;  // Track if we've received the EndSnapshot status
};
