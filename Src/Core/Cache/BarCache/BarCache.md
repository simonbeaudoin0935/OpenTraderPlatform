# BarCache Class Architecture

## Overview

The BarCache class provides efficient caching of 1-minute stock bars using a day-based storage approach. Each trading day (6am-8pm ET) is stored in a separate QVector, with bars indexed by their minute offset from 6am.

## Key Design Principles

1. **Day-Based Storage**: Uses `QMap<QDate, QVector<Bar>>` where each QVector represents one complete trading day
2. **Complete Day Rule**: If a day exists in the cache, ALL bars for that day are available (except for current day streaming)
3. **Index-Based Access**: Each bar's position in the vector corresponds to its minute offset from 6am (index 0 = 6:00am, index 840 = 8:00pm)
4. **Pre-Allocated Vectors**: Day vectors are pre-allocated to 841 bars (6:00am to 8:00pm inclusive) for efficiency

## Class Diagram

```mermaid
classDiagram
    class BarCache {
        -QString symbol
        -bool isStreaming
        -QMap<QDate, QVector<Bar>> barCacheByDay
        -QReadWriteLock barCacheRwLock
        -QSqlDatabase db
        -StreamBars* streamBar
        -quint64 duplicateStoreCount

        +BarCache(QString symbol, bool isStreaming, QObject* parent)
        +~BarCache()
        +QString getSymbol() const
        +unsigned int getNumberOfBars() const
        +bool warmUpBarsOfDayUntilNow(QDate)
        +GetBarsResult_t getBars(QDateTime first, QDateTime last)
        +qsizetype getDuplicateStoreCount() const
        +void clearDatabase()

        -void storeBarInCache(const Bar& bar)
        -void storeBarsInCache(const QVector<Bar>& bars)
        -QVector<Bar> getBarsFromCache(QDateTime start, QDateTime end) const
        -QVector<Bar> getBarsFromDatabase(QDateTime start, QDateTime end) const
        -void storeBarsInDatabase(const QVector<Bar>& bars)
        -size_t timeToIndex(QTime time)
        -QTime indexToTime(size_t index)
        -QVector<Bar>& getOrCreateDayVector(QDate date)
        -const QVector<Bar>* getDayVector(QDate date) const
        -void onReceivedNewBar(QString symbol, Bar newBar)

        #signals
        +receivedNewBar(QString symbol, Bar newBar)
    }

    class Bar {
        -QDateTime timestamp
        -float open
        -float high
        -float low
        -float close
        -qint64 volume
        -BarStatus status

        +QDateTime getTimeStamp() const
        +float getOpen() const
        +float getHigh() const
        +float getLow() const
        +float getClose() const
        +qint64 getTotalVolume() const
        +BarStatus getBarStatus() const
        +bool getIsRealtime() const
        +static Bar nullBar(QDateTime timestamp)
    }

    class StreamBars {
        +signals
        +receivedNewBar(QString symbol, Bar newBar)
    }

    class TSClient {
        +static TSClient& getInstance()
        +bool getBarsSync(QVector<Bar>& bars, QString symbol, int interval, BarUnit unit, int sessions, BarSessionTemplate template, QDateTime start, QDateTime end)
        +StreamBars* openStreamBars(QString symbol, int interval, BarUnit unit, int sessions, BarSessionTemplate template)
        +void closeStreamBars(StreamBars* stream)
    }

    BarCache --> HitType : uses
    BarCache --> Bar : contains
    BarCache --> StreamBars : manages
    BarCache --> TSClient : uses
    BarCache --> QSqlDatabase : uses
    BarCache --> QReadWriteLock : uses
    BarCache --> QMap : uses
```

## Database Schema

```mermaid
erDiagram
    BARS {
        INTEGER timestamp PK
        REAL open
        REAL high
        REAL low
        REAL close
        INTEGER volume
    }
```

## Sequence Diagram - getBarsInRange() Method (New Complete Day Logic)

```mermaid
sequenceDiagram
    participant Client
    participant BarCache
    participant MemoryCache
    participant Database
    participant TSClient

    Client->>BarCache: getBarsInRange(first, last)
    Note over BarCache: Assert single-day range
    
    BarCache->>MemoryCache: Check if day exists in cache
    
    alt Day exists in memory cache
        MemoryCache-->>BarCache: Day vector found
        BarCache->>BarCache: Extract requested range from day vector
        BarCache-->>Client: Return bars (Complete HIT)
    else Day not in memory
        BarCache->>Database: getBarsFromDatabase(6am-8pm for day)
        Database-->>BarCache: dbBars[]
        
        alt Complete day in database
            BarCache->>MemoryCache: storeBarsInCache(complete day)
            BarCache->>BarCache: Extract requested range
            BarCache-->>Client: Return bars (Database HIT)
        else Incomplete/Missing day in database
            BarCache->>TSClient: Fetch complete day from API (6am-8pm)
            TSClient-->>BarCache: Complete day bars[]
            BarCache->>BarCache: fillHolesOfReceivedRequest()
            BarCache->>MemoryCache: storeBarsInCache(complete day)
            BarCache->>Database: storeBarsInDatabase(complete day)
            BarCache-->>Client: Return QFuture (API fetch in progress)
        end
    end
```

