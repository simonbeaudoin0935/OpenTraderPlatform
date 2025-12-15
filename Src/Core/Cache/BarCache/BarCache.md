# BarCache Class Architecture

## Class Diagram

```mermaid
classDiagram
    class BarCache {
        -QString symbol
        -bool isStreaming
        -QMap<QDateTime, Bar> barCacheOneMinute
        -QReadWriteLock rwLock
        -QSqlDatabase db
        -StreamBars* streamBar
        -HitType lastHitType
        -quint64 duplicateStoreCount
        -quint64 lastNumberFetchedBars

        +BarCache(QString symbol, bool isStreaming, QObject* parent)
        +~BarCache()
        +QString getSymbol() const
        +unsigned int getNumberOfBars() const
        +bool warmUpBarsOfDayUntilNow(QDate)
        +QVector<Bar> getBars(QDateTime first, QDateTime last)
        +QVector<Bar> getAfterHourBars(QDate date)
        +HitType getLastHitType() const
        +qsizetype getDuplicateStoreCount() const
        +qsizetype getLastNumberFetchedBars() const
        +void clearDatabase()

        -void storeBarInCache(const Bar& bar)
        -void storeBarsInCache(const QVector<Bar>& bars)
        -QVector<Bar> getBarsFromCache(QDateTime start, QDateTime end) const
        -QVector<Bar> getBarsFromDatabase(QDateTime start, QDateTime end) const
        -void storeBarsInDatabase(const QVector<Bar>& bars)
        -QVector<QPair<QDateTime, QDateTime>> identifyMissingRanges(QDateTime start, QDateTime end, const QVector<Bar>& cachedBars) const
        -void onReceivedNewBar(QString symbol, Bar newBar)

        #signals
        +receivedNewBar(QString symbol, Bar newBar)
    }

    class HitType {
        <<enumeration>>
        None
        Hit        // Complete hit: all bars found in memory or database (no API call needed)
        Miss       // Complete miss: all bars fetched from API (none in memory or database)
        PartialHit // Partial hit: some bars in memory or database, rest fetched from API
    }

    class Bar {
        -QDateTime timestamp
        -double open
        -double high
        -double low
        -double close
        -qint64 volume
        -BarStatus status

        +QDateTime getTimeStamp() const
        +double getOpen() const
        +double getHigh() const
        +double getLow() const
        +double getClose() const
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

## Sequence Diagram - getBars() Method

```mermaid
sequenceDiagram
    participant Client
    participant BarCache
    participant MemoryCache
    participant Database
    participant TSClient

    Client->>BarCache: getBars(first, last)
    
    BarCache->>MemoryCache: getBarsFromCache(first, last)
    MemoryCache-->>BarCache: cachedBars[]
    
    alt Complete HIT from Memory
        BarCache->>BarCache: cachedBars.size() == expected
        BarCache-->>Client: cachedBars (HIT)
    else Check Database for Gaps
        BarCache->>BarCache: identifyMissingRanges()
        BarCache->>Database: getBarsFromDatabase(missing ranges)
        Database-->>BarCache: dbBars[]
        BarCache->>MemoryCache: storeBarsInCache(dbBars)
        
        BarCache->>MemoryCache: getBarsFromCache(first, last)
        MemoryCache-->>BarCache: updatedCachedBars[]
        
        alt Complete HIT after DB load
            BarCache->>BarCache: updatedCachedBars.size() == expected
            BarCache-->>Client: updatedCachedBars (HIT)
        else Fetch from API
            BarCache->>BarCache: identifyMissingRanges()
            BarCache->>TSClient: getBarsSync(missing ranges)
            TSClient-->>BarCache: fetchedBars[]
            BarCache->>BarCache: process fetchedBars (fill holes)
            BarCache->>MemoryCache: storeBarsInCache(resultBars)
            BarCache-->>Client: resultBars (MISS or PARTIAL HIT)
        end
    end
```

## Flowchart - Cache Hit Logic

```mermaid
flowchart TD
    START(["getBars(first, last)"]) --> CHECK_MEMORY["Check Memory Cache"]
    CHECK_MEMORY --> COMPLETE_MEMORY{"Complete Set?"}

    COMPLETE_MEMORY -->|"Yes"| HIT["Return HIT"]
    COMPLETE_MEMORY -->|"No"| IDENTIFY_GAPS["Identify Missing Ranges"]
    IDENTIFY_GAPS --> LOAD_DB["Load Missing Ranges from Database"]
    LOAD_DB --> CHECK_MEMORY_AGAIN["Check Memory Cache Again"]
    CHECK_MEMORY_AGAIN --> COMPLETE_AFTER_DB{"Complete Set?"}

    COMPLETE_AFTER_DB -->|"Yes"| HIT_DB["Return HIT"]
    COMPLETE_AFTER_DB -->|"No"| HAD_BARS_BEFORE_API{"Had Any Bars Before API?"}

    HAD_BARS_BEFORE_API -->|"No"| FETCH_ALL["Fetch All Missing Ranges from API"]
    HAD_BARS_BEFORE_API -->|"Yes"| FETCH_GAPS["Fetch Missing Ranges from API"]

    FETCH_ALL --> PROCESS_API["Process API Response (fill holes)"]
    FETCH_GAPS --> PROCESS_API

    PROCESS_API --> STORE_CACHE["Store in Cache + DB"]
    STORE_CACHE --> RESULT_TYPE{"Had Bars Before API?"}
    RESULT_TYPE -->|"No"| MISS["Return MISS"]
    RESULT_TYPE -->|"Yes"| PARTIAL["Return PARTIAL HIT"]

    HIT --> END(["End"])
    HIT_DB --> END
    MISS --> END
    PARTIAL --> END
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
        MC["Memory Cache (QMap<QDateTime, Bar>)"]
        DB[("SQLite Database (bars_cache_{symbol}.db)")]
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

    BC -.->|"1. Check Memory Cache"| MC
    BC -.->|"2. Load gaps from DB"| DB
    BC -.->|"3. Fetch missing from API"| TSC
    BC -.->|"Store bars"| DB
    BC -.->|"Cache bars"| MC
    SB -.->|"Stream bars"| BC
```