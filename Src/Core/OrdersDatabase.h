#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QDateTime>
#include <QMap>
#include <QVector>
#include <QLoggingCategory>
#include <QMetaType>

#include "Order.h"

Q_DECLARE_LOGGING_CATEGORY(OrdersDatabaseLog)

struct StrategyLogEntry
{
    int id = 0; // DB-assigned, 0 before insert
    QString strategyID;
    QString symbol;
    QDateTime timestamp; // America/New_York timezone
    QString message;
};

struct StrategyStatusEntry
{
    enum class Action : quint8
    {
        Upsert,
        Clear,
    };

    QString strategyID;
    QString symbol;
    QDateTime timestamp; // America/New_York timezone
    QString message;
    Action action = Action::Upsert;
};

struct StrategyBracketOverlayEntry
{
    enum class Action
    {
        Upsert,
        Clear
    };

    enum class Side
    {
        Long,
        Short
    };

    int id = 0; // DB-assigned, 0 before insert
    QString strategyID;
    QString symbol;
    QDateTime timestamp; // America/New_York timezone
    Action action = Action::Upsert;
    Side side = Side::Long;
    double stopPrice = 0.0;
    double takePrice = 0.0;
    double referenceEntryPrice = 0.0;
    bool triggered = false;
    QString triggerReason;
};

Q_DECLARE_METATYPE(StrategyLogEntry)
Q_DECLARE_METATYPE(StrategyStatusEntry)
Q_DECLARE_METATYPE(StrategyBracketOverlayEntry)

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

    /**
     * @brief Update the strategy_log field for an existing order.
     * @param p_orderID Order to update
     * @param p_log     Log message to attach
     * @return true if successful, false otherwise
     */
    bool updateOrderStrategyLog(const QString& p_orderID, const QString& p_log);

    /**
     * @brief Insert a strategy log entry into the strategy_logs table.
     * @param p_entry The log entry to insert (id field is ignored; assigned by DB)
     * @return true if successful, false otherwise
     */
    bool insertStrategyLog(const StrategyLogEntry& p_entry);

    /**
     * @brief Load all strategy log entries for a given symbol.
     * @param p_symbol The stock symbol to filter by
     * @return Vector of log entries ordered by timestamp ascending
     */
    QVector<StrategyLogEntry> loadStrategyLogs(const QString& p_symbol) const;

    /**
     * @brief Insert a bracket-overlay event emitted by a strategy.
     * @param p_entry Event payload to persist
     * @return true if successful, false otherwise
     */
    bool insertStrategyBracketOverlay(const StrategyBracketOverlayEntry& p_entry);

    /**
     * @brief Load all bracket-overlay events for one symbol.
     * @param p_symbol Symbol filter
     * @return Events ordered by timestamp ascending
     */
    QVector<StrategyBracketOverlayEntry> loadStrategyBracketOverlays(const QString& p_symbol) const;

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
