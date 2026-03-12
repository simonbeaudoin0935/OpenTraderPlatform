# BarCache - Bar Data Caching System - Agent Instructions

The BarCache system provides efficient two-tier (memory + disk) caching for historical and live bar data with thread-safe access.

## Overview

**Location**: `Src/Core/Cache/BarCache/`
**Purpose**: Cache and persist bar data for fast retrieval and historical analysis
**Thread Safety**: Yes - uses QReadWriteLock for concurrent access

## Architecture

### Two-Tier Cache Design

```
┌─────────────────────────────────────┐
│     In-Memory Cache (Tier 1)       │
│   QMap<QDate, QVector<Bar>>        │  ← Fast access
│   - Current day bars always loaded  │
│   - Recent days kept in memory      │
└─────────────────────────────────────┘
                 ↕
┌─────────────────────────────────────┐
│    SQLite Database (Tier 2)         │
│   ~/.local/share/L2Trader/bars/    │  ← Persistent storage
│   - Historical data                 │
│   - Automatic preloading            │
└─────────────────────────────────────┘
```

### Key Components

**BarCache.h/cpp**: Main cache class
- In-memory bar storage by date
- Database persistence coordination
- Thread-safe read/write operations

**DatabaseThread.h/cpp**: Dedicated SQLite thread
- Async database operations
- Schema management
- Query execution
- Connection pooling

**Bar.h** (`Src/Core/Models/Bar.h`): Bar data structure
- OHLC data (float precision)
- Volume (`quint64`)
- Timestamp (QDateTime with timezone)
- `BarStatus` enum (`Uninitialized`, `Null`, `Open`, `Closed`)

## Bar Data Structure

### Bar Class (`Src/Core/Models/Bar.h`)

```cpp
class Bar {
    QDateTime m_timeStamp;
    quint64 m_totalVolume = 0;
    float m_open = 0.0f;
    float m_high = 0.0f;
    float m_low = 0.0f;
    float m_close = 0.0f;
    BarStatus m_barStatus = BarStatus::Uninitialized;
};
```

Source-agnostic: populated from Databento OhlcvMsg (historical),
accumulated TradeMsg (live forming bar), or from the SQLite bar cache.

### BarStatus Enum

```cpp
enum class BarStatus : quint8 {
    Uninitialized = 0,  // Default-constructed, not yet populated
    Null,               // Placeholder bar (no market data)
    Open,               // Bar still forming (current interval)
    Closed,             // Bar is finalized
};
```

## Database Schema

### Bars Table

```sql
CREATE TABLE IF NOT EXISTS bars (
    timestamp INTEGER PRIMARY KEY,  -- Unix timestamp
    open REAL NOT NULL,
    high REAL NOT NULL,
    low REAL NOT NULL,
    close REAL NOT NULL,
    total_volume INTEGER NOT NULL,
    bar_status INTEGER NOT NULL    -- BarStatus enum value
);
```

### Indexes

```sql
CREATE INDEX IF NOT EXISTS idx_timestamp ON bars(timestamp);
```

**Database Location**:
- Pattern: `~/.cache/L2Trader/Bars/{SYMBOL}.db`
- Example: `NVDA.db`, `AAPL.db`
- One database per symbol

### SQL Queries

All SQL defined in `Src/SQL/` headers:
- `DatabaseThreadQueries.h`: Schema creation, CRUD operations
- `StockPriceChartQueries.h`: Chart data queries

## Thread Safety

### Read-Write Lock Pattern

```cpp
class BarCache {
    mutable QReadWriteLock m_barCacheRwLock;
    mutable QMap<QDate, QVector<Bar>> m_barCacheByDay;

public:
    // Multiple readers can access simultaneously
    QVector<Bar> getBarsForDay(const QDate& date) const {
        QReadLocker locker(&m_barCacheRwLock);
        return m_barCacheByDay.value(date);
    }

    // Writers get exclusive access
    void addBar(const Bar& bar) {
        QWriteLocker locker(&m_barCacheRwLock);
        QDate date = bar.timestamp().date();
        m_barCacheByDay[date].append(bar);
    }
};
```

### Benefits

- Multiple threads can read concurrently
- Writers block all readers and other writers
- Prevents race conditions
- Optimized for read-heavy workloads (common in trading)

## DatabaseThread Pattern

### Dedicated Worker Thread

