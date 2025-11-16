#pragma once

#include <QString>

namespace SqlQueries {
    const QString CREATE_BARS_TABLE = "CREATE TABLE IF NOT EXISTS bars ("
                                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                      "stock TEXT, "
                                      "stock_sequence INTEGER, "
                                      "timestamp INTEGER, "
                                      "json_data TEXT)";

    const QString INSERT_BAR = "INSERT INTO bars (stock, stock_sequence, timestamp, json_data) "
                               "VALUES (?, ?, ?, ?)";
}