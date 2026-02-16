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
 * @brief Database for persisting order history (Singleton)
 *
 * This class manages a SQLite database that stores all orders received through
 * the orders stream, including their received and filled timestamps. This allows
 * the application to maintain order history across restarts.
 *
 * This is a singleton class - use getInstance() to get the single instance.
 */
class OrdersDatabase : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Get the singleton instance of OrdersDatabase
     * @param p_parent Optional parent object (only used on first call)
     * @return Pointer to the singleton instance
     */
    static OrdersDatabase* getInstance(QObject* p_parent = nullptr);

    /**
     * @brief Destroy the singleton instance
     *
     * Called when switching between Live/Sim and Replay modes to recreate
     * the database with the correct file path.
     */
    static void destroyInstance();

    ~OrdersDatabase();

    // Delete copy constructor and assignment operator
    OrdersDatabase(const OrdersDatabase&) = delete;
    OrdersDatabase& operator=(const OrdersDatabase&) = delete;

    /**
     * @brief Insert a new order into the database
     * @param p_order The order to insert
     * @param p_latencyMs The ack/fill latency in milliseconds (nullopt if not yet known)
     * @return true if successful, false otherwise
     */
    bool insertOrder(const Order& p_order, std::optional<qint64> p_latencyMs = std::nullopt);

    /**
     * @brief Update an existing order in the database
     * @param p_order The order to update
     * @param p_latencyMs The ack/fill latency in milliseconds (nullopt to keep existing)
     * @return true if successful, false otherwise
     */
    bool updateOrder(const Order& p_order, std::optional<qint64> p_latencyMs = std::nullopt);

    /**
     * @brief Check if an order exists in the database
     * @param p_orderID The order ID to check
     * @return true if the order exists, false otherwise
     */
    bool orderExists(const QString& p_orderID) const;

    /**
     * @brief Load all orders from the database
     * @return Map of order ID to tuple of (Order, latencyMs)
     */
    QMap<QString, std::tuple<Order, std::optional<qint64>>> loadAllOrders() const;

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
    QString getDatabasePath() const
    {
        return m_dbPath;
    }

    /**
     * @brief Clear all orders from the database
     * @return true if successful, false otherwise
     */
    bool clearAllOrders();

  private:
    explicit OrdersDatabase(const QString& p_dbPath, QObject* p_parent = nullptr);

    void createTable();
    QString orderStatusToString(Order::Status p_status) const;
    QString orderTypeToString(OrderType::Type p_type) const;

    QSqlDatabase m_db;
    QString m_dbPath;
    QString m_connectionName;

    static OrdersDatabase* s_instance; // Singleton instance
};
