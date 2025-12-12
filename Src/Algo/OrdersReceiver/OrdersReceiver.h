#pragma once

#include <QObject>
#include <QFutureWatcher>

#include "StreamOrders.h"

Q_DECLARE_LOGGING_CATEGORY(OrdersReceiverLog)

class OrdersReceiver : public QObject
{
    Q_OBJECT
public:
    explicit OrdersReceiver(const QString &p_account, QObject *p_parent = nullptr);

    void stopStream(const QString &p_account);
    void stopStream(const char* p_account);

signals:
    void receivedNewOrder(QString account, Order order);

private slots:
    void onReceivedNewOrder(QString account, Order order);

private:
    StreamOrders* m_stream = nullptr;
    QString m_account;
};
