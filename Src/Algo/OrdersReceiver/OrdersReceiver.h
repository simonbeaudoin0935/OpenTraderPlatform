#pragma once

#include <QObject>
#include <QFutureWatcher>
#include <QDateTime>
#include <QMap>

#include "StreamOrders.h"
#include "OrdersDatabase.h"

Q_DECLARE_LOGGING_CATEGORY(OrdersReceiverLog)

class OrdersReceiver : public QObject
{
    Q_OBJECT
  public:
    explicit OrdersReceiver(const QString& p_account, QObject* p_parent = nullptr);

    void stopStream(const QString& p_account);
    void stopStream(const char* p_account);

  signals:
    void receivedNewOrder(QString account, Order order);

  private slots:
    void onReceivedNewOrder(Order order);
    void onEndSnapshotReceived();

  private:
    QPointer<StreamOrders> m_stream = nullptr;
    QString m_account;
    OrdersDatabase* m_database = nullptr;
    bool m_receivedEndSnapshot = false;

    // Track orders for the initial snapshot validation
    QMap<QString, QDateTime> m_snapshotOrders; // orderID -> received time

    // Cache of loaded orders with their timestamps
    QMap<QString,
         std::tuple<QDateTime,
                    std::optional<QDateTime>>>
        m_loadedOrderTimes; // orderID -> (receivedTime, filledTime)

    void createOrdersStream();
    void validateSnapshotOrders();
};
