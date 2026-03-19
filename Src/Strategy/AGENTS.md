# Strategy/ Directory - External Strategy Runtime - Agent Instructions

The Strategy directory contains the host-side runtime for external strategy processes. Strategies are launched as child processes and communicate with the host over Unix domain sockets using framed Protobuf messages.

## Overview

**Location**: `Src/Strategy/`
**Purpose**: Extensible strategy framework with process isolation and host-side supervision
**Architecture**: External-process runtime with one supervised child process per strategy

For complete documentation, see `Doc/STRATEGY.md`.

## Core Components

### StrategyManager (StrategyManager.h/cpp)

**Role**: Orchestrates strategy lifecycle and manages multiple strategies

**Responsibilities**:
- Load strategy manifests / executables
- Create and supervise one process per strategy
- Start/stop strategies
- Handle crashes and errors
- Provide SDK access to strategies
- Aggregate logs and events

**Key Features**:
- Multiple strategies can run simultaneously
- Each strategy in an isolated process
- Crash in one strategy doesn't affect host memory
- Socket + Protobuf communication

**API**:
```cpp
class StrategyManager : public QObject {
public:
    static StrategyManager& getInstance();

    // Plugin management
    bool loadStrategy(const QString& pluginPath);
    void unloadStrategy(const QString& strategyId);
    void unloadAllStrategies();

    // Strategy control
    bool startStrategy(const QString& strategyId);
    void stopStrategy(const QString& strategyId);
    void stopAllStrategies();

    // Query
    QVector<QString> getLoadedStrategyIds() const;
    StrategyStatus getStrategyStatus(const QString& strategyId) const;

signals:
    // Strategy events
    void strategyLoaded(const QString& strategyId, const QString& name);
    void strategyUnloaded(const QString& strategyId);
    void strategyStarted(const QString& strategyId);
    void strategyStopped(const QString& strategyId);
    void strategyError(const QString& strategyId, const QString& error);
    void strategyCrashed(const QString& strategyId, const QString& reason);

    // Strategy outputs
    void strategyLog(const QString& strategyId, const QString& message);
    void strategyMetric(const QString& strategyId, const QString& name, double value);
};
```

### StrategyBase (StrategyBase.h)

**Role**: Abstract base class that all strategies must inherit from

**Lifecycle Methods**:
```cpp
class StrategyBase : public QObject {
    Q_OBJECT

public:
    explicit StrategyBase(StrategySDK* sdk, QObject* parent = nullptr);
    virtual ~StrategyBase() = default;

    // Lifecycle (pure virtual - must implement)
    virtual void onStart() = 0;
    virtual void onStop() = 0;

    // Metadata (pure virtual - must implement)
    virtual QString getName() const = 0;
    virtual QString getVersion() const = 0;
    virtual QString getDescription() const = 0;

    // Optional overrides
    virtual void onBar(const QString& symbol, const Bar& bar) {}
    virtual void onPosition(const Position& position) {}
    virtual void onOrder(const Order& order) {}
    virtual void onLevel2(const QString& symbol, const Level2& level2) {}
    virtual void onLevel1(const QString& symbol, const Level1& level1) {}
    virtual void onTrade(const QString& symbol, const Trade& trade) {}
    virtual void onTick(qint64 milliseconds) {}  // Called every second

signals:
    // Strategy can emit these signals
    void log(const QString& message);
    void metric(const QString& name, double value);
    void requestPlaceOrder(const PlaceOrderRequest& request);
    void requestCancelOrder(const QString& orderId);

protected:
    StrategySDK* m_sdk;  // Access platform features
};
```

### StrategySDK (StrategySDK.h)

**Role**: Public API providing strategies access to platform features

