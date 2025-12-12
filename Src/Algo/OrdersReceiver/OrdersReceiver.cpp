#include "OrdersReceiver.h"
#include "TSClient.h"

#include <QFutureWatcher>

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
            CRITICAL << "Orders Receiver future finished for account" << m_account;
        }
    ).onFailed(this, [this](Stream::TimeoutException ex){
        WARNING << "Orders Receiver future failed for" << m_account << "- Exception:" << ex.what();
    }).onFailed(this, [this](Stream::BadRequestException ex){
        WARNING << "Orders Receiver future failed for" << m_account << "- Exception:" << ex.what();
    }).onFailed(this, [this](Stream::DualLogonException ex){
        WARNING << "Orders Receiver future failed for" << m_account << "- Exception:" << ex.what();
    }).onFailed(this, [this](Stream::GoAwayException ex){
        WARNING << "Orders Receiver future failed for" << m_account << "- Exception:" << ex.what();
    }).onFailed(this, [this](Stream::InternalServerErrorException ex){
        WARNING << "Orders Receiver future failed for" << m_account << "- Exception:" << ex.what();
    }).onFailed(this, [this](Stream::InvalidSymbolException ex){
        WARNING << "Orders Receiver future failed for" << m_account << "- Exception:" << ex.what();
    });
}

void OrdersReceiver::onReceivedNewOrder(Order order)
{
    qCDebug(OrdersReceiverLog).noquote() << "New order for account (" << m_account << ") : OrderID=" << order.getOrderID();
    emit receivedNewOrder(m_account, order);
}
