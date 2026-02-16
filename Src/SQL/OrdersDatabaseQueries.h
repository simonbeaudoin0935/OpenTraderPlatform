#pragma once

#include <QString>

namespace OrdersDatabaseQueries
{
    const QString CREATE_ORDERS_TABLE = "CREATE TABLE IF NOT EXISTS orders ("
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
                                        "latency_ms INTEGER, "
                                        "json_data TEXT NOT NULL"
                                        ")";

    const QString INSERT_ORDER = "INSERT INTO orders ("
                                 "order_id, account_id, symbol, quantity, trade_action, order_type, "
                                 "status, status_description, limit_price, stop_price, filled_price, "
                                 "opened_datetime, closed_datetime, latency_ms, json_data"
                                 ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    const QString UPDATE_ORDER = "UPDATE orders SET "
                                 "status = ?, status_description = ?, filled_price = ?, "
                                 "closed_datetime = ?, latency_ms = ?, json_data = ? "
                                 "WHERE order_id = ?";

    const QString SELECT_ORDER_EXISTS = "SELECT COUNT(*) FROM orders WHERE order_id = ?";

    const QString SELECT_ALL_ORDERS = "SELECT order_id, latency_ms, status, order_type, json_data, "
                                     "filled_price, limit_price, stop_price, opened_datetime, closed_datetime "
                                     "FROM orders";

    const QString SELECT_ORDER_COUNT = "SELECT COUNT(*) FROM orders";

    const QString DELETE_ALL_ORDERS = "DELETE FROM orders";
} // namespace OrdersDatabaseQueries
