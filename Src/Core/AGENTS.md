# Core/ Directory - Agent Instructions

The Core directory contains the central application components that orchestrate the entire OpenTraderPlatform system.

## Overview

**Location**: `Src/Core/`
**Purpose**: Application orchestration, memory monitoring, data persistence, cache management, and replay functionality

## Key Components

### MainApp (MainApp.h/cpp)

**Role**: Application orchestrator and lifecycle manager

**Responsibilities**:
- Initialize and connect all core components (TSClient, MainAlgo, FrontEnd)
- Manage application lifecycle (startup, shutdown)
- Create signal/slot chains between components
- Coordinate memory monitoring
- Handle application-wide events

**Key Members**:
```cpp
class MainApp : public QObject {
    TSClient& m_tradeStationClient;      // Reference to singleton (brokerage only)
    MainAlgo& m_mainAlgo;                // Reference to singleton
    MemoryMonitor* m_memoryMonitor;      // Resource tracking
};
```

**Startup Sequence**:
1. Construct MainApp instance
2. Get TSClient and MainAlgo singletons
4. Create MemoryMonitor
5. Connect all signal/slot chains
6. Call start() to begin operations:
   - Start TSClient thread
   - Start MainAlgo thread
   - Start memory monitoring (500ms interval)
7. Enter Qt event loop

**Shutdown Sequence**:
1. User initiates quit (Ctrl+Q or window close)
2. Stop timers and streams
3. Quit worker threads (TSClient, MainAlgo)
4. Wait for threads with 5-second timeout
5. Terminate threads if needed (last resort)
6. Clean up resources in destructors
7. Exit Qt event loop

### MemoryMonitor (MemoryMonitor.h/cpp)

**Role**: System resource monitoring and reporting

**Monitoring**:
- Application memory usage (RSS - Resident Set Size)
- Update interval: Configurable (default 500ms)
- Reports via signal: `memoryUsageUpdated(qsizetype bytes)`

**Implementation**:
```cpp
class MemoryMonitor : public QObject {
    QTimer* m_timer;           // Polling timer
    qsizetype m_currentUsage;  // Last measured usage

public:
    void startMonitoring(int intervalMs = 500);
    void stopMonitoring();
    qsizetype getCurrentMemoryUsage() const;

signals:
    void memoryUsageUpdated(qsizetype bytes);
};
```

**Usage**:
- Frontend displays in status bar
- Can trigger warnings/actions on high usage
- Useful for detecting memory leaks during development

**Platform-Specific**:
- Linux: Reads `/proc/self/status` for VmRSS
- Cross-platform: Falls back to Qt memory tracking

### OrdersDatabase (OrdersDatabase.h/cpp)

**Role**: SQLite persistence for orders plus strategy chart artifacts

**Features**:
- Stores all orders (status, optional latency, serialized JSON payload)
- Stores strategy chart log markers (`strategy_logs`)
- Keeps legacy strategy bracket-overlay history table (`strategy_bracket_overlays`) for compatibility/migration
- Automatic schema creation for all three tables
- Symbol-scoped reload for chart logs when switching charts

**Schema**:
```sql
CREATE TABLE IF NOT EXISTS orders (
    ... order fields + strategy_log + json_data ...
);

CREATE TABLE IF NOT EXISTS strategy_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    strategy_id TEXT NOT NULL,
    symbol TEXT NOT NULL,
    timestamp TEXT NOT NULL,
    message TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS strategy_bracket_overlays (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    strategy_id TEXT NOT NULL,
    symbol TEXT NOT NULL,
    timestamp TEXT NOT NULL,
    action TEXT NOT NULL,          -- upsert / clear
    side TEXT,                     -- long / short (for upsert)
    stop_price REAL,
    take_price REAL,
    reference_entry_price REAL,
    triggered INTEGER NOT NULL DEFAULT 0,
    trigger_reason TEXT
);
```

