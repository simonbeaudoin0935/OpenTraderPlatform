#include "OrdersDatabase.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>

#include "Logging.h"

#define LOGGING_CATEGORY OrdersDatabaseLog
Q_LOGGING_CATEGORY(OrdersDatabaseLog, "OrdersDatabase");

OrdersDatabase::OrdersDatabase(const QString& p_dbPath, QObject* p_parent)
    : QObject(p_parent)
    , m_dbPath(p_dbPath)
{
    setObjectName("OrdersDatabase");

    // Ensure the directory exists
    QFileInfo fileInfo(p_dbPath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            CRITICAL << "Failed to create directory for orders database:" << dir.path();
            return;
        }
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", "OrdersDB");
    m_db.setDatabaseName(p_dbPath);

    if (!m_db.open()) {
        CRITICAL << "Failed to open orders database:" << m_db.lastError().text();
        return;
    }

    createTable();
    INFO << "Orders database opened at" << p_dbPath;
}

OrdersDatabase::~OrdersDatabase() {
    if (m_db.isOpen()) {
        m_db.close();
    }
    QSqlDatabase::removeDatabase("OrdersDB");
}

void OrdersDatabase::createTable() {
    QSqlQuery query(m_db);
    
    QString createTableQuery = 
        "CREATE TABLE IF NOT EXISTS orders ("
        "order_id TEXT PRIMARY KEY, "
        "account_id TEXT NOT NULL, "
        "symbol TEXT NOT NULL, "
        "quantity TEXT NOT NULL, "
        "trade_action TEXT NOT NULL, "
        "order_type TEXT NOT NULL, "
        "status TEXT NOT NULL, "
        "status_description TEXT, "
        "limit_price REAL, "
        "stop_price REAL, "
        "filled_price REAL, "
        "opened_datetime TEXT, "
        "closed_datetime TEXT, "
        "received_time TEXT NOT NULL, "
        "filled_time TEXT, "
        "json_data TEXT NOT NULL"
        ")";

    if (!query.exec(createTableQuery)) {
        CRITICAL << "Failed to create orders table:" << query.lastError().text();
        Q_ASSERT_X(false, "OrdersDatabase::createTable", "Failed to create orders table");
    }
}

bool OrdersDatabase::insertOrder(const Order& p_order, const QDateTime& p_receivedTime) {
    QSqlQuery query(m_db);
    
    QString insertQuery = 
        "INSERT INTO orders ("
        "order_id, account_id, symbol, quantity, trade_action, order_type, "
        "status, status_description, limit_price, stop_price, filled_price, "
        "opened_datetime, closed_datetime, received_time, filled_time, json_data"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    query.prepare(insertQuery);
    query.addBindValue(p_order.getOrderID());
    query.addBindValue(p_order.getAccountID());
    query.addBindValue(p_order.getSymbol());
    query.addBindValue(p_order.getQuantity());
    query.addBindValue(p_order.getTradeAction());
    query.addBindValue(orderTypeToString(p_order.getOrderType().type));
    query.addBindValue(orderStatusToString(p_order.getOrderStatus()));
    query.addBindValue(p_order.getStatusDescription());
    
    // Handle optional limit and stop prices
    if (p_order.getLimitPrice().has_value()) {
        query.addBindValue(p_order.getLimitPrice().value());
    } else {
        query.addBindValue(QVariant(QVariant::Double));
    }
    
    if (p_order.getStopPrice().has_value()) {
        query.addBindValue(p_order.getStopPrice().value());
    } else {
        query.addBindValue(QVariant(QVariant::Double));
    }
    
    query.addBindValue(p_order.getFilledPrice());
    query.addBindValue(p_order.getOpenedDateTime().toString(Qt::ISODate));
    
    // Handle closed datetime (might not be set)
    if (p_order.closedDateTime.isValid()) {
        query.addBindValue(p_order.closedDateTime.toString(Qt::ISODate));
    } else {
        query.addBindValue(QVariant(QVariant::String));
    }
    
    query.addBindValue(p_receivedTime.toString(Qt::ISODate));
    
    // Filled time is null for new orders
    query.addBindValue(QVariant(QVariant::String));
    
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
    
    if (!query.exec()) {
        WARNING << "Failed to insert order" << p_order.getOrderID() << ":" << query.lastError().text();
        return false;
    }
    
    DEBUG << "Inserted order" << p_order.getOrderID() << "into database";
    return true;
}

