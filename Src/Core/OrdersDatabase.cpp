#include "OrdersDatabase.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCoreApplication>

#include "Logging.h"
#include "Settings.h"
#include "SQL/OrdersDatabaseQueries.h"

#define LOGGING_CATEGORY OrdersDatabaseLog
Q_LOGGING_CATEGORY(OrdersDatabaseLog, "OrdersDatabase");

// Static singleton instance
OrdersDatabase* OrdersDatabase::s_instance = nullptr;

OrdersDatabase* OrdersDatabase::getInstance(QObject* p_parent)
{
    if (s_instance == nullptr)
    {
        QString cacheDir = getCacheLocation();
        QString dbPath = cacheDir + "/orders.db";
        s_instance = new OrdersDatabase(dbPath, p_parent);
    }
    return s_instance;
}

OrdersDatabase::OrdersDatabase(const QString& p_dbPath, QObject* p_parent)
    : QObject(p_parent), m_dbPath(p_dbPath), m_connectionName("OrdersDB") // Use fixed connection name for singleton
{
    setObjectName("OrdersDatabase");

    // Ensure the directory exists
    QFileInfo fileInfo(p_dbPath);
    QDir dir = fileInfo.dir();
    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            CRITICAL << "Failed to create directory for orders database:" << dir.path();
            return;
        }
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(p_dbPath);

    if (!m_db.open())
    {
        CRITICAL << "Failed to open orders database:" << m_db.lastError().text();
        return;
    }

    createTable();
    INFO << "Orders database opened at" << p_dbPath;
}

OrdersDatabase::~OrdersDatabase()
{
    if (m_db.isOpen())
    {
        m_db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
    s_instance = nullptr;
}

void OrdersDatabase::createTable()
{
    QSqlQuery query(m_db);

    if (!query.exec(OrdersDatabaseQueries::CREATE_ORDERS_TABLE))
    {
        CRITICAL << "Failed to create orders table:" << query.lastError().text();
        Q_ASSERT_X(false, "OrdersDatabase::createTable", "Failed to create orders table");
    }
}

bool OrdersDatabase::insertOrder(const Order& p_order, const QDateTime& p_receivedTime)
{
    QSqlQuery query(m_db);

    query.prepare(OrdersDatabaseQueries::INSERT_ORDER);
    query.addBindValue(p_order.getOrderID());
    query.addBindValue(p_order.getAccountID());
    query.addBindValue(p_order.getSymbol());
    query.addBindValue(p_order.getQuantity());
    query.addBindValue(p_order.getTradeAction());
    query.addBindValue(QtEnum::toString(p_order.getOrderType().type));
    query.addBindValue(QtEnum::toString(p_order.getOrderStatus()));
    query.addBindValue(p_order.getStatusDescription());

    // Handle optional limit and stop prices
    if (p_order.getLimitPrice().has_value())
    {
        query.addBindValue(p_order.getLimitPrice().value());
    }
    else
    {
        query.addBindValue(QVariant()); // NULL value for SQL
    }

    if (p_order.getStopPrice().has_value())
    {
        query.addBindValue(p_order.getStopPrice().value());
    }
    else
    {
        query.addBindValue(QVariant()); // NULL value for SQL
    }

    query.addBindValue(p_order.getFilledPrice());
    query.addBindValue(p_order.getOpenedDateTime().toString(Qt::ISODate));

    // Handle closed datetime (might not be set)
    if (p_order.getClosedDateTime().isValid())
    {
        query.addBindValue(p_order.getClosedDateTime().toString(Qt::ISODate));
    }
    else
    {
        query.addBindValue(QVariant()); // NULL value for SQL
    }

    query.addBindValue(p_receivedTime.toString(Qt::ISODate));

    // Filled time is null for new orders
    query.addBindValue(QVariant()); // NULL value for SQL

    // Store a minimal order representation in JSON
    // Note: We don't need to store the complete order data because orders are
    // received from the stream on every startup. We only store the minimal data
    // needed to identify the order (OrderID, AccountID, Symbol, etc.) along with
    // the timestamps in dedicated columns. The full order data will be restored
    // from the stream when the application restarts.
    QJsonObject jsonObj;
    jsonObj["OrderID"] = p_order.getOrderID();
    jsonObj["AccountID"] = p_order.getAccountID();
    jsonObj["Symbol"] = p_order.getSymbol();
    jsonObj["Quantity"] = p_order.getQuantity();
    jsonObj["TradeAction"] = p_order.getTradeAction();
    query.addBindValue(QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact)));

    if (!query.exec())
    {
        WARNING << "Failed to insert order" << p_order.getOrderID() << ":" << query.lastError().text();
        return false;
    }

    DEBUG << "Inserted order" << p_order.getOrderID() << "into database";
    return true;
}

