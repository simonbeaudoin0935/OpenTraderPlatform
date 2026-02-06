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
- Stream lifecycle management
- Thread-safe read/write operations

**DatabaseThread.h/cpp**: Dedicated SQLite thread
- Async database operations
- Schema management
- Query execution
- Connection pooling

**LiveStreamDB.h/cpp**: Live stream data persistence
- Records streaming bars to database
- Real-time write batching
- Crash recovery

**Bar.h**: Bar data structure (optimized to ~104 bytes)
- OHLC data (float precision)
- Volume and trade count
- Timestamp (QDateTime with timezone)
- Status flags (bitfield packed)

## Bar Data Structure

### Optimized Bar Class

```cpp
class Bar {
    QDateTime m_timestamp;      // 8 bytes (Unix timestamp + timezone)
    
    // OHLC as float (sufficient precision for stock prices)
    float m_open;               // 4 bytes
    float m_high;               // 4 bytes
    float m_low;                // 4 bytes
    float m_close;              // 4 bytes
    
    qint64 m_totalVolume;       // 8 bytes
    int m_tradeCount;           // 4 bytes
    
    // Bit-packed flags (1 byte total)
    quint8 m_isRealTime : 1;
    quint8 m_isEndOfDay : 1;
    quint8 m_isPrevious : 1;
    // ... other flags
};
```

**Memory Optimization**:
- Before: ~152 bytes per bar
- After: ~104 bytes per bar  
- **32% reduction**
- Impact: For 10,000 bars, saves ~480 KB per symbol

### Bar Status Flags

Encoded as bitfield in single byte:
- `isRealTime`: Bar from live stream vs historical API
- `isEndOfDay`: Day's final bar
- `isPrevious`: Previous day's bar
- Additional flags defined in `Src/Misc/CONSTANTS.h` under `BarFlags`

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
    trade_count INTEGER NOT NULL,
    flags INTEGER NOT NULL         -- Bitfield status flags
);
```

### Indexes

```sql
CREATE INDEX IF NOT EXISTS idx_timestamp ON bars(timestamp);
```

**Database Location**:
- Pattern: `~/.local/share/L2Trader/bars/{symbol}_{timeframe}.db`
- Example: `AAPL_1Min.db`, `MSFT_5Min.db`
- One database per symbol-timeframe combination

### SQL Queries

All SQL defined in `Src/SQL/` headers:
- `DatabaseThreadQueries.h`: Schema creation, CRUD operations
- `LiveStreamDBQueries.h`: Stream persistence
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

## Stream Integration

### Live Bar Storage

When streaming live bars:

```cpp
class BarCache {
    QPointer<StreamBars> m_stream;  // Auto-null when stream closes
    std::unique_ptr<LiveStreamDB> m_liveStreamDB;  // Persists stream data
    
    void startStreaming(const QString& symbol, const QString& interval) {
        // Create stream
        m_stream = TSClient::getInstance().createStreamBars(symbol, interval);
        
        // Create persistence
        m_liveStreamDB = std::make_unique<LiveStreamDB>(symbol, interval);
        
        // Connect signals
        connect(m_stream, &StreamBars::barReceived, this, 
                [this](const Bar& bar) {
                    addBar(bar);                  // Add to memory cache
                    m_liveStreamDB->storeBar(bar); // Persist to database
                });
    }
};
```

### Stream Lifecycle

1. **Start**: Create StreamBars, connect signals
2. **Running**: Receive bars, store in cache + DB
3. **Stop**: Close stream, flush pending writes
4. **Cleanup**: QPointer auto-nulls on stream deletion

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

### Stream Errors

```cpp
connect(m_stream, &StreamBars::errorOccurred, this, 
        [this](const QString& error) {
            qCWarning() << "Stream error:" << error;
            // Attempt reconnection or notify user
        });
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
- Stream → Cache → Database
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
// In StockInstruments or similar
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
    // Not in cache, request from API
    TSClient::getInstance().getBars(symbol, interval, startDate, endDate,
        [this](bool success, const QJsonDocument& response) {
            if (success) {
                QVector<Bar> apiBars = parseBars(response);
                m_barCache.mergeBarsIntoCache(apiBars);
            }
        });
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
- `BarCache.stream` - Stream integration

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
- `../../../Clients/TSClient/AGENTS.md`: API for fetching bars
- `../../../SQL/AGENTS.md`: SQL query definitions

## Related Documentation

- `Doc/ARCHITECTURE.md`: Bar caching in system context
- `Doc/DEVELOPMENT.md`: Development guidelines
