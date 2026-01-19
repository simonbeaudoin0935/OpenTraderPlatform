#pragma once

#include <QString>

namespace SqlQueries
{
// ============================================================================
// Recorder Database Queries (LiveStreamDB)
// ============================================================================

const QString CREATE_BARS_TABLE = "CREATE TABLE IF NOT EXISTS bars ("
                                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                  "stockTicker TEXT, "       // e.g. "AAPL"
                                  "stockTickerSeq INTEGER, " // sequence number per stock ticker
                                  "epochMs INTEGER, "        // milliseconds since epoch
                                  "jsonRawData TEXT)"; // JSON fragment, possibly incomplete due to TCP packet splitting

const QString INSERT_BAR = "INSERT INTO bars (stockTicker, stockTickerSeq, epochMs, jsonRawData) "
                           "VALUES (?, ?, ?, ?)";

const QString CREATE_MARKET_DEPTH_QUOTES_TABLE =
    "CREATE TABLE IF NOT EXISTS market_depth_quotes ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "stockTicker TEXT, "       // e.g. "AAPL"
    "stockTickerSeq INTEGER, " // sequence number per stock ticker
    "epochMs INTEGER, "        // milliseconds since epoch
    "jsonRawData TEXT)";       // JSON fragment, possibly incomplete due to TCP packet splitting

const QString INSERT_MARKET_DEPTH_QUOTE =
    "INSERT INTO market_depth_quotes (stockTicker, stockTickerSeq, epochMs, jsonRawData) "
    "VALUES (?, ?, ?, ?)";

const QString SELECT_COUNT_FROM_TABLE = "SELECT COUNT(*) FROM %1";

// ============================================================================
// Orders Database Queries (OrdersDatabase)
// ============================================================================

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
                                    "received_time TEXT NOT NULL, "
                                    "filled_time TEXT, "
                                    "json_data TEXT NOT NULL"
                                    ")";

const QString INSERT_ORDER = "INSERT INTO orders ("
                             "order_id, account_id, symbol, quantity, trade_action, order_type, "
                             "status, status_description, limit_price, stop_price, filled_price, "
                             "opened_datetime, closed_datetime, received_time, filled_time, json_data"
                             ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

const QString UPDATE_ORDER = "UPDATE orders SET "
                             "status = ?, status_description = ?, filled_price = ?, "
                             "closed_datetime = ?, filled_time = ?, json_data = ? "
                             "WHERE order_id = ?";

const QString SELECT_ORDER_EXISTS = "SELECT COUNT(*) FROM orders WHERE order_id = ?";

const QString SELECT_ALL_ORDERS = "SELECT order_id, received_time, filled_time, json_data FROM orders";

const QString SELECT_ORDER_COUNT = "SELECT COUNT(*) FROM orders";

const QString DELETE_ALL_ORDERS = "DELETE FROM orders";

// ============================================================================
// Bar Cache Database Queries (DatabaseThread)
// ============================================================================

const QString CREATE_BAR_CACHE_TABLE = "CREATE TABLE IF NOT EXISTS bars ("
                                       "date TEXT, "
                                       "[index] INTEGER, "
                                       "open REAL, "
                                       "high REAL, "
                                       "low REAL, "
                                       "close REAL, "
                                       "volume INTEGER, "
                                       "status INTEGER DEFAULT 0, "
                                       "PRIMARY KEY (date, [index]))";

const QString SELECT_BARS_BY_DATE_AND_INDEX = "SELECT [index], open, high, low, close, volume, status "
                                              "FROM bars WHERE date = ? AND [index] >= ? AND [index] <= ? "
                                              "ORDER BY [index]";

const QString INSERT_OR_REPLACE_BAR = "INSERT OR REPLACE INTO bars "
                                      "(date, [index], open, high, low, close, volume, status) "
                                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

const QString DELETE_ALL_BARS = "DELETE FROM bars";

// ============================================================================
// Chart Replay Queries (StockPriceChart)
// ============================================================================

const QString SELECT_STOCK_TIME_RANGE = "SELECT MIN(epochMs), MAX(epochMs), COUNT(*) FROM bars "
                                        "WHERE stockTicker = ? AND epochMs >= ? AND epochMs <= ?";

} // namespace SqlQueries