bool OrdersDatabase::updateOrder(const Order& p_order, const std::optional<QDateTime>& p_filledTime)
{
    QSqlQuery query(m_db);

    query.prepare(OrdersDatabaseQueries::UPDATE_ORDER);
    query.addBindValue(QtEnum::toString(p_order.getOrderStatus()));
    query.addBindValue(p_order.getStatusDescription());
    query.addBindValue(p_order.getFilledPrice());

    // Handle closed datetime (might not be set)
    if (p_order.getClosedDateTime().isValid())
    {
        query.addBindValue(p_order.getClosedDateTime().toString(Qt::ISODate));
    }
    else
    {
        query.addBindValue(QVariant()); // NULL value for SQL
    }

    if (p_filledTime.has_value())
    {
        query.addBindValue(p_filledTime.value().toString(Qt::ISODate));
    }
    else
    {
        query.addBindValue(QVariant()); // NULL value for SQL
    }

    // Update JSON data
    QJsonObject jsonObj;
    jsonObj["OrderID"] = p_order.getOrderID();
    jsonObj["AccountID"] = p_order.getAccountID();
    jsonObj["Symbol"] = p_order.getSymbol();
    jsonObj["Quantity"] = p_order.getQuantity();
    jsonObj["TradeAction"] = p_order.getTradeAction();
    query.addBindValue(QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact)));

    query.addBindValue(p_order.getOrderID());

    if (!query.exec())
    {
        WARNING << "Failed to update order" << p_order.getOrderID() << ":" << query.lastError().text();
        return false;
    }

    DEBUG << "Updated order" << p_order.getOrderID() << "in database";
    return true;
}

bool OrdersDatabase::orderExists(const QString& p_orderID) const
{
    QSqlQuery query(m_db);
    query.prepare(OrdersDatabaseQueries::SELECT_ORDER_EXISTS);
    query.addBindValue(p_orderID);

    if (!query.exec())
    {
        WARNING << "Failed to check if order exists:" << query.lastError().text();
        return false;
    }

    if (query.next())
    {
        return query.value(0).toInt() > 0;
    }

    return false;
}

QMap<QString, std::tuple<Order, QDateTime, std::optional<QDateTime>>> OrdersDatabase::loadAllOrders() const
{
    QMap<QString, std::tuple<Order, QDateTime, std::optional<QDateTime>>> orders;

    QSqlQuery query(m_db);
    if (!query.exec(OrdersDatabaseQueries::SELECT_ALL_ORDERS))
    {
        WARNING << "Failed to load orders from database:" << query.lastError().text();
        return orders;
    }

    while (query.next())
    {
        QString orderId = query.value(0).toString();
        QString receivedTimeStr = query.value(1).toString();
        QString filledTimeStr = query.value(2).toString();
        QString jsonDataStr = query.value(3).toString();

        QDateTime receivedTime = QDateTime::fromString(receivedTimeStr, Qt::ISODate);
        std::optional<QDateTime> filledTime;
        if (!filledTimeStr.isEmpty())
        {
            filledTime = QDateTime::fromString(filledTimeStr, Qt::ISODate);
        }

        // Reconstruct the Order object from JSON
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonDataStr.toUtf8());
        if (jsonDoc.isObject())
        {
            Order order(jsonDoc.object());
            orders.insert(orderId, std::make_tuple(order, receivedTime, filledTime));
        }
    }

    INFO << "Loaded" << orders.size() << "orders from database";
    return orders;
}

bool OrdersDatabase::isOpen() const
{
    return m_db.isOpen();
}

int OrdersDatabase::getOrderCount() const
{
    QSqlQuery query(m_db);
    if (!query.exec(OrdersDatabaseQueries::SELECT_ORDER_COUNT))
    {
        WARNING << "Failed to get order count:" << query.lastError().text();
        return 0;
    }

    if (query.next())
    {
        return query.value(0).toInt();
    }

    return 0;
}

bool OrdersDatabase::clearAllOrders()
{
    QSqlQuery query(m_db);
    if (!query.exec(OrdersDatabaseQueries::DELETE_ALL_ORDERS))
    {
        WARNING << "Failed to clear orders:" << query.lastError().text();
        return false;
    }

    INFO << "Cleared all orders from database";
    return true;
}