bool OrdersDatabase::updateOrder(const Order& p_order, const std::optional<QDateTime>& p_filledTime) {
    QSqlQuery query(m_db);
    
    QString updateQuery = 
        "UPDATE orders SET "
        "status = ?, status_description = ?, filled_price = ?, "
        "closed_datetime = ?, filled_time = ?, json_data = ? "
        "WHERE order_id = ?";

    query.prepare(updateQuery);
    query.addBindValue(orderStatusToString(p_order.getOrderStatus()));
    query.addBindValue(p_order.getStatusDescription());
    query.addBindValue(p_order.getFilledPrice());
    
    // Handle closed datetime (might not be set)
    if (p_order.closedDateTime.isValid()) {
        query.addBindValue(p_order.closedDateTime.toString(Qt::ISODate));
    } else {
        query.addBindValue(QVariant(QVariant::String));
    }
    
    if (p_filledTime.has_value()) {
        query.addBindValue(p_filledTime.value().toString(Qt::ISODate));
    } else {
        query.addBindValue(QVariant(QVariant::String));
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
    
    if (!query.exec()) {
        WARNING << "Failed to update order" << p_order.getOrderID() << ":" << query.lastError().text();
        return false;
    }
    
    DEBUG << "Updated order" << p_order.getOrderID() << "in database";
    return true;
}

bool OrdersDatabase::orderExists(const QString& p_orderID) const {
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM orders WHERE order_id = ?");
    query.addBindValue(p_orderID);
    
    if (!query.exec()) {
        WARNING << "Failed to check if order exists:" << query.lastError().text();
        return false;
    }
    
    if (query.next()) {
        return query.value(0).toInt() > 0;
    }
    
    return false;
}

QMap<QString, std::tuple<Order, QDateTime, std::optional<QDateTime>>> OrdersDatabase::loadAllOrders() const {
    QMap<QString, std::tuple<Order, QDateTime, std::optional<QDateTime>>> orders;
    
    QSqlQuery query(m_db);
    if (!query.exec("SELECT order_id, received_time, filled_time, json_data FROM orders")) {
        WARNING << "Failed to load orders from database:" << query.lastError().text();
        return orders;
    }
    
    while (query.next()) {
        QString orderId = query.value(0).toString();
        QString receivedTimeStr = query.value(1).toString();
        QString filledTimeStr = query.value(2).toString();
        QString jsonDataStr = query.value(3).toString();
        
        QDateTime receivedTime = QDateTime::fromString(receivedTimeStr, Qt::ISODate);
        std::optional<QDateTime> filledTime;
        if (!filledTimeStr.isEmpty()) {
            filledTime = QDateTime::fromString(filledTimeStr, Qt::ISODate);
        }
        
        // Reconstruct the Order object from JSON
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonDataStr.toUtf8());
        if (jsonDoc.isObject()) {
            Order order(jsonDoc.object());
            orders[orderId] = std::make_tuple(order, receivedTime, filledTime);
        }
    }
    
    INFO << "Loaded" << orders.size() << "orders from database";
    return orders;
}

bool OrdersDatabase::isOpen() const {
    return m_db.isOpen();
}

QString OrdersDatabase::orderStatusToString(OrderStatus p_status) const {
    switch (p_status) {
        case OrderStatus::ACK: return "ACK";
        case OrderStatus::BRO: return "BRO";
        case OrderStatus::CAN: return "CAN";
        case OrderStatus::EXP: return "EXP";
        case OrderStatus::FLL: return "FLL";
        case OrderStatus::FLP: return "FLP";
        case OrderStatus::FPR: return "FPR";
        case OrderStatus::LAT: return "LAT";
        case OrderStatus::OPN: return "OPN";
        case OrderStatus::OUT: return "OUT";
        case OrderStatus::REJ: return "REJ";
        case OrderStatus::UCH: return "UCH";
        case OrderStatus::UCN: return "UCN";
        case OrderStatus::TSC: return "TSC";
        case OrderStatus::RJC: return "RJC";
        case OrderStatus::DON: return "DON";
        case OrderStatus::RSN: return "RSN";
        case OrderStatus::CND: return "CND";
        case OrderStatus::OSO: return "OSO";
        case OrderStatus::SUS: return "SUS";
        default: return "UNKNOWN";
    }
}

QString OrdersDatabase::orderTypeToString(OrderType::Type p_type) const {
    switch (p_type) {
        case OrderType::Type::Market: return "Market";
        case OrderType::Type::Limit: return "Limit";
        case OrderType::Type::StopMarket: return "StopMarket";
        case OrderType::Type::StopLimit: return "StopLimit";
        default: return "Unknown";
    }
}