Each BarCache instance has its own DatabaseThread:

```cpp
class BarCache {
    DatabaseThread* m_databaseThread;  // Owned, runs async DB ops
    QThread m_dbThread;                // Stack-allocated thread

    BarCache(const QString& symbol, const QString& timeframe) {
        m_databaseThread = new DatabaseThread(symbol, timeframe);
        m_databaseThread->moveToThread(&m_dbThread);
        m_dbThread.start();
    }

    ~BarCache() {
        m_dbThread.quit();
        if (!m_dbThread.wait(5000)) {
            m_dbThread.terminate();
            m_dbThread.wait();
        }
        delete m_databaseThread;
    }
};
```

### Async Operations

All database operations are async via signals:

```cpp
// Request (from any thread)
emit requestBarsFromDatabase(startDate, endDate);

// Response (received on requesting thread)
connect(m_databaseThread, &DatabaseThread::barsLoaded,
        this, [this](const QVector<Bar>& bars) {
            // Handle loaded bars
            QWriteLocker locker(&m_barCacheRwLock);
            mergeBarsIntoCache(bars);
        });
```

### Database Connection Management

- One connection per DatabaseThread
- Connection opened on thread start
- Proper cleanup on thread stop
- Transaction support for bulk operations

## Cache Management

### Automatic Preloading

On BarCache construction:
1. Open database (if exists)
2. Load last N days of bars into memory
3. Default: Last 30 days
4. Configurable via settings

### Cache Eviction

Strategies for memory management:
- **LRU (Least Recently Used)**: Remove oldest accessed days
- **Size-based**: Remove when cache exceeds threshold
- **Time-based**: Remove days older than retention period

Current: Manual clearing via GUI/API, automatic eviction not yet implemented

### Gap Detection

BarCache detects missing bars:
```cpp
QVector<QPair<QDateTime, QDateTime>> detectGaps(const QVector<Bar>& bars) {
    // Find time ranges with missing bars
    // Consider trading hours and holidays
    // Return list of gap ranges
}
```

Used by StockPriceChart to request missing data.

## Data Source Integration

### Databento Integration (Complete)

Bars are sourced from Databento via `DBClient`:
- **Historical**: `DBClient::fetchHistoricalBars()` → `historicalBarsReceived` signal → `BarCache::storeBarsInCache()`
- **Live**: `DBClient::newTrade` → `LiveBarAccumulator::barClosed` → `BarReceiver::receivedNewBar` → `BarCache::storeBar()`

The BarCache is data-source agnostic — it accepts `Bar` objects from any source via `addBar()` / `storeBarsInCache()`.

### Bar Timestamp Convention

**Open-time**: bars are timestamped at their open time (Databento native convention).
- Bar covering 4:00:00–4:00:59 → timestamped `4:00`
- First bar index 0 = 4:00 AM, last index 899 = 6:59 PM
- 900 bars per day (XNAS.ITCH trading hours)

### Database Location

- Pattern: `~/.cache/L2Trader/Bars/{SYMBOL}.db`
- Example: `NVDA.db`, `AAPL.db`
- One database per symbol

### Completeness Threshold

`DatabaseThread` accepts a database cache hit at ≥90% of expected bars (vs strict equality). This prevents infinite refetch loops when the data provider has minor gaps (e.g., 898/900 bars).

## Data Retrieval Patterns

### Get Bars for Date Range

```cpp
QVector<Bar> getBarsForDateRange(const QDate& start, const QDate& end) {
    QVector<Bar> result;

    // Try memory cache first
    {
        QReadLocker locker(&m_barCacheRwLock);
        for (QDate date = start; date <= end; date = date.addDays(1)) {
            if (m_barCacheByDay.contains(date)) {
                result.append(m_barCacheByDay.value(date));
            } else {
                // Miss - need to load from database
                emit requestBarsFromDatabase(date, date);
            }
        }
    }

    return result;
}
```

### Merge New Bars

When receiving bars from API or database:
```cpp
void mergeBarsIntoCache(const QVector<Bar>& newBars) {
    QWriteLocker locker(&m_barCacheRwLock);

    for (const Bar& bar : newBars) {
        QDate date = bar.timestamp().date();
        QVector<Bar>& dayBars = m_barCacheByDay[date];

        // Check for duplicates (by timestamp)
        if (!containsBarWithTimestamp(dayBars, bar.timestamp())) {
            dayBars.append(bar);
            std::sort(dayBars.begin(), dayBars.end(),
                     [](const Bar& a, const Bar& b) {
                         return a.timestamp() < b.timestamp();
                     });
        }
    }
}
```