## Flowchart - New Complete Day Cache Logic

```mermaid
flowchart TD
    START(["getBarsInRange(first, last)"]) --> ASSERT["Assert: Single day range<br/>Trading hours: 6am-8pm"]
    ASSERT --> CHECK_DAY["Check if day exists<br/>in memory cache"]
    
    CHECK_DAY -->|"Day exists"| EXTRACT_MEMORY["Extract requested range<br/>from day vector"]
    EXTRACT_MEMORY --> HIT["Return Complete HIT"]
    
    CHECK_DAY -->|"Day missing"| CHECK_DB["Query database for<br/>complete day (6am-8pm)"]
    CHECK_DB --> DB_COMPLETE{"Complete day<br/>in database?"}
    
    DB_COMPLETE -->|"Yes"| LOAD_DAY["Load complete day<br/>into memory cache"]
    LOAD_DAY --> EXTRACT_DB["Extract requested range"]
    EXTRACT_DB --> DB_HIT["Return Database HIT"]
    
    DB_COMPLETE -->|"No"| IS_TODAY{"Is this<br/>current day?"}
    IS_TODAY -->|"Yes"| FETCH_PARTIAL["Fetch API: 6am to now"]
    IS_TODAY -->|"No"| FETCH_FULL["Fetch API: 6am to 8pm"]
    
    FETCH_PARTIAL --> FILL_HOLES["Fill holes with null bars"]
    FETCH_FULL --> FILL_HOLES
    
    FILL_HOLES --> STORE_BOTH["Store complete day in:<br/>- Memory cache<br/>- Database"]
    STORE_BOTH --> ASYNC["Return QFuture<br/>(async fetch)"]
    
    HIT --> END(["End"])
    DB_HIT --> END
    ASYNC --> END
```

## State Diagram - BarCache Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Created: BarCache(symbol, isStreaming)
    Created --> DatabaseInitialized: Setup SQLite DB
    DatabaseInitialized --> StreamInitialized: if isStreaming

    Created --> DatabaseInitialized
    DatabaseInitialized --> Ready

    Ready --> Fetching: getBars() called
    Fetching --> Ready: Bars returned

    Ready --> Streaming: Real-time bars received
    Streaming --> Ready: Bars stored

    Ready --> [*]: ~BarCache()

    note right of DatabaseInitialized
        Creates bars_cache_{symbol}.db
        Creates bars table if needed
    end note

    note right of StreamInitialized
        Connects to TSClient stream
        Listens for new bars
    end note
```

## Component Diagram

```mermaid
graph TB
    subgraph "BarCache System"
        BC[BarCache]
        MC["Memory Cache<br/>(QMap&lt;QDate, QVector&lt;Bar&gt;&gt;)<br/>Day-based storage"]
        DB[("SQLite Database<br/>(bars_cache_{symbol}.db)")]
        SB[StreamBars]
    end

    subgraph "External Dependencies"
        TSC[TSClient]
        API["TradeStation API"]
    end

    BC --> MC
    BC --> DB
    BC --> SB
    SB --> TSC
    TSC --> API

    BC -.->|"1. Check if day exists"| MC
    BC -.->|"2. Load complete day from DB"| DB
    BC -.->|"3. Fetch complete day from API"| TSC
    BC -.->|"Store complete day"| DB
    BC -.->|"Cache complete day"| MC
    SB -.->|"Stream live bars"| BC
```

## Storage Architecture

### Memory Cache Structure
- **Container**: `QMap<QDate, QVector<Bar>>`
- **Key**: Trading date (QDate)
- **Value**: Vector of 841 bars (one per minute, 6:00am-8:00pm inclusive)
- **Index Mapping**: `index = (hour - 6) * 60 + minute`
  - Index 0 = 6:00am
  - Index 1 = 6:01am
  - ...
  - Index 839 = 7:59pm
  - Index 840 = 8:00pm

### Complete Day Rule
- **Invariant**: If a QDate exists in the map, ALL bars for that day must be present
- **Exception**: Current day (streaming) may have partial data from 6am to now
- **Benefit**: Simplifies cache logic - no need to track partial day states

### Index Calculation Example
```cpp
// Convert time to vector index
QTime time(14, 30, 0);  // 2:30 PM
size_t index = (14 - 6) * 60 + 30;  // = 8 * 60 + 30 = 510

// Convert index back to time
size_t index = 510;
int hour = 6 + (510 / 60);  // = 6 + 8 = 14
int minute = 510 % 60;       // = 30
// Result: 14:30 (2:30 PM)
```