**Services Provided**:
```cpp
class StrategySDK {
public:
    // Market data access
    QVector<Bar> getBars(const QString& symbol,
                        const QString& interval,
                        const QDateTime& start,
                        const QDateTime& end);

    Bar getLatestBar(const QString& symbol, const QString& interval);

    QVector<Position> getPositions(const QString& accountId);
    QVector<Order> getOrders(const QString& accountId);
    Balance getBalance(const QString& accountId);

    // Data subscription and exclusive claiming
    // claimSymbols() is the PRIMARY API — replaces subscribeToSymbol() as the
    // symbol acquisition path. Grants exclusive trading authority.
    QFuture<QStringList> claimSymbols(const QStringList& symbols);

    // Legacy internal subscription (kept for secondary use cases)
    QFuture<bool> subscribeToSymbol(const QString& symbol);

    // Order management
    QString placeOrder(const PlaceOrderRequest& request);
    bool cancelOrder(const QString& orderId);
    bool modifyOrder(const QString& orderId, const ModifyOrderRequest& request);

    // Account info
    QVector<Account> getAccounts();
    QString getSelectedAccountId();

    // Utilities
    bool isMarketOpen();
    QDateTime getMarketTime();

    // Settings (strategy-specific)
    void setSetting(const QString& key, const QVariant& value);
    QVariant getSetting(const QString& key, const QVariant& defaultValue = QVariant());
};
```

### StrategySignalHandler

**Role**: Crash detection and isolation using POSIX signals

**Catches**:
- SIGSEGV (segmentation fault)
- SIGABRT (abort)
- SIGTERM (termination)
- SIGFPE (floating point exception)

**Behavior**:
- Logs crash with backtrace
- Marks strategy as crashed
- Prevents application crash
- Allows other strategies to continue

**Implementation**:
```cpp
void StrategySignalHandler::handleSignal(int signal) {
    // Log crash
    qCCritical() << "Strategy crashed with signal:" << signal;

    // Generate backtrace
    void* buffer[100];
    int size = backtrace(buffer, 100);
    char** symbols = backtrace_symbols(buffer, size);

    for (int i = 0; i < size; i++) {
        qCCritical() << symbols[i];
    }

    // Notify manager
    emit strategyCrashed(getCurrentStrategyId(), "Signal: " + QString::number(signal));

    // Don't terminate entire application
    // Strategy thread will be cleaned up
}
```

## Strategy States

```cpp
enum class StrategyStatus {
    LOADED,    // Plugin loaded but not started
    RUNNING,   // Strategy actively executing
    STOPPED,   // Strategy finished or stopped by user
    ERROR,     // Strategy encountered error
    CRASHED    // Strategy crashed (signal caught)
};
```

## Threading Model

### Thread Architecture

```
Main Thread (StrategyManager)
    │
    ├─→ Strategy Thread 1 (Strategy A)
    │   ├─ Signal handler installed
    │   ├─ onStart() → onBar() → onStop()
    │   └─ Emits signals to manager
    │
    ├─→ Strategy Thread 2 (Strategy B)
    │   ├─ Signal handler installed
    │   ├─ onStart() → onBar() → onStop()
    │   └─ Emits signals to manager
    │
    └─→ Strategy Thread N (Strategy N)
        └─ (same pattern)
```

### Thread Communication

**Strategy → Manager**:
```cpp
// In strategy
emit log("Processing bar");
emit metric("profit", 1250.50);
emit requestPlaceOrder(orderRequest);

// Received by manager
connect(strategy, &StrategyBase::log,
        manager, &StrategyManager::onStrategyLog);
```

**Manager → Strategy**:
```cpp
// Call methods on strategy thread
QMetaObject::invokeMethod(strategy, "onBar",
                         Qt::QueuedConnection,
                         Q_ARG(QString, symbol),
                         Q_ARG(Bar, bar));
```

### Thread Safety

- Each strategy has isolated memory and thread
- SDK methods use queued connections to backend
- No shared mutable state between strategies
- Crashes isolated to strategy thread

## Plugin Development

### Creating a Strategy Plugin

