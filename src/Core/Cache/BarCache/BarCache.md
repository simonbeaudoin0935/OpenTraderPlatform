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

        -void storeBarInCache(const Bar& bar)
        -void storeBarsInCache(const QVector<Bar>& bars)
        -QVector<Bar> getBarsFromCache(QDateTime start, QDateTime end) const
        -QVector<Bar> getBarsFromDatabase(QDateTime start, QDateTime end) const
        -void storeBarsInDatabase(const QVector<Bar>& bars)
        -void onReceivedNewBar(QString symbol, Bar newBar)

        #signals
        +receivedNewBar(QString symbol, Bar newBar)
    }

    class HitType {
        <<enumeration>>
        None
        Hit
        Miss
        PartialHit
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
        +void ajustTimeStampToOpeningMinute()
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
    participant Database
    participant MemoryCache
    participant TSClient

    Client->>BarCache: getBars(first, last)
    BarCache->>Database: getBarsFromDatabase(first, last)
    Database-->>BarCache: dbBars[]
    BarCache->>MemoryCache: storeBarsInCache(dbBars)

    BarCache->>MemoryCache: getBarsFromCache(first, last)
    MemoryCache-->>BarCache: cachedBars[]

    alt Complete HIT
        BarCache->>BarCache: cachedBars.size() == expected
        BarCache-->>Client: cachedBars (HIT)
    else Complete MISS
        BarCache->>TSClient: getBarsSync(symbol, first, last)
        TSClient-->>BarCache: fetchedBars[]
        BarCache->>BarCache: process fetchedBars (fill holes)
        BarCache->>MemoryCache: storeBarsInCache(resultBars)
        BarCache-->>Client: resultBars (MISS)
    else Partial HIT
        BarCache->>BarCache: identify gaps
        BarCache->>TSClient: fetch missing bars (before gap)
        TSClient-->>BarCache: preBars[]
        BarCache->>BarCache: process preBars
        BarCache->>MemoryCache: storeBarsInCache(preBars)

        BarCache->>TSClient: fetch missing bars (after gap)
        TSClient-->>BarCache: postBars[]
        BarCache->>BarCache: process postBars
        BarCache->>MemoryCache: storeBarsInCache(postBars)

        BarCache->>BarCache: combine all bars
        BarCache-->>Client: allBars (PARTIAL HIT)
    end
```

## Flowchart - Cache Hit Logic

```mermaid
flowchart TD
    A[getBars(first, last)] --> B[Load from Database]
    B --> C[Check Memory Cache]
    C --> D{Complete Set?}

    D -->|Yes| E[Return HIT]
    D -->|No| F{Any Cached Bars?}

    F -->|No| G[Fetch All from API]
    F -->|Yes| H[Identify Gaps]

    G --> I[Process API Response]
    I --> J[Store in Cache + DB]
    J --> K[Return MISS]

    H --> L[Fetch Missing Before]
    L --> M[Fetch Missing After]
    M --> N[Process All Responses]
    N --> O[Store in Cache + DB]
    O --> P[Return PARTIAL HIT]

    E --> Q[End]
    K --> Q
    P --> Q
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
        MC[Memory Cache<br/>QMap<QDateTime, Bar>]
        DB[(SQLite Database<br/>bars_cache_{symbol}.db)]
        SB[StreamBars]
    end

    subgraph "External Dependencies"
        TSC[TSClient]
        API[TradeStation API]
    end

    BC --> MC
    BC --> DB
    BC --> SB
    SB --> TSC
    TSC --> API

    BC -.->|Fetches bars| TSC
    BC -.->|Stores bars| DB
    BC -.->|Caches bars| MC
    SB -.->|Streams bars| BC
```