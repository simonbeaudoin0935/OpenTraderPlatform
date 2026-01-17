#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QDateTime>
#include <QMap>
#include <QLoggingCategory>

#include "Order.h"

Q_DECLARE_LOGGING_CATEGORY(OrdersDatabaseLog)

/**
 * @brief Database for persisting order history
 * 
 * This class manages a SQLite database that stores all orders received through
 * the orders stream, including their received and filled timestamps. This allows
 * the application to maintain order history across restarts.
 */
class OrdersDatabase : public QObject {
    Q_OBJECT

public:
    explicit OrdersDatabase(const QString& p_dbPath, QObject* p_parent = nullptr);
    ~OrdersDatabase();

    /**
     * @brief Insert a new order into the database
     * @param p_order The order to insert
     * @param p_receivedTime When the order was first received
     * @return true if successful, false otherwise
     */
    bool insertOrder(const Order& p_order, const QDateTime& p_receivedTime);

    /**
     * @brief Update an existing order in the database
     * @param p_order The order to update
     * @param p_filledTime When the order was filled (optional)
     * @return true if successful, false otherwise
     */
    bool updateOrder(const Order& p_order, const std::optional<QDateTime>& p_filledTime = std::nullopt);

    /**
     * @brief Check if an order exists in the database
     * @param p_orderID The order ID to check
     * @return true if the order exists, false otherwise
     */
    bool orderExists(const QString& p_orderID) const;

    /**
     * @brief Load all orders from the database
     * @return Map of order ID to tuple of (Order, receivedTime, filledTime)
     */
    QMap<QString, std::tuple<Order, QDateTime, std::optional<QDateTime>>> loadAllOrders() const;

    /**
     * @brief Check if the database is open
     * @return true if open, false otherwise
     */
    bool isOpen() const;

    /**
     * @brief Get the total number of orders in the database
     * @return Total order count
     */
    int getOrderCount() const;

    /**
     * @brief Get the database file path
     * @return Database file path
     */
    QString getDatabasePath() const { return m_dbPath; }

    /**
     * @brief Clear all orders from the database
     * @return true if successful, false otherwise
     */
    bool clearAllOrders();

private:
    void createTable();
    QString orderStatusToString(Order::Status p_status) const;
    QString orderTypeToString(OrderType::Type p_type) const;

    QSqlDatabase m_db;
    QString m_dbPath;
    QString m_connectionName;  // Unique connection name for this instance
    
    static int s_instanceCounter;  // Counter for generating unique connection names
};
