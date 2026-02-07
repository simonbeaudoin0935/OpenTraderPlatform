# SQL/ Directory - SQL Query Management - Agent Instructions

The SQL directory contains ALL SQL query definitions for the L2Trader application.

## Overview

**Location**: `Src/SQL/`
**Purpose**: Centralize SQL queries in dedicated header files
**Organization**: One header file per class/component, organized by namespace

## Core Principle

**NEVER write SQL queries directly in source files**

All SQL must be defined in this directory:
```cpp
// ✓ CORRECT - Use centralized SQL
#include "SQL/OrdersDatabaseQueries.h"
query.exec(OrdersDatabaseQueries::CREATE_ORDERS_TABLE);

// ✗ WRONG - Don't write SQL inline in .cpp files
query.exec("CREATE TABLE orders (...)");  // NO!
```

## File Structure

Each header file corresponds to a class or component that uses SQL:

### OrdersDatabaseQueries.h

SQL for OrdersDatabase class:

```cpp
#ifndef ORDERSDATABASEQUERIES_H
#define ORDERSDATABASEQUERIES_H

namespace OrdersDatabaseQueries {

// Table creation
constexpr const char* CREATE_ORDERS_TABLE = R"(
    CREATE TABLE IF NOT EXISTS orders (
        order_id TEXT PRIMARY KEY,
        account_id TEXT NOT NULL,
        symbol TEXT NOT NULL,
        trade_action TEXT NOT NULL,
        order_type TEXT NOT NULL,
        quantity INTEGER NOT NULL,
        limit_price REAL,
        stop_price REAL,
        time_in_force TEXT,
        status TEXT NOT NULL,
        filled_quantity INTEGER,
        average_fill_price REAL,
        timestamp TEXT NOT NULL
    )
)";

// Indexes
constexpr const char* CREATE_TIMESTAMP_INDEX = R"(
    CREATE INDEX IF NOT EXISTS idx_orders_timestamp
    ON orders(timestamp)
)";

constexpr const char* CREATE_SYMBOL_INDEX = R"(
    CREATE INDEX IF NOT EXISTS idx_orders_symbol
    ON orders(symbol)
)";

// CRUD operations
constexpr const char* INSERT_ORDER = R"(
    INSERT OR REPLACE INTO orders
    (order_id, account_id, symbol, trade_action, order_type, quantity,
     limit_price, stop_price, time_in_force, status, filled_quantity,
     average_fill_price, timestamp)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
)";

constexpr const char* UPDATE_ORDER = R"(
    UPDATE orders
    SET status = ?, filled_quantity = ?, average_fill_price = ?
    WHERE order_id = ?
)";

constexpr const char* DELETE_ORDER = R"(
    DELETE FROM orders WHERE order_id = ?
)";

// Queries
constexpr const char* SELECT_ORDER_BY_ID = R"(
    SELECT * FROM orders WHERE order_id = ?
)";

constexpr const char* SELECT_ORDERS_BY_DATE_RANGE = R"(
    SELECT * FROM orders
    WHERE timestamp BETWEEN ? AND ?
    ORDER BY timestamp DESC
)";

constexpr const char* SELECT_ORDERS_BY_SYMBOL = R"(
    SELECT * FROM orders
    WHERE symbol = ?
    ORDER BY timestamp DESC
)";

constexpr const char* SELECT_ALL_ORDERS = R"(
    SELECT * FROM orders
    ORDER BY timestamp DESC
)";

}  // namespace OrdersDatabaseQueries

#endif // ORDERSDATABASEQUERIES_H
```

### DatabaseThreadQueries.h

SQL for BarCache DatabaseThread operations:

```cpp
namespace DatabaseThreadQueries {

// Bar table creation
constexpr const char* CREATE_BARS_TABLE = R"(
    CREATE TABLE IF NOT EXISTS bars (
        timestamp INTEGER PRIMARY KEY,
        open REAL NOT NULL,
        high REAL NOT NULL,
        low REAL NOT NULL,
        close REAL NOT NULL,
        total_volume INTEGER NOT NULL,
        trade_count INTEGER NOT NULL,
        flags INTEGER NOT NULL
    )
)";

// Index
constexpr const char* CREATE_TIMESTAMP_INDEX = R"(
    CREATE INDEX IF NOT EXISTS idx_bars_timestamp
    ON bars(timestamp)
)";

// Insert/Update
constexpr const char* INSERT_BAR = R"(
    INSERT OR REPLACE INTO bars
    (timestamp, open, high, low, close, total_volume, trade_count, flags)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
)";

// Queries
constexpr const char* SELECT_BARS_BY_DATE_RANGE = R"(
    SELECT * FROM bars
    WHERE timestamp BETWEEN ? AND ?
    ORDER BY timestamp ASC
)";

constexpr const char* SELECT_BAR_COUNT = R"(
    SELECT COUNT(*) FROM bars
)";

constexpr const char* SELECT_LATEST_BAR = R"(
    SELECT * FROM bars
    ORDER BY timestamp DESC
    LIMIT 1
)";

constexpr const char* SELECT_OLDEST_BAR = R"(
    SELECT * FROM bars
    ORDER BY timestamp ASC
    LIMIT 1
)";

// Delete
constexpr const char* DELETE_BARS_BEFORE_DATE = R"(
    DELETE FROM bars WHERE timestamp < ?
)";

constexpr const char* DELETE_ALL_BARS = R"(
    DELETE FROM bars
)";

}  // namespace DatabaseThreadQueries
```