**Key Methods**:
```cpp
bool insertOrder(const Order& order, std::optional<qint64> latencyMs = std::nullopt);
bool updateOrder(const Order& order, std::optional<qint64> latencyMs = std::nullopt);
QMap<QString, std::tuple<Order, std::optional<qint64>>> loadAllOrders() const;
bool clearAllOrders();

bool updateOrderStrategyLog(const QString& orderID, const QString& log);
bool insertStrategyLog(const StrategyLogEntry& entry);
QVector<StrategyLogEntry> loadStrategyLogs(const QString& symbol) const;

// Legacy compatibility APIs (managed brackets are now live state in MainAlgo)
bool insertStrategyBracketOverlay(const StrategyBracketOverlayEntry& entry);
QVector<StrategyBracketOverlayEntry> loadStrategyBracketOverlays(const QString& symbol) const;
```

`createTable()` performs a backward-compatible migration for old ledgers by adding
`reference_entry_price` via `ALTER TABLE` when absent.

**SQL Queries**: All defined in `Src/SQL/OrdersDatabaseQueries.h`

**Threading**: Runs on the caller's thread (single-connection SQLite object). Callers must ensure thread-affinity safety.

### PositionsDatabase (PositionsDatabase.h/cpp)

**Role**: SQLite persistence for streamed positions.

**Threading**:
- Owns a single SQLite connection on the thread that creates the singleton (normally MainAlgo thread).
- Public APIs are thread-safe entry points: calls from other threads are marshaled back to the owner thread via `QMetaObject::invokeMethod(..., Qt::BlockingQueuedConnection)`.
- This avoids cross-thread use of one `QSqlDatabase` connection while keeping synchronous call semantics for callers.

### RiskStateStore (RiskStateStore.h/cpp)

**Role**: SQLite persistence for risk-manager runtime state, event history, and config snapshots.

**Scope**:
- Binds to `LedgerPaths::currentLedgerDatabasePath()`
- Live/simulation: persists across app relaunch within the long-lived ledger
- Replay: session-scoped by replay ledger timestamp (fresh on replay restart)

**Schema ownership**:
- `risk_state`
- `risk_events`
- `risk_config_snapshot`

**SQL Queries**: `Src/SQL/RiskStateStoreQueries.h`

### Cache/ Subdirectory

Contains data caching subsystems.

