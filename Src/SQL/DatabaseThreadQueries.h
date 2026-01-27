#pragma once

#include <QString>

namespace DatabaseThreadQueries
{
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
} // namespace DatabaseThreadQueries