### LiveStreamDBQueries.h

SQL for live stream data persistence:

```cpp
namespace LiveStreamDBQueries {

// Stream session tracking
constexpr const char* CREATE_STREAM_SESSIONS_TABLE = R"(
    CREATE TABLE IF NOT EXISTS stream_sessions (
        session_id TEXT PRIMARY KEY,
        symbol TEXT NOT NULL,
        interval TEXT NOT NULL,
        start_time TEXT NOT NULL,
        end_time TEXT,
        bar_count INTEGER DEFAULT 0
    )
)";

// Bar storage (same as DatabaseThreadQueries but separate table)
constexpr const char* CREATE_STREAM_BARS_TABLE = R"(
    CREATE TABLE IF NOT EXISTS stream_bars (
        timestamp INTEGER NOT NULL,
        session_id TEXT NOT NULL,
        open REAL NOT NULL,
        high REAL NOT NULL,
        low REAL NOT NULL,
        close REAL NOT NULL,
        total_volume INTEGER NOT NULL,
        trade_count INTEGER NOT NULL,
        flags INTEGER NOT NULL,
        PRIMARY KEY (timestamp, session_id),
        FOREIGN KEY (session_id) REFERENCES stream_sessions(session_id)
    )
)";

// Session operations
constexpr const char* INSERT_STREAM_SESSION = R"(
    INSERT INTO stream_sessions
    (session_id, symbol, interval, start_time)
    VALUES (?, ?, ?, ?)
)";

constexpr const char* UPDATE_STREAM_SESSION_END = R"(
    UPDATE stream_sessions
    SET end_time = ?, bar_count = ?
    WHERE session_id = ?
)";

// Bar operations
constexpr const char* INSERT_STREAM_BAR = R"(
    INSERT OR REPLACE INTO stream_bars
    (timestamp, session_id, open, high, low, close, total_volume, trade_count, flags)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
)";

}  // namespace LiveStreamDBQueries
```

### StockPriceChartQueries.h

SQL for chart-specific queries:

```cpp
namespace StockPriceChartQueries {

// Cache table for chart state
constexpr const char* CREATE_CHART_STATE_TABLE = R"(
    CREATE TABLE IF NOT EXISTS chart_state (
        symbol TEXT PRIMARY KEY,
        visible_range_start INTEGER,
        visible_range_end INTEGER,
        zoom_level REAL,
        origin_index INTEGER,
        last_update TEXT
    )
)";

// Save/Load chart state
constexpr const char* SAVE_CHART_STATE = R"(
    INSERT OR REPLACE INTO chart_state
    (symbol, visible_range_start, visible_range_end, zoom_level, origin_index, last_update)
    VALUES (?, ?, ?, ?, ?, ?)
)";

constexpr const char* LOAD_CHART_STATE = R"(
    SELECT * FROM chart_state WHERE symbol = ?
)";

}  // namespace StockPriceChartQueries
```

### ReplayDataQueries.h

SQL for replay data management:

```cpp
namespace ReplayDataQueries {

// Replay data table
constexpr const char* CREATE_REPLAY_DATA_TABLE = R"(
    CREATE TABLE IF NOT EXISTS replay_data (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        symbol TEXT NOT NULL,
        timestamp INTEGER NOT NULL,
        open REAL NOT NULL,
        high REAL NOT NULL,
        low REAL NOT NULL,
        close REAL NOT NULL,
        volume INTEGER NOT NULL,
        UNIQUE(symbol, timestamp)
    )
)";

// Import from CSV
constexpr const char* INSERT_REPLAY_BAR = R"(
    INSERT OR IGNORE INTO replay_data
    (symbol, timestamp, open, high, low, close, volume)
    VALUES (?, ?, ?, ?, ?, ?, ?)
)";

// Queries
constexpr const char* SELECT_REPLAY_BARS = R"(
    SELECT * FROM replay_data
    WHERE symbol = ? AND timestamp BETWEEN ? AND ?
    ORDER BY timestamp ASC
)";

constexpr const char* SELECT_REPLAY_DATE_RANGE = R"(
    SELECT MIN(timestamp) as start, MAX(timestamp) as end
    FROM replay_data
    WHERE symbol = ?
)";

}  // namespace ReplayDataQueries
```

## Best Practices

### Using Raw String Literals

Use R"(...)" for multi-line SQL:
```cpp
// ✓ CORRECT - Raw string literal (no escaping needed)
constexpr const char* QUERY = R"(
    SELECT * FROM table
    WHERE column = 'value'
)";

// ✗ WRONG - Traditional string (requires escaping)
constexpr const char* QUERY = "SELECT * FROM table WHERE column = 'value'";
```

### Query Organization

