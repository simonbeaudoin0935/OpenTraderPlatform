#include "OrdersReceiver.h"
#include "TSClient.h"
#include "Logging.h"

#define LOGGING_CATEGORY OrdersReceiverLog
Q_LOGGING_CATEGORY(OrdersReceiverLog, "OrdersReceiver");

OrdersReceiver::OrdersReceiver(const QString &p_account, QObject *p_parent) :
    QObject(p_parent),
    m_account(p_account)
{
    this->setObjectName("OrdersReceiver");
    
    DEBUG <<  "Starting Orders stream for account " << p_account;
    
    createOrdersStream();
}

void OrdersReceiver::createOrdersStream()
{
    m_stream = TSClient::getInstance()->openStreamOrders(m_account);
    Q_CHECK_PTR(m_stream);

    connect(m_stream, &StreamOrders::newOrderReceived, this, &OrdersReceiver::onReceivedNewOrder);
    connect(m_stream, &StreamOrders::endSnapshotReceived, this,
        [this](){
            INFO << "Received EndSnapshot for Orders stream";
        });

    m_stream->future().then(this,
        [this](std::optional<QString> error){
            // We get here when the stream is gracefully closed by the receiving side.
            // This should never happen for the orders stream, its supposed to operate during
            // all the program execution.

            if (error.has_value()) {
                CRITICAL << "Orders stream for account" << m_account
                         << "finished with error:" << error.value();
            } else {
                DEBUG << "Orders stream for account" << m_account << "finished without error";
            }

            QTimer::singleShot(300, this, &OrdersReceiver::createOrdersStream);
        }
    );
}

void OrdersReceiver::onReceivedNewOrder(Order order)
{
    qCDebug(OrdersReceiverLog).noquote() << "New order for account (" << m_account << ") : OrderID=" << order.getOrderID();

    emit receivedNewOrder(m_account, order);
}