## Performance Optimizations

### Memory Efficiency

1. **Float precision**: OHLC as float (4 bytes) vs double (8 bytes)
   - Stock prices rarely need more than 2-3 decimal places
   - Saves 16 bytes per bar

2. **Bitfield packing**: Status flags in single byte
   - Previously: Multiple bool members (8+ bytes)
   - Now: Single quint8 (1 byte)

3. **Efficient storage**: QVector for contiguous memory
   - Better cache locality
   - Faster iteration

### Database Efficiency

1. **Batch insertions**: Use transactions for multiple bars
   ```cpp
   db.transaction();
   for (const Bar& bar : bars) {
       insertBar(bar);
   }
   db.commit();
   ```

2. **Prepared statements**: Reuse for repeated queries
3. **Indexes**: Timestamp index for range queries
4. **Write-ahead logging**: Enable for concurrent readers

### Query Optimization

- Limit result sets with date ranges
- Use covering indexes when possible
- Avoid SELECT * (specify columns)
- Connection pooling per thread

## Error Handling

### Database Errors

```cpp
if (!db.open()) {
    qCCritical() << "Failed to open database:" << db.lastError();
    // Fallback to memory-only mode
    m_databaseEnabled = false;
}
```

### Data Corruption

- Validate bars before insertion (OHLC relationships)
- Reject bars with invalid timestamps
- Log suspicious data for review

## Testing Considerations

### Unit Tests

Test each component independently:
- Bar validation and serialization
- Cache operations (add, get, merge)
- Database operations (CRUD)
- Thread safety (concurrent access)

### Integration Tests

Test full pipeline:
- Data source → Cache → Database
- Database → Cache → UI
- Missing bar detection and filling

### Performance Tests

Measure:
- Cache hit rate
- Database query latency
- Memory usage with large datasets
- Concurrent read/write throughput

## Configuration

### Cache Settings

Via QSettings:
```cpp
Settings::getValue("BarCache/PreloadDays", 30);       // Days to preload
Settings::getValue("BarCache/MaxMemoryMB", 100);      // Memory limit
Settings::getValue("BarCache/EnablePersistence", true); // DB enabled
```

### Database Path

Configurable via settings or environment:
```cpp
QString dbPath = Settings::getValue(
    "BarCache/DatabasePath",
    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/bars"
);
```

## Common Patterns

### Creating BarCache

```cpp
// In SymbolContext or similar
BarCache m_barCache;  // Composition (preferred)

// Initialize
m_barCache.initialize(symbol, timeframe);
m_barCache.enablePersistence(true);
m_barCache.setPreloadDays(30);
```

### Requesting Historical Data

```cpp
// Check cache first
QVector<Bar> bars = m_barCache.getBarsForDateRange(startDate, endDate);

if (bars.isEmpty()) {
    // Cache miss → request from Databento via DBClient::fetchHistoricalBars()
    // Results arrive via historicalBarsReceived signal → storeBarsInCache()
}
```

### Clearing Cache

```cpp
// Clear memory only
m_barCache.clearMemoryCache();

// Clear database (permanent)
m_barCache.clearDatabase();

// Clear everything
m_barCache.clearAll();
```

## Debugging Tips

### Enable Logging

```cpp
QLoggingCategory::setFilterRules("BarCache*=true");
```

Categories:
- `BarCache.memory` - Memory operations
- `BarCache.database` - Database operations

### Monitor Cache Stats

```cpp
qCDebug() << "Cache size:" << m_barCache.getCacheSize();
qCDebug() << "Days in cache:" << m_barCache.getDaysInCache();
qCDebug() << "Database size:" << m_barCache.getDatabaseSize();
qCDebug() << "Cache hit rate:" << m_barCache.getCacheHitRate();
```

## Related Agent Instructions

- `../../AGENTS.md`: Core application components
- `../../../Algo/AGENTS.md`: Algorithm components using cache
- `../../../SQL/AGENTS.md`: SQL query definitions

## Related Documentation

- `Doc/ARCHITECTURE.md`: Bar caching in system context
- `Doc/DEVELOPMENT.md`: Development guidelines