**1. Create strategy class**:
```cpp
// MyStrategy.h
#include "Strategy/StrategyBase.h"

class MyStrategy : public StrategyBase {
    Q_OBJECT

public:
    explicit MyStrategy(StrategySDK* sdk, QObject* parent = nullptr);

    // Required implementations
    void onStart() override;
    void onStop() override;

    QString getName() const override { return "My Strategy"; }
    QString getVersion() const override { return "1.0.0"; }
    QString getDescription() const override { return "Example strategy"; }

    // Optional overrides
    void onBar(const QString& symbol, const Bar& bar) override;

private:
    // Strategy state
    QString m_symbol;
    double m_positionSize;
};
```

**2. Implement strategy logic**:
```cpp
// MyStrategy.cpp
#include "MyStrategy.h"

MyStrategy::MyStrategy(StrategySDK* sdk, QObject* parent)
    : StrategyBase(sdk, parent)
    , m_symbol("AAPL")
    , m_positionSize(0.0)
{
}

void MyStrategy::onStart() {
    emit log("Strategy starting");

    // Subscribe to symbol
    m_sdk->subscribeToSymbol(m_symbol, "1min");

    // Load settings
    m_positionSize = m_sdk->getSetting("positionSize", 100.0).toDouble();
}

void MyStrategy::onBar(const QString& symbol, const Bar& bar) {
    if (symbol != m_symbol) return;

    emit log(QString("Received bar: %1").arg(bar.close()));

    // Strategy logic here
    if (shouldBuy(bar)) {
        PlaceOrderRequest request;
        request.symbol = symbol;
        request.tradeAction = TradeAction::Buy;
        request.quantity = m_positionSize;
        emit requestPlaceOrder(request);
    }
}

void MyStrategy::onStop() {
    // Cleanup
    m_sdk->unsubscribeFromSymbol(m_symbol, "1min");
    emit log("Strategy stopped");
}
```

**3. Export plugin symbols**:
```cpp
// Export functions for plugin loading
extern "C" {
    StrategyBase* createStrategy(StrategySDK* sdk) {
        return new MyStrategy(sdk);
    }

    void destroyStrategy(StrategyBase* strategy) {
        delete strategy;
    }
}
```

**4. Build as shared library**:
```cmake
# CMakeLists.txt
add_library(MyStrategy SHARED
    MyStrategy.h
    MyStrategy.cpp
)

target_link_libraries(MyStrategy PRIVATE
    Qt6::Core
    L2TraderStrategy  # Strategy framework
)

set_target_properties(MyStrategy PROPERTIES
    PREFIX ""         # No 'lib' prefix
    SUFFIX ".so"      # Explicit .so extension
)
```

### Plugin Loading

```cpp
// Load plugin
bool success = StrategyManager::getInstance().loadStrategy("/path/to/MyStrategy.so");

// Start strategy
if (success) {
    StrategyManager::getInstance().startStrategy("MyStrategy");
}

// Stop strategy
StrategyManager::getInstance().stopStrategy("MyStrategy");

// Unload plugin
StrategyManager::getInstance().unloadStrategy("MyStrategy");
```

## GUI Integration

### StrategyQuickView (in Trade tab)

All strategy management is handled from **StrategyQuickView** embedded in the Trade tab:
- **Load** button (top-right corner) — opens `StrategyLoadDialog`
- Right-click **Start / Stop / Unload** on a strategy row
- Right-click **Display Logs** → opens `StrategyLogWidget` in bottom panel
- Strategy → symbol hierarchy with live position columns (Qty, Avg Price, U/P&L, R/P&L)

> **Note**: The dedicated StrategiesTab was removed. `StrategyLoadDialog` was moved to
> `Src/FrontEnd/GUI/Dialogs/`.

### StrategyLogWidget (bottom splitter panel)

Per-strategy log viewer displayed alongside the platform logger at the bottom of the main window.
Polls `StrategyLogger::getMessages()` every 1000 ms and appends only new entries.

## Strategy Logging

### StrategyLogger (StrategyLogger.h/cpp)

**Per-strategy log system — writes incrementally to disk** (same model as platform `AppLogs`).

**File location**: `~/.local/state/L2Trader/StrategiesLogs/strategy_{Name}_YYYY-MM-DD_hh-mm-ss.log`