**BarCache/** - Historical and live bar data caching
- See `Cache/BarCache/AGENTS.md` for detailed information
- In-memory + SQLite two-tier cache
- **Multi-timescale**: stores all 9 TimeFrames (1m through 1M) independently
- Memory structure: `QMap<TimeFrame, QMap<QDate, QVector<Bar>>>`
- SQLite schema v2 includes `timescale` column (auto-migrated from v1 on open)
- Automatic preloading from database

**Key Features**:
- Thread-safe via QReadWriteLock
- Memory optimization (Bar class ~104 bytes)
- All public methods accept a `TimeFrame tf` parameter
- Efficient historical data retrieval with gap detection and auto-fill
- Automatic database persistence via DatabaseThread

### Replay/ Subdirectory

**Role**: Market data replay for backtesting and analysis

**Components**:
- `ReplayEngine`: Controls replay playback using `databento::DbnFileStore` for `.dbn.zst` files
- `OrderEmulator`: Simulates order fills based on real-time Level 2 depth updates
- See `Replay/AGENTS.md` for detailed replay architecture and workflow

`ReplayDataLoader` was deleted as part of the Databento migration. `ReplayEngine` now reads directly from `~/.local/share/OpenTraderPlatform/ReplayData/{YYYY-MM-DD}/{SYMBOL}_mbp10.dbn.zst` and `{SYMBOL}_trades.dbn.zst`.

Replay restart semantics relevant to risk/order state:

- `MainApp::restartReplaySession(...)` rotates replay session timestamp and recreates replay-ledger backing stores
- replay order/position streams and risk runtime state are intentionally reset as a fresh session

### TradingSession Enum

Defined in `Src/Core/` (used by `MainApp` and `MarketCalendar`):

```cpp
enum class TradingSession {
    PreMarket,      // Before 9:30 AM ET
    Regular,        // 9:30 AM – 4:00 PM ET
    AfterHours,     // After 4:00 PM ET
    Weekend,        // Saturday / Sunday
    Holiday,        // NYSE full-day closure (see MarketCalendar::HOLIDAYS_2026)
};
```

`Weekend` and `Holiday` values allow the app to distinguish non-trading days and suppress bar-gap warnings on those days.

### Component Coordination

MainApp uses dependency injection pattern:
```cpp
MainApp::MainApp()
    : m_tradeStationClient(TSClient::getInstance())  // Get singleton
    , m_mainAlgo(MainAlgo::getInstance())            // Get singleton
    , m_appFrontend(nullptr)                         // Create based on build
    , m_memoryMonitor(new MemoryMonitor(this))       // Create and own
{
    // Inject dependencies and connect signals
}
```

### Signal/Slot Chains

MainApp creates signal chains to propagate data:

```
TSClient ─[authStateChanged]→ MainAlgo
        ─[authStateChanged]→ FrontEnd

MainAlgo ─[receivedNewPosition]→ FrontEnd
         ─[receivedNewOrder]→ FrontEnd
         ─[tradeStationAccountsReceived]→ FrontEnd

MemoryMonitor ─[memoryUsageUpdated]→ FrontEnd

FrontEnd ─[selectedDisplayedStock]→ MainAlgo
```

> **Note**: Market data (bars, Level 2, trades) is NOT wired through MainApp.
> MainAlgo writes snapshots into `DisplaySnapshot` (inside SymbolContext) and
> GUIFrontend reads them at 30 Hz via a pull timer. See `Src/FrontEnd/AGENTS.md`.

### Lifetime Management

- **MainApp**: Lives in main(), destroyed on app exit
- **Singletons** (TSClient, MainAlgo): Static lifetime
- **FrontEnd**: Owned by MainApp via raw pointer, Qt parent-child deletion
- **MemoryMonitor**: Owned by MainApp via raw pointer with parent
- **Threads**: Stack-allocated members in singletons

## Memory Management

### Smart Pointer Usage

```cpp
// ✓ Correct: Qt objects with parent (Qt manages)
m_memoryMonitor = new MemoryMonitor(this);  // 'this' is parent

// ✓ Correct: Non-Qt or no parent (use unique_ptr)
std::unique_ptr<OrdersDatabase> m_ordersDB;

// ✓ Correct: Shared across async operations
std::shared_ptr<QVector<Bar>> bars;
```

### Resource Cleanup

MainApp destructor ensures proper cleanup:
```cpp
MainApp::~MainApp() {
    // Stop monitoring
    if (m_memoryMonitor) {
        m_memoryMonitor->stopMonitoring();
    }

    // Close databases
    // Qt parent-child deletes m_memoryMonitor and m_appFrontend
}
```

## Threading Considerations

### Thread Ownership

- **MainApp**: Lives in main thread
- **MemoryMonitor**: Runs in main thread (timer-based)
- **OrdersDatabase**: No thread affinity (caller's thread)
- **ReplayEngine**: Can run in main or dedicated thread

### Thread Safety

- **MemoryMonitor**: Not thread-safe, single-threaded
- **OrdersDatabase**: Each instance for one thread
- **Cache components**: Thread-safe with locks

## Configuration

### Application Settings

MainApp reads settings on startup:
- Last window geometry (GUI mode)
- Last selected account
- Last displayed symbol
- Replay state (if resuming)

Settings stored via QSettings:
- Linux: `~/.config/OpenTraderPlatform/OpenTraderPlatform.conf`
- Organization: "OpenTraderPlatform"
- Application: "OpenTraderPlatform"

### Database Locations and Mode-Based Organization

OpenTraderPlatform uses three distinct trading modes, each with its own database storage:

| Mode | Endpoint | Database Path | Description |
|------|----------|---------------|-------------|
| **Live** | `api.tradestation.com` | `~/.local/share/OpenTraderPlatform/Orders/Live/Orders.db` | Real money trading |
| **Simulation** | `sim-api.tradestation.com` | `~/.local/share/OpenTraderPlatform/Orders/Simulation/Orders.db` | Paper trading via real API |
| **Replay** | (None - local emulation) | `~/.local/share/OpenTraderPlatform/Orders/Replay/Orders_YYYY-MM-DD_HHMMSS.db` | Historical playback |

**Important Distinction**:
- **Simulation** uses the real TradeStation sim API endpoint (`sim-api.tradestation.com`) - orders go through real network requests, just to a paper trading account
- **Replay** uses completely local emulation via `OrderEmulator` - no network requests, orders are processed by the emulator based on recorded market depth

**Directory Structure**:
```
~/.local/share/OpenTraderPlatform/
├── Orders/
│   ├── Live/
│   │   └── Orders.db              (single file, all live trading history)
│   ├── Simulation/
│   │   └── Orders.db              (single file, all paper trading history)
│   └── Replay/
│       ├── Orders_2026-02-14_143022.db
│       ├── Orders_2026-02-15_091533.db
│       └── ...                    (one per replay session)
└── Positions/
    ├── Live/
    │   └── Positions.db
    ├── Simulation/
    │   └── Positions.db
    └── Replay/
        ├── Positions_2026-02-14_143022.db
        └── ...
```

**Timestamp Format**: `YYYY-MM-DD_HHMMSS` (e.g., `2026-02-14_143022` for Feb 14, 2026 at 14:30:22)

**Other Database Locations**:
- **Bar Cache Databases**: `~/.local/share/OpenTraderPlatform/bars/<symbol>_<timeframe>.db`
- **Replay Data**: User-specified paths (recorded market data)

## Build Configuration

### Frontend Construction

MainApp constructs the GUI frontend directly:
```cpp
m_appFrontend = new GUIFrontend(m_mainAlgo);
```

## Error Handling

### Startup Failures

MainApp handles initialization errors:
```cpp
void MainApp::start() {
    if (!initializeComponents()) {
        qCCritical() << "Failed to initialize components";
        QCoreApplication::exit(1);
        return;
    }
    // Continue startup...
}
```

### Runtime Errors

- Component failures logged with appropriate severity
- Non-critical failures degrade gracefully
- Critical failures trigger application shutdown

## Testing Support

### Mock Components

For unit testing MainApp:
1. Mock TSClient and MainAlgo singletons (difficult)
2. Test FrontEnd and MemoryMonitor independently
3. Integration tests with real components

### Replay for Testing

ReplayEngine useful for:
- Backtesting strategies
- Reproducing bugs from historical data
- Performance testing with known datasets

## Performance Considerations

- **Memory Monitoring**: Low overhead, 500ms interval
- **Signal/Slot**: Cross-thread queuing is efficient
- **Database Operations**: Async preferred for UI responsiveness
- **Cache Access**: Optimized with read-write locks

## Common Patterns

### Adding New Component to MainApp

```cpp
// 1. Add member variable
class MainApp {
    NewComponent* m_newComponent;
};

// 2. Initialize in constructor
MainApp::MainApp() {
    m_newComponent = new NewComponent(this);  // Qt parent
}

// 3. Connect signals in connectSignals()
void MainApp::connectSignals() {
    connect(m_newComponent, &NewComponent::someSignal,
            this, &MainApp::handleSomeSignal);
}

// 4. Start in start() method
void MainApp::start() {
    m_newComponent->initialize();
}
```

### Accessing Core Components

From other parts of the application:
```cpp
// ✓ Via signals (preferred for cross-thread)
emit requestData();

// ✓ Via singletons (if same thread or thread-safe)
TSClient& client = TSClient::getInstance();

// ✗ Don't access MainApp directly (no singleton)
```

## Related Agent Instructions

- `Cache/BarCache/AGENTS.md`: Bar caching system details
- `Replay/AGENTS.md`: Market data replay architecture and workflow
- `../Algo/AGENTS.md`: MainAlgo coordination
- `../Clients/TSClient/AGENTS.md`: TSClient API communication
- `../FrontEnd/AGENTS.md`: FrontEnd interface implementations

## Related Documentation

- `Doc/ARCHITECTURE.md`: Complete system architecture
- `Doc/DEVELOPMENT.md`: Development setup and guidelines
