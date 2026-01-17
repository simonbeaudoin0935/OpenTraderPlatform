# BarCache Class Architecture

## Overview

The BarCache class provides efficient caching of 1-minute stock bars using a day-based storage approach. Each trading day (6:01am-8:00pm ET) is stored in a separate QVector, with bars indexed by their minute offset. Database operations are handled asynchronously via a dedicated `DatabaseThread` singleton.

## Key Design Principles

1. **Day-Based Storage**: Uses `QMap<QDate, QVector<Bar>>` where each QVector represents one complete trading day
2. **Complete Day Rule**: If a day exists in the cache, ALL bars for that day are available. The exception is for the current day; for past days, if the date is in the QMap, it means the QVector of bars for that day will contain all 840 minute bars. Even for minutes where no trades happened, there will still be a 'null' bar for that minute. This is a design choice, so that an algorithm can iterate through all 840 bars and know it's stepping one minute at a time, and take action on whether or not the minute had any trades.
3. **Index-Based Access**: Each bar's position in the vector corresponds to its minute offset (index 0 = 6:01am bar, index 839 = 8:00pm bar)
4. **Pre-Allocated Vectors**: Day vectors are pre-allocated to 840 bars for efficiency
5. **TradeStation Timestamp Quirk**: TradeStation timestamps minute bars at the **end** of the minute interval, not the beginning. This means:
   - The first tradeable minute (06:00:00 to 06:00:59) is timestamped as **06:01** — there is no bar with timestamp 06:00
   - The last bar of the day IS timestamped **20:00** (representing the 19:59:00 to 19:59:59 interval)
   - Therefore, valid bar timestamps range from 06:01 to 20:00 inclusive (840 bars total)
6. **Asynchronous Database Operations**: All database I/O is performed on a dedicated `DatabaseThread` to avoid blocking the main thread
7. **Thread-Safe Access**: Memory cache is protected by `QReadWriteLock` for concurrent access


## Class Diagram

```mermaid
classDiagram
    class BarCache {
        -QString m_symbol
        -bool m_isStreaming
        -QString m_dbPath
        -QPointer~StreamBars~ m_stream
        -QReadWriteLock m_barCacheRwLock
        -QMap~QDate, QVector~Bar~~ m_barCacheByDay

        +BarCache(QString symbol, bool isStreaming, QObject* parent)
        +~BarCache()
        +const QString& getSymbol() const
        +GetBarsResult_t getBars(QDate day, QTime first, QTime last)
        +void clearDatabase()

        -void startStream()
        -static size_t timeToIndex(QTime time)
        -static QTime indexToTime(size_t index)
        -QVector~Bar~& getOrCreateDayVector(QDate date)
        -optional~unique_ptr~QVector~Bar~~~ getBarsFromCache(QDate, QTime, QTime) const
        -void storeBarInCache(const Bar& bar)
        -void storeBarsInCache(QDate, shared_ptr~QVector~Bar~~)
        -QVector~Bar~ fillHolesOfReceivedRequest(QDateTime, QDateTime, QVector~Bar~) const
        -void onReceivedNewLiveBar(Bar newBar)

        +signals receivedNewBar(QString symbol, Bar newBar)

        +TRADING_START_TIME$ QTime
        +TRADING_END_TIME$ QTime
        +BARS_PER_DAY$ unsigned int
    }

    class Bar {
        -QDateTime m_timeStamp
        -float m_open
        -float m_high
        -float m_low
        -float m_close
        -quint64 m_totalVolume
        -quint8 m_flags

        +QDateTime getTimeStamp() const
        +float getOpen() const
        +float getHigh() const
        +float getLow() const
        +float getClose() const
        +quint64 getTotalVolume() const
        +BarStatus getBarStatus() const
        +bool getIsRealtime() const
        +static Bar nullBar(QDateTime timestamp)
    }

    class BarStatus {
        <<enumeration>>
        Uninitialized
        Null
        Open
        Closed
    }

    class StreamBars {
        +QFuture~optional~QString~~ future()
        +signals newBarReceived(Bar newBar)
    }

    class DatabaseThread {
        <<singleton>>
        +static DatabaseThread* getInstance()
        +void start()
        +QFuture~bool~ openDatabase(QString symbol, QString dbPath)
        +void closeDatabase(QString symbol)
        +QFuture~optional~unique_ptr~QVector~Bar~~~~ getBarsFromDatabase(QString, QDate, QTime, QTime)
        +QFuture~int~ storeBarsInDatabase(QString, QDate, shared_ptr~QVector~Bar~~)
        +QFuture~bool~ clearDatabase(QString symbol)
    }

    class TSClient {
        <<singleton>>
        +static TSClient* getInstance()
        +QFuture~expected~unique_ptr~QVector~Bar~~, Error~~ getBars(...)
        +StreamBars* openStreamBars(...)
        +void closeStream(StreamBars* stream)
    }

    BarCache --> Bar : contains
    BarCache --> StreamBars : manages
    BarCache --> DatabaseThread : uses
    BarCache --> TSClient : uses
    Bar --> BarStatus : has
```