**Key design points**:
- File opened at **construction time** (timestamp = strategy load time; fixed for the session)
- Every `log()` call writes and flushes to disk immediately (no buffering)
- In-memory circular buffer (10 000 messages) preserved for live UI polling via `getMessages()`
- Plain text format (no ANSI codes): `[yyyy-MM-dd hh:mm:ss.zzz] LEVEL - message`
- `saveToFile()` is now a const no-op; `getLogFilePath()` returns the fixed path

**Thread safety**: `log()` is called from the strategy's own thread — no cross-thread access.

---

## Symbol Claiming Mechanism

### Purpose

Strategies must claim exclusive authority over symbols before trading them.
No two strategies can claim the same ticker simultaneously.

### API

```cpp
// Primary symbol acquisition API — call in onStart()
QFuture<QStringList> claimSymbols(const QStringList& symbols);
// Returns the approved subset (symbols not already owned by another strategy).
// For each approved symbol: data feeds are subscribed and trading authority is granted.
```

### Enforcement

`StrategySDK::placeOrder()` checks that `p_order.getSymbol()` is in the strategy's
claimed set before forwarding to MainAlgo. Unclaimed symbols are immediately rejected.

### Platform Registry

`StrategyManager::m_symbolRegistry` (`QMap<QString,QString>`) maps symbol → ownerStrategyID.
`StrategyManager::processClaimSymbols()` grants or denies each requested symbol.
`StrategyManager::releaseSymbols(strategyID)` frees all claims for a strategy.
`releaseSymbols` is called automatically from `unloadStrategy()`.

### New Signal

```cpp
// Emitted from MainAlgo thread when a strategy's claim is processed
void symbolsClaimed(const QString& strategyID, const QStringList& claimedSymbols);
```

### Usage Pattern in Strategies

```cpp
void MyStrategy::onStart() {
    m_sdk->claimSymbols({"NVDA", "SPY"}).then(this, [this](const QStringList& approved) {
        if (!approved.contains("NVDA")) {
            m_sdk->log("NVDA unavailable", LogLevel::Error);
            return;
        }
        // Proceed with approved symbols
    });
}
```

---

## Error Handling

### Plugin Loading Errors

```cpp
if (!QLibrary::isLibrary(pluginPath)) {
    qCWarning() << "Not a valid library:" << pluginPath;
    return false;
}

QLibrary library(pluginPath);
if (!library.load()) {
    qCWarning() << "Failed to load library:" << library.errorString();
    return false;
}

// Resolve symbols
auto createFunc = (CreateStrategyFunc)library.resolve("createStrategy");
if (!createFunc) {
    qCWarning() << "Plugin missing createStrategy function";
    return false;
}
```

### Runtime Errors

```cpp
void StrategyManager::handleStrategyError(const QString& strategyId,
                                         const QString& error) {
    qCWarning() << "Strategy error:" << strategyId << error;

    // Mark as error state
    m_strategies[strategyId].status = StrategyStatus::ERROR;

    // Notify UI
    emit strategyError(strategyId, error);

    // Optional: Auto-stop strategy
    stopStrategy(strategyId);
}
```

### Crash Recovery

```cpp
void StrategyManager::handleStrategyCrash(const QString& strategyId) {
    qCCritical() << "Strategy crashed:" << strategyId;

    // Mark as crashed
    m_strategies[strategyId].status = StrategyStatus::CRASHED;

    // Stop strategy thread
    m_strategies[strategyId].thread->quit();
    m_strategies[strategyId].thread->wait();

    // Notify UI
    emit strategyCrashed(strategyId, "Segmentation fault");

    // Optional: Offer to restart
    // restartStrategy(strategyId);
}
```

## Configuration

### Strategy Settings

Per-strategy settings stored in:
```
~/.config/L2Trader/strategies/<strategy-name>.conf
```

Accessed via SDK:
```cpp
// In strategy
double threshold = m_sdk->getSetting("threshold", 0.02).toDouble();
m_sdk->setSetting("lastRun", QDateTime::currentDateTime());
```

### Global Strategy Settings

