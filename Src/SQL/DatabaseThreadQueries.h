#pragma once

#include <QString>

namespace DatabaseThreadQueries
{
    /**
     * @brief Probe query used to detect the schema version.
     *
     * If this query fails (no timescale column), the database has the old v1 schema
     * and must be dropped and recreated.
     */
    const QString PROBE_SCHEMA_V2 = "SELECT timescale FROM bars LIMIT 1";

    const QString DROP_BAR_CACHE_TABLE = "DROP TABLE IF EXISTS bars";

    /**
     * @brief Multi-timescale bars table (schema v2).
     *
     * PRIMARY KEY is (timescale, date, index) so all 9 timescales live in one table.
     * The timescale column stores the TimeFrame enum value (1, 5, 15, 30, 60, 240, 1440, …).
     */
    const QString CREATE_BAR_CACHE_TABLE = "CREATE TABLE IF NOT EXISTS bars ("
                                           "timescale INTEGER NOT NULL, "
                                           "date TEXT NOT NULL, "
                                           "[index] INTEGER NOT NULL, "
                                           "open REAL, "
                                           "high REAL, "
                                           "low REAL, "
                                           "close REAL, "
                                           "volume INTEGER, "
                                           "status INTEGER DEFAULT 0, "
                                           "PRIMARY KEY (timescale, date, [index]))";

    const QString SELECT_BARS_BY_TIMESCALE_DATE_AND_INDEX =
        "SELECT [index], open, high, low, close, volume, status "
        "FROM bars WHERE timescale = ? AND date = ? AND [index] >= ? AND [index] <= ? "
        "ORDER BY [index]";

    const QString INSERT_OR_REPLACE_BAR = "INSERT OR REPLACE INTO bars "
                                          "(timescale, date, [index], open, high, low, close, volume, status) "
                                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";

    const QString DELETE_ALL_BARS = "DELETE FROM bars";

    const QString DELETE_BARS_BY_TIMESCALE = "DELETE FROM bars WHERE timescale = ?";

} // namespace DatabaseThreadQueries
