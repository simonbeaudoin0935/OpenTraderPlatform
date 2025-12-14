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
        [this](){
            // We get here when the stream is gracefully closed by the receiving side.
            // This should never happen for the orders stream, its supposed to operate during
            // all the program execution.
            CRITICAL << "Not supposed to voluntarly close the orders stream with TSClient::closeStream()";
            Q_ASSERT(false);
        }
    ).onFailed(this,
        [this](const std::exception& e){

            CRITICAL << "Orders Receiver future failed for" << m_account
                     << "- Exception:" << QString::fromStdString(e.what());

            // Since we are in the failed path, it means the stream on the other end
            // will have called deleteLater() on itself after throwing an exception at us.
            // Its safe to then just re-execute this function, since we don't have to worry amout
            // freeing the current stream variable. 
            createOrdersStream();
        }
    );
}

void OrdersReceiver::onReceivedNewOrder(Order order)
{
    qCDebug(OrdersReceiverLog).noquote() << "New order for account (" << m_account << ") : OrderID=" << order.getOrderID();

    emit receivedNewOrder(m_account, order);
}