## Database Schema

The database file is named `bars_cache_{symbol}.db` and is managed entirely by `DatabaseThread`.

```mermaid
erDiagram
    BARS {
        INTEGER timestamp PK "Unix epoch seconds"
        REAL open
        REAL high
        REAL low
        REAL close
        INTEGER volume
        INTEGER status "BarStatus enum value"
    }
```

## Sequence Diagram - getBars() Method

```mermaid
sequenceDiagram
    participant Client
    participant BarCache
    participant MemoryCache
    participant DatabaseThread
    participant TSClient

    Client->>BarCache: getBars(date, first, last)
    Note over BarCache: Assert: weekday, valid hours,<br/>not future date/time
    
    BarCache->>MemoryCache: getBarsFromCache(date, first, last)
    
    alt Day exists and range is initialized
        MemoryCache-->>BarCache: unique_ptr<QVector<Bar>>
        BarCache-->>Client: Return bars immediately (variant holds shared_ptr)
    else Cache miss (day missing or uninitialized bars)
        MemoryCache-->>BarCache: std::nullopt
        
        BarCache->>DatabaseThread: getBarsFromDatabase(symbol, date, first, last)
        Note over BarCache: Returns QFuture - async
        
        DatabaseThread-->>BarCache: optional<unique_ptr<QVector<Bar>>>
        
        alt Database has complete range
            BarCache->>MemoryCache: storeBarsInCache(date, bars)
            BarCache-->>Client: Resolve QFuture with bars
        else Database miss
            Note over BarCache: Determine fetch range:<br/>Past day: 6:01am-8:00pm<br/>Current day: 6:01am to now
            
            BarCache->>TSClient: getBars(symbol, startDateTime, endDateTime)
            TSClient-->>BarCache: expected<unique_ptr<QVector<Bar>>, Error>
            
            BarCache->>BarCache: fillHolesOfReceivedRequest()
            BarCache->>MemoryCache: storeBarsInCache(date, bars)
            BarCache->>DatabaseThread: storeBarsInDatabase(symbol, date, bars)
            Note over DatabaseThread: Fire-and-forget async storage
            
            BarCache-->>Client: Resolve QFuture with bars
        end
    end
```

## Flowchart - getBars() Logic

```mermaid
flowchart TD
    START(["getBars(date, first, last)"]) --> ASSERT["Assert preconditions:<br/>• Weekday (Mon-Fri)<br/>• 6:01am ≤ time ≤ 8:00pm<br/>• Not future date/time<br/>• Seconds/ms are zero"]
    ASSERT --> CHECK_MEMORY["Check memory cache<br/>getBarsFromCache()"]
    
    CHECK_MEMORY -->|"Hit: all bars initialized"| RETURN_SYNC["Return shared_ptr<QVector<Bar>><br/>(synchronous)"]
    
    CHECK_MEMORY -->|"Miss"| CHECK_DB["Query DatabaseThread<br/>(async)"]
    
    CHECK_DB --> DB_RESULT{"Database<br/>has data?"}
    
    DB_RESULT -->|"Yes"| STORE_MEMORY["Store in memory cache"]
    STORE_MEMORY --> RESOLVE_DB["Resolve QFuture<br/>with bars"]
    
    DB_RESULT -->|"No"| DETERMINE_RANGE{"Current day?"}
    
    DETERMINE_RANGE -->|"Yes"| FETCH_PARTIAL["Fetch API: 6:01am to now"]
    DETERMINE_RANGE -->|"No"| FETCH_FULL["Fetch API: 6:01am to 8:00pm"]
    
    FETCH_PARTIAL --> FILL_HOLES["fillHolesOfReceivedRequest()<br/>Insert null bars for gaps"]
    FETCH_FULL --> FILL_HOLES
    
    FILL_HOLES --> STORE_BOTH["Store in:<br/>• Memory cache (sync)<br/>• Database (async)"]
    STORE_BOTH --> RESOLVE_API["Resolve QFuture<br/>with bars"]
    
    RETURN_SYNC --> END(["End"])
    RESOLVE_DB --> END
    RESOLVE_API --> END
```