Group related queries:
```cpp
namespace MyQueries {
    // 1. Schema/Table Creation
    constexpr const char* CREATE_TABLE = "...";
    constexpr const char* CREATE_INDEX = "...";

    // 2. Insert/Update/Delete
    constexpr const char* INSERT = "...";
    constexpr const char* UPDATE = "...";
    constexpr const char* DELETE = "...";

    // 3. Select Queries
    constexpr const char* SELECT_BY_ID = "...";
    constexpr const char* SELECT_ALL = "...";
}
```

### Parameterized Queries

Use placeholders for parameters:
```cpp
constexpr const char* INSERT_ORDER = R"(
    INSERT INTO orders (order_id, symbol, quantity)
    VALUES (?, ?, ?)
)";

// Usage
QSqlQuery query;
query.prepare(OrdersDatabaseQueries::INSERT_ORDER);
query.addBindValue(orderId);
query.addBindValue(symbol);
query.addBindValue(quantity);
query.exec();
```

### Documentation

Add comments explaining complex queries:
```cpp
// Retrieve all orders from last 30 days with filled status,
// joining with positions table to calculate actual P/L
constexpr const char* SELECT_RECENT_FILLED_ORDERS_WITH_PNL = R"(
    SELECT o.*, p.realized_pnl
    FROM orders o
    LEFT JOIN positions p ON o.order_id = p.order_id
    WHERE o.status = 'Filled'
      AND o.timestamp > datetime('now', '-30 days')
    ORDER BY o.timestamp DESC
)";
```

## Adding New Queries

### Steps

1. **Create header file** if it doesn't exist:
   ```cpp
   // NewComponentQueries.h
   #ifndef NEWCOMPONENTQUERIES_H
   #define NEWCOMPONENTQUERIES_H

   namespace NewComponentQueries {
       // Queries here
   }

   #endif
   ```

2. **Define queries** in namespace:
   ```cpp
   constexpr const char* CREATE_TABLE = R"(...)";
   constexpr const char* INSERT = R"(...)";
   // etc.
   ```

3. **Include in CMakeLists.txt**:
   ```cmake
   set(SQL_HEADERS
       SQL/OrdersDatabaseQueries.h
       SQL/DatabaseThreadQueries.h
       SQL/NewComponentQueries.h  # Add new file
   )
   ```

4. **Use in source file**:
   ```cpp
   #include "SQL/NewComponentQueries.h"
   query.exec(NewComponentQueries::CREATE_TABLE);
   ```

## Schema Versioning

For database migrations:
```cpp
namespace SchemaVersioning {
    constexpr int CURRENT_VERSION = 2;

    constexpr const char* CREATE_VERSION_TABLE = R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            version INTEGER PRIMARY KEY
        )
    )";

    constexpr const char* GET_VERSION = R"(
        SELECT version FROM schema_version LIMIT 1
    )";

    constexpr const char* SET_VERSION = R"(
        INSERT OR REPLACE INTO schema_version (version) VALUES (?)
    )";

    // Migration queries
    constexpr const char* MIGRATE_V1_TO_V2 = R"(
        ALTER TABLE orders ADD COLUMN commission REAL DEFAULT 0.0
    )";
}
```

## Testing Queries

### Validation

Test queries for:
- Syntax correctness
- Parameter binding
- Expected results
- Performance

```cpp
TEST(OrdersDatabaseQueries, CreatesTableSuccessfully) {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    db.open();

    QSqlQuery query(db);
    EXPECT_TRUE(query.exec(OrdersDatabaseQueries::CREATE_ORDERS_TABLE));

    // Verify table exists
    EXPECT_TRUE(db.tables().contains("orders"));
}
```

## SQLite Specific

### Pragma Statements

For SQLite configuration:
```cpp
namespace SQLitePragmas {
    constexpr const char* ENABLE_FOREIGN_KEYS = "PRAGMA foreign_keys = ON";
    constexpr const char* ENABLE_WAL = "PRAGMA journal_mode = WAL";
    constexpr const char* SET_CACHE_SIZE = "PRAGMA cache_size = 10000";
    constexpr const char* SYNCHRONOUS_NORMAL = "PRAGMA synchronous = NORMAL";
}
```

### Common SQLite Patterns

```cpp
// Transaction
constexpr const char* BEGIN_TRANSACTION = "BEGIN TRANSACTION";
constexpr const char* COMMIT_TRANSACTION = "COMMIT";
constexpr const char* ROLLBACK_TRANSACTION = "ROLLBACK";

// Database info
constexpr const char* GET_TABLE_LIST = R"(
    SELECT name FROM sqlite_master WHERE type='table'
)";

constexpr const char* GET_TABLE_INFO = "PRAGMA table_info(?)";
constexpr const char* GET_INDEX_LIST = "PRAGMA index_list(?)";
```

## Related Agent Instructions

- `../Misc/AGENTS.md`: Constants and utilities
- `../Core/AGENTS.md`: Core database components
- `../Core/Cache/BarCache/AGENTS.md`: Bar cache database

## Related Documentation

- SQLite Documentation: https://www.sqlite.org/docs.html
- Qt SQL Module: https://doc.qt.io/qt-6/qtsql-index.html
