#include "OrdersReceiver.h"
#include "TSClient.h"
#include "Logging.h"
#include "Settings.h"

#include <QDir>
#include <QTimer>

#define LOGGING_CATEGORY OrdersReceiverLog
Q_LOGGING_CATEGORY(OrdersReceiverLog, "OrdersReceiver");

OrdersReceiver::OrdersReceiver(const QString &p_account, QObject *p_parent) :
    QObject(p_parent),
    m_account(p_account)
{
    this->setObjectName("OrdersReceiver");
    
    DEBUG <<  "Starting Orders stream for account " << p_account;
    
    // Initialize the orders database
    QString cacheDir = getCacheLocation();
    QString dbPath = cacheDir + "/orders.db";
    m_database = new OrdersDatabase(dbPath, this);
    Q_CHECK_PTR(m_database);
    
    if (!m_database->isOpen()) {
        CRITICAL << "Failed to open orders database";
        Q_ASSERT_X(false, "OrdersReceiver::OrdersReceiver", "Failed to open orders database");
    }
    
    // Load existing orders from database
    auto existingOrders = m_database->loadAllOrders();
    INFO << "Loaded" << existingOrders.size() << "orders from database";
    
    createOrdersStream();
}

void OrdersReceiver::createOrdersStream()
{
    m_stream = TSClient::getInstance()->openStreamOrders(m_account);
    Q_CHECK_PTR(m_stream);

    connect(m_stream, &StreamOrders::newOrderReceived, this, &OrdersReceiver::onReceivedNewOrder);
    connect(m_stream, &StreamOrders::endSnapshotReceived, this, &OrdersReceiver::onEndSnapshotReceived);

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
    QDateTime currentTime = QDateTime::currentDateTimeUtc();
    QString orderId = order.getOrderID();
    
    qCDebug(OrdersReceiverLog).noquote() << "New order for account (" << m_account << ") : OrderID=" << orderId;
    
    // If we haven't received the end snapshot yet, this is part of the initial snapshot
    if (!m_receivedEndSnapshot) {
        m_snapshotOrders[orderId] = currentTime;
    }
    
    // Check if this is a new order or an update
    bool orderExists = m_database->orderExists(orderId);
    
    if (!orderExists) {
        // New order - set received time and store in database
        order.setReceivedTime(currentTime);
        m_database->insertOrder(order, currentTime);
        DEBUG << "Stored new order" << orderId << "in database";
    } else {
        // Existing order - check if it was just filled
        std::optional<QDateTime> filledTime;
        if (order.getOrderStatus() == OrderStatus::FLL && !order.getFilledTime().has_value()) {
            filledTime = currentTime;
            order.setFilledTime(currentTime);
            INFO << "Order" << orderId << "was filled at" << currentTime.toString(Qt::ISODate);
        }
        
        // Update the order in the database
        m_database->updateOrder(order, filledTime);
        DEBUG << "Updated order" << orderId << "in database";
    }

    emit receivedNewOrder(m_account, order);
}

void OrdersReceiver::onEndSnapshotReceived()
{
    INFO << "Received EndSnapshot for Orders stream";
    m_receivedEndSnapshot = true;
    
    // Validate that all snapshot orders exist in our database
    validateSnapshotOrders();
    
    // Clear snapshot tracking
    m_snapshotOrders.clear();
}

void OrdersReceiver::validateSnapshotOrders()
{
    INFO << "Validating" << m_snapshotOrders.size() << "snapshot orders against database";
    
    for (auto it = m_snapshotOrders.constBegin(); it != m_snapshotOrders.constEnd(); ++it) {
        const QString& orderId = it.key();
        
        if (!m_database->orderExists(orderId)) {
            WARNING << "Snapshot order" << orderId << "was not found in database - this is unexpected";
        }
    }
    
    INFO << "Snapshot validation complete";
}
