#include "OrdersReceiver.h"
#include "TSClient.h"
#include "Logging.h"
#include "Settings.h"
#include "MainApp.h"
#include "Assume.h"

#include <QDir>
#include <QTimer>

#define LOGGING_CATEGORY OrdersReceiverLog
Q_LOGGING_CATEGORY(OrdersReceiverLog, "OrdersReceiver");

OrdersReceiver::OrdersReceiver(const QString& p_account, QObject* p_parent)
    : StreamReceiver(p_parent), m_account(p_account)
{
    this->setObjectName("OrdersReceiver");

    DEBUG << "Starting Orders stream for account " << p_account;

    // Get the singleton database instance
    m_database = OrdersDatabase::getInstance();
    Q_CHECK_PTR(m_database);

    if (!m_database->isOpen())
    {
        CRITICAL << "Failed to open orders database";
        // Failed to open orders database
        Q_UNREACHABLE();
    }

    // Load existing orders from database
    auto existingOrders = m_database->loadAllOrders();
    DEBUG << "Loaded" << existingOrders.size() << "orders from database";

    // Store the loaded latencies for restoring when we receive them in the snapshot
    for (auto it = existingOrders.constBegin(); it != existingOrders.constEnd(); ++it)
    {
        const QString& orderId = it.key();
        const auto& [order, latencyMs] = it.value();
        m_loadedLatencies[orderId] = latencyMs;
    }

    createOrdersStream();
}

OrdersReceiver::~OrdersReceiver()
{
    if (m_stream != nullptr)
    {
        TSClient::getInstance()->closeStream(m_stream);
    }
}

void OrdersReceiver::createOrdersStream()
{
    m_stream = TSClient::getInstance()->openStreamOrders(m_account);
    Q_CHECK_PTR(m_stream);

    connect(m_stream, &StreamOrders::newOrderReceived, this, &OrdersReceiver::onReceivedNewOrder);
    connect(m_stream, &StreamOrders::endSnapshotReceived, this, &OrdersReceiver::onEndSnapshotReceived);

    connect(m_stream,
            &Stream::streamClosed,
            this,
            [this](Stream::StreamError reason, QString message)
            {
                if (reason != Stream::StreamError::Closed)
                {
                    CRITICAL << "Orders stream for account" << m_account << "finished with error:" << message;
                    m_stream = nullptr;
                    QTimer::singleShot(300, this, &OrdersReceiver::createOrdersStream);
                }
                else
                {
                    DEBUG << "Orders stream for account" << m_account << "intentionally closed";
                }
            });
}

void OrdersReceiver::onReceivedNewOrder(Order order)
{
    QDateTime currentTime = MainApp::getCurrentAppTime();
    QString orderId = order.getOrderID();

    qCDebug(OrdersReceiverLog).noquote() << "New order for account (" << m_account << ") : OrderID=" << orderId;

    // If we haven't received the end snapshot yet, this is part of the initial snapshot
    if (!m_receivedEndSnapshot)
    {
        m_snapshotOrders[orderId] = currentTime;
    }

    // Check if this order was loaded from the database
    bool wasLoadedFromDB = m_loadedLatencies.contains(orderId);

    // Check if this is a new order or an update
    bool orderExistsInDB = m_database->orderExists(orderId);

    if (!orderExistsInDB)
    {
        // New order - compute ack latency and store in database
        qint64 latencyMs = order.getOpenedDateTime().msecsTo(currentTime);
        order.setLatencyMs(latencyMs);
        m_database->insertOrder(order, latencyMs);
        DEBUG << "Stored new order" << orderId << "in database with latency" << latencyMs << "ms";
    }
    else
    {
        // Existing order - restore latency from database if available
        if (wasLoadedFromDB && m_loadedLatencies[orderId].has_value())
        {
            order.setLatencyMs(m_loadedLatencies[orderId].value());
        }

        // If the order was just filled and doesn't have a latency yet, compute fill latency
        std::optional<qint64> newLatencyMs;
        if (order.getOrderStatus() == Order::Status::FLL && !order.getLatencyMs().has_value())
        {
            qint64 latencyMs = order.getOpenedDateTime().msecsTo(currentTime);
            order.setLatencyMs(latencyMs);
            newLatencyMs = latencyMs;
            INFO << "Order" << orderId << "was filled with latency" << latencyMs << "ms";
        }

        // Update the order in the database
        m_database->updateOrder(order, newLatencyMs);
        DEBUG << "Updated order" << orderId << "in database";
    }

    emit receivedNewOrder(m_account, order);
}

void OrdersReceiver::onEndSnapshotReceived()
{
    DEBUG << "Received EndSnapshot for Orders stream";
    m_receivedEndSnapshot = true;

    // Validate that all snapshot orders exist in our database
    validateSnapshotOrders();

    // Clear snapshot tracking
    m_snapshotOrders.clear();
}

void OrdersReceiver::validateSnapshotOrders()
{
    DEBUG << "Validating" << m_snapshotOrders.size() << "snapshot orders against database";

    for (auto it = m_snapshotOrders.constBegin(); it != m_snapshotOrders.constEnd(); ++it)
    {
        const QString& orderId = it.key();

        if (!m_database->orderExists(orderId))
        {
            WARNING << "Snapshot order" << orderId << "was not found in database - this is unexpected";
        }
    }

    DEBUG << "Snapshot validation complete";
}

void OrdersReceiver::stopStream(const QString& p_account)
{
    Q_UNUSED(p_account);
    if (m_stream != nullptr)
    {
        TSClient::getInstance()->closeStream(m_stream);
        m_stream = nullptr;
        DEBUG << "Orders stream stopped for account" << m_account;
    }
}

void OrdersReceiver::stopStream(const char* p_account)
{
    stopStream(QString(p_account));
}
