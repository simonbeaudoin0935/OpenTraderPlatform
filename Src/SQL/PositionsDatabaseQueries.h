#pragma once

#include <QString>

namespace PositionsDatabaseQueries
{
    const QString CREATE_POSITIONS_TABLE = "CREATE TABLE IF NOT EXISTS positions ("
                                           "position_id TEXT NOT NULL, "
                                           "account_id TEXT NOT NULL, "
                                           "symbol TEXT NOT NULL, "
                                           "quantity TEXT NOT NULL, "
                                           "average_price TEXT NOT NULL, "
                                           "last TEXT NOT NULL, "
                                           "mark_to_market_price TEXT NOT NULL, "
                                           "market_value TEXT NOT NULL, "
                                           "total_cost TEXT NOT NULL, "
                                           "unrealized_profit_loss TEXT NOT NULL, "
                                           "unrealized_profit_loss_percent TEXT NOT NULL, "
                                           "todays_profit_loss TEXT NOT NULL, "
                                           "long_short TEXT NOT NULL, "
                                           "asset_type TEXT NOT NULL, "
                                           "bid TEXT NOT NULL, "
                                           "ask TEXT NOT NULL, "
                                           "conversion_rate TEXT NOT NULL, "
                                           "day_trade_requirement TEXT NOT NULL, "
                                           "initial_requirement TEXT NOT NULL, "
                                           "maintenance_margin TEXT NOT NULL, "
                                           "unrealized_profit_loss_qty TEXT NOT NULL, "
                                           "deleted INTEGER NOT NULL DEFAULT 0, "
                                           "expiration_date TEXT, "
                                           "timestamp TEXT NOT NULL, "
                                           "opened_datetime TEXT, "
                                           "closed_datetime TEXT, "
                                           "json_data TEXT NOT NULL, "
                                           "PRIMARY KEY (position_id, timestamp)"
                                           ")";

    const QString INSERT_POSITION =
        "INSERT INTO positions ("
        "position_id, account_id, symbol, quantity, average_price, last, "
        "mark_to_market_price, market_value, total_cost, unrealized_profit_loss, "
        "unrealized_profit_loss_percent, todays_profit_loss, long_short, asset_type, "
        "bid, ask, conversion_rate, day_trade_requirement, initial_requirement, "
        "maintenance_margin, unrealized_profit_loss_qty, deleted, expiration_date, "
        "timestamp, opened_datetime, closed_datetime, json_data"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    const QString UPDATE_POSITION = "UPDATE positions SET "
                                    "quantity = ?, average_price = ?, last = ?, mark_to_market_price = ?, "
                                    "market_value = ?, total_cost = ?, unrealized_profit_loss = ?, "
                                    "unrealized_profit_loss_percent = ?, todays_profit_loss = ?, "
                                    "bid = ?, ask = ?, deleted = ?, timestamp = ?, closed_datetime = ?, json_data = ? "
                                    "WHERE position_id = ?";

    const QString SELECT_POSITION_EXISTS = "SELECT COUNT(*) FROM positions WHERE position_id = ?";

    const QString SELECT_ALL_POSITIONS =
        "SELECT position_id, opened_datetime, closed_datetime, json_data FROM positions";

    const QString SELECT_POSITION_ORDERS = "SELECT json_data, status, filled_price, opened_datetime, closed_datetime "
                                           "FROM orders WHERE account_id = ? AND symbol = ?";

    const QString SELECT_POSITION_COUNT = "SELECT COUNT(*) FROM positions";

    const QString DELETE_ALL_POSITIONS = "DELETE FROM positions";
} // namespace PositionsDatabaseQueries
