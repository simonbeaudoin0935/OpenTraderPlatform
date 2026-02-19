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
#include "Assume.h"
#include "MainApp.h"
#include "TSClient.h"

#define LOGGING_CATEGORY OrdersDatabaseLog
Q_LOGGING_CATEGORY(OrdersDatabaseLog, "OrdersDatabase");

// Static singleton instance
OrdersDatabase* OrdersDatabase::s_instance = nullptr;

namespace
{
    /**
 * @brief Determines the database path based on current trading mode
 *
 * Database structure:
 * - Live:       ~/.cache/L2Trader/Orders/Live/Orders.db
 * - Simulation: ~/.cache/L2Trader/Orders/Simulation/Orders.db
 * - Replay:     ~/.cache/L2Trader/Orders/Replay/Orders_YYYY-MM-DD_HHMMSS.db
 *
 * @return Full path to the orders database file
 */
    QString determineDatabasePath()
    {
        QString baseDir = getCacheLocation();
        baseDir += "/Orders/";

        // Check TSClient mode first - if Replay, use replay path
        TSClient* client = TSClient::getInstance();
        if (client && client->getMode() == TSClient::Mode::Replay)
        {
            QString timestamp = client->getReplaySessionTimestamp();
            ASSUME_TRUE(!timestamp.isEmpty());

            QString replayDir = baseDir + "Replay/";
            QDir().mkpath(replayDir);
            return replayDir + "Orders_" + timestamp + ".db";
        }

        // Otherwise check TradingMode (Live vs Sim)
        TradingMode tradingMode = MainApp::getTradingMode();
        if (tradingMode == TradingMode::Sim)
        {
            QString simDir = baseDir + "Simulation/";
            QDir().mkpath(simDir);
            return simDir + "Orders.db";
        }

        // Default to Live
        QString liveDir = baseDir + "Live/";
        QDir().mkpath(liveDir);
        return liveDir + "Orders.db";
    }
} // anonymous namespace

OrdersDatabase* OrdersDatabase::getInstance(QObject* p_parent)
{
    if (s_instance == nullptr)
    {
        QString dbPath = determineDatabasePath();
        s_instance = new OrdersDatabase(dbPath, p_parent);
    }
    return s_instance;
}

void OrdersDatabase::destroyInstance()
{
    if (s_instance != nullptr)
    {
        delete s_instance;
        s_instance = nullptr;
    }
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
    m_db = QSqlDatabase(); // Release the copy before removal (Qt requirement)
    QSqlDatabase::removeDatabase(m_connectionName);
    s_instance = nullptr;
}

void OrdersDatabase::createTable()
{
    QSqlQuery query(m_db);

    if (!query.exec(OrdersDatabaseQueries::CREATE_ORDERS_TABLE))
    {
        CRITICAL << "Failed to create orders table:" << query.lastError().text();
        // Failed to create orders table
        Q_UNREACHABLE();
    }
}

bool OrdersDatabase::insertOrder(const Order& p_order, std::optional<qint64> p_latencyMs)
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

    // Latency in milliseconds
    if (p_latencyMs.has_value())
    {
        query.addBindValue(p_latencyMs.value());
    }
    else
    {
        query.addBindValue(QVariant()); // NULL value for SQL
    }

    // Store a minimal order representation in JSON
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

bool OrdersDatabase::updateOrder(const Order& p_order, std::optional<qint64> p_latencyMs)
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

    if (p_latencyMs.has_value())
    {
        query.addBindValue(p_latencyMs.value());
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

QMap<QString, std::tuple<Order, std::optional<qint64>>> OrdersDatabase::loadAllOrders() const
{
    QMap<QString, std::tuple<Order, std::optional<qint64>>> orders;

    QSqlQuery query(m_db);
    if (!query.exec(OrdersDatabaseQueries::SELECT_ALL_ORDERS))
    {
        WARNING << "Failed to load orders from database:" << query.lastError().text();
        return orders;
    }

    while (query.next())
    {
        QString orderId = query.value(0).toString();
        std::optional<qint64> latencyMs;
        if (!query.value(1).isNull())
        {
            latencyMs = query.value(1).toLongLong();
        }
        QString statusStr = query.value(2).toString();
        QString orderTypeStr = query.value(3).toString();
        QString jsonDataStr = query.value(4).toString();

        // Additional columns
        double filledPrice = query.value(5).toDouble();
        QVariant limitPriceVar = query.value(6);
        QVariant stopPriceVar = query.value(7);
        QString openedDateTimeStr = query.value(8).toString();
        QString closedDateTimeStr = query.value(9).toString();

        // Reconstruct the Order object from JSON with additional fields
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonDataStr.toUtf8());
        if (jsonDoc.isObject())
        {
            QJsonObject jsonObj = jsonDoc.object();
            jsonObj["Status"] = statusStr;
            jsonObj["OrderType"] = orderTypeStr;
            jsonObj["FilledPrice"] = QString::number(filledPrice, 'f', 4);

            if (!limitPriceVar.isNull())
            {
                jsonObj["LimitPrice"] = QString::number(limitPriceVar.toDouble(), 'f', 4);
            }
            if (!stopPriceVar.isNull())
            {
                jsonObj["StopPrice"] = QString::number(stopPriceVar.toDouble(), 'f', 4);
            }
            if (!openedDateTimeStr.isEmpty())
            {
                jsonObj["OpenedDateTime"] = openedDateTimeStr;
            }
            if (!closedDateTimeStr.isEmpty())
            {
                jsonObj["ClosedDateTime"] = closedDateTimeStr;
            }

            Order order(jsonObj);
            orders.insert(orderId, std::make_tuple(order, latencyMs));
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