## State Diagram - BarCache Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Created: BarCache(symbol, isStreaming, parent)
    
    Created --> InitializingDB: Open database via DatabaseThread
    
    InitializingDB --> Ready: Database opened (async)
    InitializingDB --> StreamStarted: if isStreaming
    
    StreamStarted --> Ready: Stream connected
    
    Ready --> Fetching: getBars() cache miss
    Fetching --> Ready: Bars received & cached
    
    Ready --> Streaming: Live bar received
    Streaming --> Ready: Bar stored in cache
    
    Ready --> StreamReconnecting: Stream error
    StreamReconnecting --> Ready: Stream reconnected
    
    Ready --> [*]: ~BarCache()

    note right of Created
        Sets up database path:
        {cacheLocation}/bars_cache_{symbol}.db
    end note

    note right of StreamStarted
        Connects to TSClient::openStreamBars()
        with auto-reconnect on failure
    end note
```

## Component Diagram

```mermaid
graph TB
    subgraph "BarCache System"
        BC[BarCache]
        MC["Memory Cache<br/>(QMap&lt;QDate, QVector&lt;Bar&gt;&gt;)<br/>Protected by QReadWriteLock"]
        SB[StreamBars]
    end
    
    subgraph "Database Layer"
        DBT["DatabaseThread<br/>(Singleton)"]
        DB[("SQLite Database<br/>bars_cache_{symbol}.db")]
    end

    subgraph "External API"
        TSC["TSClient<br/>(Singleton)"]
        API["TradeStation API"]
    end

    BC --> MC
    BC --> DBT
    BC --> SB
    DBT --> DB
    SB --> TSC
    TSC --> API

    BC -.->|"1. Check memory cache"| MC
    BC -.->|"2. Query database (async)"| DBT
    BC -.->|"3. Fetch from API (async)"| TSC
    BC -.->|"Store bars (async)"| DBT
    BC -.->|"Cache bars"| MC
    SB -.->|"Stream live bars"| BC
```

## Storage Architecture

### Memory Cache Structure
- **Container**: `QMap<QDate, QVector<Bar>>`
- **Key**: Trading date (QDate)
- **Value**: Vector of exactly 840 bars (6:01am to 8:00pm inclusive)
- **Thread Safety**: Protected by `mutable QReadWriteLock m_barCacheRwLock`
- **Index Mapping**: `index = (hour - 6) * 60 + minute - 1`
  - Index 0 = 6:01am bar
  - Index 1 = 6:02am bar
  - ...
  - Index 509 = 2:30pm bar
  - Index 839 = 8:00pm bar

### Complete Day Rule
- **Invariant**: If a QDate exists in the map, the QVector is pre-allocated to 840 bars
- **Uninitialized Detection**: Bars with `BarStatus::Uninitialized` indicate missing data (cache miss)
- **Exception**: Current day (streaming) may have partial data from 6:01am to now
- **Benefit**: Simplifies cache logic - no need to track partial day states

### Index Calculation (Current Implementation)
```cpp
// Constants
static inline const QTime TRADING_START_TIME = QTime(6, 1);  // 6:01 AM ET
static inline const QTime TRADING_END_TIME = QTime(20, 0);   // 8:00 PM ET
static constexpr unsigned int BARS_PER_DAY = 840;

// Convert time to vector index (0-839)
size_t BarCache::timeToIndex(const QTime& time)
{
    // time must be between 6:01 AM and 8:00 PM inclusive
    size_t minutesSince6AM = (time.hour() - 6) * 60 + time.minute();
    size_t index = minutesSince6AM - 1;  // -1 because first bar is 6:01, not 6:00
    return index;  // Range: 0 to 839
}

// Convert index back to bar timestamp
QTime BarCache::indexToTime(size_t index)
{
    size_t adjustedMinutes = index + 1;  // +1 to offset back
    int hour = 6 + (adjustedMinutes / 60);
    int minute = adjustedMinutes % 60;
    return QTime(hour, minute, 0);
}

// Examples:
// timeToIndex(QTime(6, 1))   → 0    (first bar)
// timeToIndex(QTime(14, 30)) → 509  (2:30 PM bar)
// timeToIndex(QTime(20, 0))  → 839  (last bar)
```

### Return Type: GetBarsResult_t

The `getBars()` method returns a variant that can hold either:
1. **Synchronous result**: `std::shared_ptr<QVector<Bar>>` - when data is in memory cache
2. **Asynchronous result**: `QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>` - when data must be fetched from database or API

```cpp
typedef std::variant<
    std::shared_ptr<QVector<Bar>>,
    QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>
> GetBarsResult_t;
```

### Bar Status States
- **Uninitialized**: Default state, bar has never been set (cache miss indicator)
- **Null**: Bar was set but represents a minute with no trading activity
- **Open**: Live bar still being updated (current minute)
- **Closed**: Complete bar, no more updates expected