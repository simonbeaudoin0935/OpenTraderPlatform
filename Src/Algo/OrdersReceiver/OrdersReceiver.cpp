#include "OrdersReceiver.h"
#include "TSClient.h"

Q_LOGGING_CATEGORY(OrdersReceiverLog, "OrdersReceiver");

#define DEBUG qCDebug(OrdersReceiverLog) << this->objectName()
#define INFO qCInfo(OrdersReceiverLog) << this->objectName()
#define WARNING qCWarning(OrdersReceiverLog) << this->objectName()
#define CRITICAL qCCritical(OrdersReceiverLog) << this->objectName()

OrdersReceiver::OrdersReceiver(const QString &p_account, QObject *p_parent) :
    QObject(p_parent),
    m_account(p_account)
{
    this->setObjectName("OrdersReceiver");
    
    DEBUG <<  "Starting Orders stream for account " << p_account;
    
    m_stream = TSClient::getInstance()->openStreamOrders(p_account);
    Q_CHECK_PTR(m_stream);

    connect(m_stream, &StreamOrders::newOrderReceived, this, &OrdersReceiver::onReceivedNewOrder);

    m_stream->future().then(this,
        [this](){
            CRITICAL << "Orders Receiver future finished";
        }
    ).onFailed(this,
        [this](QException ex){
            Q_UNUSED(ex);

            WARNING << "Orders Receiver future failed for" << m_account
                    << "- Exception:" << m_stream->errorToString();

            CRITICAL << "TODO : deal with this";
        }
    );
}

void OrdersReceiver::onReceivedNewOrder(Order order)
{
    qCDebug(OrdersReceiverLog).noquote() << "New order for account (" << m_account << ") : OrderID=" << order.getOrderID();

    emit receivedNewOrder(m_account, order);
}