```cpp
Settings::setValue("Strategy/MaxConcurrent", 5);     // Max strategies
Settings::setValue("Strategy/AutoRestart", true);    // Auto-restart on crash
Settings::setValue("Strategy/LogLevel", "debug");    // Per-strategy logging
```

## Testing Strategies

### Unit Tests

Test strategy logic independently:
```cpp
TEST(MyStrategy, BuysOnCondition) {
    MockStrategySDK mockSdk;
    MyStrategy strategy(&mockSdk);

    strategy.onStart();

    Bar bar = createTestBar(100.0);  // Price triggers buy
    strategy.onBar("AAPL", bar);

    EXPECT_TRUE(mockSdk.orderPlaced());
}
```

### Integration Tests

Test with real platform:
```cpp
// Use paper trading account
// Load strategy plugin
// Run for short period
// Verify expected behavior
```

### Backtesting

Use ReplayEngine to test strategies on historical data:
```cpp
ReplayEngine replay;
replay.loadDataFromFile("AAPL_2024.csv");

MyStrategy strategy(&sdk);
strategy.onStart();

connect(&replay, &ReplayEngine::barEmitted, &strategy, &MyStrategy::onBar);

replay.start();  // Emit historical bars
replay.wait();   // Wait for completion

// Analyze results
```

### Testing in Replay Mode

Replay mode provides full order emulation for strategy testing:

**Key Differences from Live/Simulation**:
- Orders processed by `OrderEmulator` (no network requests)
- Market data comes from recorded SQLite databases
- Fills based on recorded market depth snapshots
- Simulated latency (100-500ms reception, 10-50ms execution)

**Testing Flow**:
1. Enter replay mode (click "Replay" button or use `MainApp::enterReplayMode()`)
2. Load strategy plugin
3. Start strategy
4. Press play to begin replay
5. Strategy receives bars via `onBar()` callback
6. Strategy can place orders via `m_sdk->placeOrder()`
7. Orders processed by OrderEmulator with realistic delays
8. Filled orders trigger `onOrder()` callback
9. Position updates trigger `onPosition()` callback

**Simulated Account**:
- Account ID: `SIM123456`
- Starting balance: $100,000
- Full order validation (balance checks, boxing prevention)

**Order Lifecycle in Replay**:
```
placeOrder() → 100-500ms delay → OPN status → 10-50ms delay → FLL status
```

**Accessing via SDK in Replay Mode**:
```cpp
void MyStrategy::onStart() {
    // Same SDK calls work in replay mode
    QVector<Position> positions = m_sdk->getPositions("SIM123456");
    QVector<Order> orders = m_sdk->getOrders("SIM123456");
    Balance balance = m_sdk->getBalance("SIM123456");

    // Place orders - processed by OrderEmulator
    PlaceOrderRequest request;
    request.setAccountID("SIM123456");
    request.setSymbol("AAPL");
    request.setQuantity(100);
    m_sdk->placeOrder(request);
}
```

See `Src/Core/Replay/OrderEmulator/AGENTS.md` for detailed order emulation documentation.

## Best Practices

1. **Always implement onStop()**: Clean up resources, unsubscribe, close positions
2. **Use SDK methods**: Don't access TSClient or MainAlgo directly
3. **Emit logs**: Use emit log() for debugging
4. **Track metrics**: Emit metrics for monitoring (P/L, win rate, etc.)
5. **Handle errors gracefully**: Don't crash, emit error messages
6. **Test thoroughly**: Unit tests, integration tests, backtests
7. **Version your strategies**: Semantic versioning (1.0.0, 1.1.0, etc.)
8. **Document parameters**: What settings does strategy use?

## Related Agent Instructions

- `../AGENTS.md`: Source directory overview
- `../Algo/AGENTS.md`: Algorithm coordination
- `../Core/AGENTS.md`: Core components

## Related Documentation

- `Doc/STRATEGY.md`: Complete strategy system documentation with examples
- `Doc/ARCHITECTURE.md`: System architecture
- `Doc/DEVELOPMENT.md`: Development guidelines
