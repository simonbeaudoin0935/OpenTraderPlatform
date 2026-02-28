# Core/ Directory - Agent Instructions

The Core directory contains the central application components that orchestrate the entire L2Trader system.

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
    FrontEnd* m_appFrontend;             // GUI or TUI instance
    MemoryMonitor* m_memoryMonitor;      // Resource tracking
};
```

**Startup Sequence**:
1. Construct MainApp instance
2. Get TSClient and MainAlgo singletons
3. Create appropriate FrontEnd (GUI/TUI based on build config)
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

**Role**: Order history persistence to SQLite

**Features**:
- Stores all orders (filled, canceled, rejected)
- Query by date range, symbol, account
- Automatic schema creation
- Thread-safe operations

**Schema**:
```sql
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
);
```

**Key Methods**:
```cpp
bool insertOrder(const Order& order);
bool updateOrder(const Order& order);
QVector<Order> getOrdersByDateRange(const QDate& start, const QDate& end);
QVector<Order> getOrdersBySymbol(const QString& symbol);
bool deleteOrder(const QString& orderId);
```

**SQL Queries**: All defined in `Src/SQL/OrdersDatabaseQueries.h`

**Thread Safety**: Runs on calling thread, caller responsible for thread management

### Cache/ Subdirectory

Contains data caching subsystems.

**BarCache/** - Historical and live bar data caching
- See `Cache/BarCache/AGENTS.md` for detailed information
- In-memory + SQLite two-tier cache
- Per-symbol, per-timeframe caching
- Automatic preloading from database
- Stream management for live data

**Key Features**:
- Thread-safe via QReadWriteLock
- Memory optimization (Bar class ~104 bytes)
- Efficient historical data retrieval
- Automatic database persistence
- Gap detection and filling

### Replay/ Subdirectory

**Role**: Market data replay for backtesting and analysis

**Components**:
- ReplayEngine: Controls replay playback, timing, and speed
- ReplayDataLoader: Loads historical data from SQLite databases (legacy format)
- OrderEmulator: Simulates order fills based on recorded market depth
- See `Replay/AGENTS.md` for detailed replay architecture and workflow

> **Note**: The replay system will be substantially revamped in Phase 7 of the
> Databento migration. Databento's `DbnFileStore` provides native `.dbn` replay,
> making ReplayEngine and ReplayDataLoader obsolete. OrderEmulator will be kept.

**Features**:
- Load data from CSV or database
- Control playback speed (1x, 2x, 5x, 10x, etc.)

## Architectural Patterns

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

MainAlgo ─[displayedStockReceivedNewBar]→ FrontEnd
         ─[receivedNewPosition]→ FrontEnd
         ─[tradeStationAccountsReceived]→ FrontEnd

MemoryMonitor ─[memoryUsageUpdated]→ FrontEnd

FrontEnd ─[selectedDisplayedStock]→ MainAlgo
```

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
- Linux: `~/.config/L2Trader/L2Trader.conf`
- Organization: "L2Trader"
- Application: "L2Trader"

### Database Locations and Mode-Based Organization

L2Trader uses three distinct trading modes, each with its own database storage:

| Mode | Endpoint | Database Path | Description |
|------|----------|---------------|-------------|
| **Live** | `api.tradestation.com` | `~/.cache/L2Trader/Orders/Live/Orders.db` | Real money trading |
| **Simulation** | `sim-api.tradestation.com` | `~/.cache/L2Trader/Orders/Simulation/Orders.db` | Paper trading via real API |
| **Replay** | (None - local emulation) | `~/.cache/L2Trader/Orders/Replay/Orders_YYYY-MM-DD_HHMMSS.db` | Historical playback |

**Important Distinction**:
- **Simulation** uses the real TradeStation sim API endpoint (`sim-api.tradestation.com`) - orders go through real network requests, just to a paper trading account
- **Replay** uses completely local emulation via `OrderEmulator` - no network requests, orders are processed by the emulator based on recorded market depth

**Directory Structure**:
```
~/.cache/L2Trader/
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
- **Bar Cache Databases**: `~/.local/share/L2Trader/bars/<symbol>_<timeframe>.db`
- **Replay Data**: User-specified paths (recorded market data)

## Build Configuration

### Conditional Compilation

Frontend selection:
```cpp
#ifdef GUI_ENABLED
    m_appFrontend = new GUIFrontend(&m_mainAlgo);
#else
    m_appFrontend = new TUIFrontend(&m_mainAlgo);
#endif
```

Build flag set in CMake:
```cmake
option(ENABLE_GUI "Enable GUI frontend" ON)
if(ENABLE_GUI)
    target_compile_definitions(L2Trader PRIVATE GUI_ENABLED)
endif()
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
