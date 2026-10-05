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
                                        "strategy_log TEXT, "
                                        "json_data TEXT NOT NULL"
                                        ")";

    const QString INSERT_ORDER = "INSERT INTO orders ("
                                 "order_id, account_id, symbol, quantity, trade_action, order_type, "
                                 "status, status_description, limit_price, stop_price, filled_price, "
                                 "opened_datetime, closed_datetime, latency_ms, strategy_log, json_data"
                                 ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    const QString UPDATE_ORDER = "UPDATE orders SET "
                                 "status = ?, status_description = ?, filled_price = ?, "
                                 "closed_datetime = ?, latency_ms = ?, json_data = ? "
                                 "WHERE order_id = ?";

    const QString SELECT_ORDER_EXISTS = "SELECT COUNT(*) FROM orders WHERE order_id = ?";

    const QString SELECT_ALL_ORDERS = "SELECT order_id, latency_ms, status, status_description, order_type, json_data, "
                                      "filled_price, limit_price, stop_price, opened_datetime, closed_datetime, "
                                      "strategy_log "
                                      "FROM orders";

    const QString UPDATE_ORDER_STRATEGY_LOG = "UPDATE orders SET strategy_log = ? WHERE order_id = ?";

    const QString SELECT_ORDER_COUNT = "SELECT COUNT(*) FROM orders";

    const QString DELETE_ALL_ORDERS = "DELETE FROM orders";

    // Strategy log markers — one row per sdk->logToChart() call
    const QString CREATE_STRATEGY_LOGS_TABLE = "CREATE TABLE IF NOT EXISTS strategy_logs ("
                                               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                               "strategy_id TEXT NOT NULL, "
                                               "symbol TEXT NOT NULL, "
                                               "timestamp TEXT NOT NULL, "
                                               "message TEXT NOT NULL"
                                               ")";

    const QString INSERT_STRATEGY_LOG = "INSERT INTO strategy_logs "
                                        "(strategy_id, symbol, timestamp, message) "
                                        "VALUES (?, ?, ?, ?)";

    const QString SELECT_STRATEGY_LOGS_FOR_SYMBOL = "SELECT id, strategy_id, symbol, timestamp, message "
                                                    "FROM strategy_logs WHERE symbol = ?";

    // Strategy bracket-overlay events — one row per upsert/clear emission
    const QString CREATE_STRATEGY_BRACKET_OVERLAYS_TABLE = "CREATE TABLE IF NOT EXISTS strategy_bracket_overlays ("
                                                           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                                           "strategy_id TEXT NOT NULL, "
                                                           "symbol TEXT NOT NULL, "
                                                           "timestamp TEXT NOT NULL, "
                                                           "action TEXT NOT NULL, "
                                                           "side TEXT, "
                                                           "stop_price REAL, "
                                                           "take_price REAL, "
                                                           "reference_entry_price REAL, "
                                                           "triggered INTEGER NOT NULL DEFAULT 0, "
                                                           "trigger_reason TEXT"
                                                           ")";

    const QString INSERT_STRATEGY_BRACKET_OVERLAY = "INSERT INTO strategy_bracket_overlays "
                                                    "(strategy_id, symbol, timestamp, action, side, stop_price, "
                                                    "take_price, reference_entry_price, triggered, trigger_reason) "
                                                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    const QString SELECT_STRATEGY_BRACKET_OVERLAYS_FOR_SYMBOL =
        "SELECT id, strategy_id, symbol, timestamp, action, side, stop_price, take_price, "
        "reference_entry_price, triggered, trigger_reason "
        "FROM strategy_bracket_overlays WHERE symbol = ? ORDER BY timestamp ASC, id ASC";
} // namespace OrdersDatabaseQueries
