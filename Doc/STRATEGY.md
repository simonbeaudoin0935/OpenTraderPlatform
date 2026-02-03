# Strategy System

## Overview

L2Trader includes a powerful, extensible strategy system that allows users to develop and deploy custom trading algorithms as dynamically loaded plugins. The strategy system provides a clean separation between the core application and user strategies, with robust threading, crash isolation, and real-time monitoring capabilities.

## Table of Contents

1. [Architecture](#architecture)
2. [Strategy Lifecycle](#strategy-lifecycle)
3. [GUI Integration](#gui-integration)
4. [Development Guide](#development-guide)
5. [API Reference](#api-reference)
6. [Configuration](#configuration)
7. [Best Practices](#best-practices)
8. [Known Limitations](#known-limitations)

---

## Architecture

### Core Components

The strategy system consists of several key components:

**Strategy Management:**
- **StrategyManager**: Orchestrates strategy lifecycle (load, start, stop, unload)
- **StrategyBase**: Abstract base class that all strategies must inherit from
- **StrategySDK**: Public API providing strategies access to platform features
- **StrategySignalHandler**: Crash detection and isolation (catches SIGSEGV, SIGABRT, SIGTERM)

**GUI Components:**
- **StrategiesTab**: Main strategy management interface
- **StrategyCard**: Visual representation of a running strategy
- **StrategyLoadDialog**: Plugin selection and configuration dialog

### Threading Model

Each loaded strategy runs in its own dedicated QThread:

```
Main Thread (UI + StrategyManager)
    │
    ├─→ Strategy Thread 1 (Strategy A)
    ├─→ Strategy Thread 2 (Strategy B)
    └─→ Strategy Thread N (Strategy N)
```

**Thread Communication:**
- Strategies emit signals to StrategyManager using `Qt::QueuedConnection`
- StrategyManager dispatches callbacks to strategies on their respective threads
- UI updates occur on the main thread via signal-slot connections

**Thread Safety:**
- Each strategy has isolated memory and execution context
- Crashes in one strategy do not affect others or the main application
- Proper cleanup using `QMetaObject::invokeMethod` with `Qt::BlockingQueuedConnection`

### Data Flow

```
Strategy Thread
    ↓
    onStart() → onData() → onStop()
    ↓
    emit signals (logs, orders, status)
    ↓
StrategyManager (main thread)
    ↓
    → StrategyCard UI update
    → Database persistence
    → BarCache / TSClient integration
```

---

## Strategy Lifecycle

### States

A strategy can be in one of the following states:

1. **LOADED**: Plugin loaded but not yet started
2. **RUNNING**: Strategy is actively executing
3. **STOPPED**: Strategy finished or was stopped by user
4. **ERROR**: Strategy crashed or encountered fatal error

### Lifecycle Flow

```
1. Load Plugin
   ├─→ Dynamic library loading (.so file)
   ├─→ Symbol resolution (create/destroy functions)
   └─→ State: LOADED

2. Start Strategy
   ├─→ Create dedicated QThread
   ├─→ Move strategy object to thread
   ├─→ Call onStart() on strategy thread
   └─→ State: RUNNING

3. Execute Strategy
   ├─→ onData() called on market events
   ├─→ Strategy emits logs, orders, status
   └─→ State: RUNNING (or ERROR on crash)

4. Stop Strategy
   ├─→ Call onStop() on strategy thread (using BlockingQueuedConnection)
   ├─→ Thread quit() and wait()
   └─→ State: STOPPED

5. Unload Plugin
   ├─→ Delete strategy object (destroy function)
   ├─→ Unload dynamic library
   └─→ State: (removed)
```

### Crash Handling

The StrategySignalHandler catches fatal signals and marks strategies as ERROR:

```cpp
// If strategy crashes (SIGSEGV, SIGABRT, etc.)
StrategySignalHandler::handleSignal(int signal)
    ↓
    → Mark strategy state as ERROR
    → Log crash details
    → Isolate crash to strategy thread only
    → Main app and other strategies unaffected
```

---

## GUI Integration

### StrategyCard Layout

Each loaded strategy is represented by a card widget with:

**Header:**
- Strategy name
- Current status indicator (RUNNING/STOPPED/ERROR)
- Start button (green when stopped, disabled when running)
- Stop button (red when running, disabled when stopped)

**Content Panels (horizontal scrolling):**
- **Logs Panel**: Real-time strategy logs with auto-scroll
- **Info Panel**: Strategy metadata and configuration
- **Orders Panel**: Orders placed by this strategy (future)
- **Positions Panel**: Positions held by this strategy (future)

### User Interactions

**Start/Stop Controls:**
- **Start Button**: Enabled when strategy is STOPPED or ERROR
  - Green color (#51cf66) for visual clarity
  - Calls `StrategyManager::startStrategy()`
  
- **Stop Button**: Enabled when strategy is RUNNING
  - Red color (#ff6b6b) for visual clarity
  - Calls `StrategyManager::unloadStrategy()`

**Log Display:**
- Auto-scrolls to bottom on new messages
- User can scroll up to review history without interruption
- Smart detection: auto-scroll disabled when user scrolls up
- Auto-scroll re-enables when user returns to bottom

**Real-Time Updates:**
- Strategy status changes update button states immediately
- Logs append in real-time (<100ms latency)
- Visual feedback for all state transitions

---

## Development Guide

### Creating a Strategy Plugin

#### 1. Project Structure

```
Strategies/
└── MyStrategy/
    ├── CMakeLists.txt          # Build configuration
    ├── MyStrategy.h            # Strategy header
    └── MyStrategy.cpp          # Strategy implementation
```

#### 2. CMakeLists.txt Template

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyStrategy)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core)

# Strategy plugin library
add_library(MyStrategy SHARED
    MyStrategy.h
    MyStrategy.cpp
)

target_link_libraries(MyStrategy PRIVATE
    Qt6::Core
)

# Install to strategies directory
install(TARGETS MyStrategy
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/L2Trader/strategies
)
```

#### 3. Strategy Header (MyStrategy.h)

```cpp
#pragma once

#include "Src/Strategy/StrategyBase.h"
#include "Src/Strategy/StrategySDK.h"

class MyStrategy : public StrategyBase {
    Q_OBJECT

public:
    explicit MyStrategy(QObject* parent = nullptr);
    ~MyStrategy() override;

    // Lifecycle hooks
    void onStart() override;
    void onStop() override;
    void onData() override;

private:
    StrategySDK m_sdk;
    // Your strategy state variables
};

// Plugin factory functions (required)
extern "C" {
    StrategyBase* createStrategy();
    void destroyStrategy(StrategyBase* strategy);
}
```

#### 4. Strategy Implementation (MyStrategy.cpp)

```cpp
#include "MyStrategy.h"

MyStrategy::MyStrategy(QObject* parent)
    : StrategyBase(parent)
{
    // Initialize strategy
}

MyStrategy::~MyStrategy()
{
    // Cleanup
}

void MyStrategy::onStart()
{
    emit logMessage("Strategy started");
    
    // Example: Request historical bars
    QDate startDate = QDate::currentDate().addDays(-5);
    QDate endDate = QDate::currentDate();
    
    m_sdk.requestHistoricalBars(startDate, endDate, [this](auto bars) {
        if (bars) {
            emit logMessage(QString("Received %1 bars").arg(bars->size()));
            // Process bars
        }
    });
}

void MyStrategy::onStop()
{
    emit logMessage("Strategy stopping");
    // Cleanup resources
}

void MyStrategy::onData()
{
    // Called on market data updates
    // Implement your trading logic here
}

// Plugin factory functions
extern "C" {
    StrategyBase* createStrategy() {
        return new MyStrategy();
    }
    
    void destroyStrategy(StrategyBase* strategy) {
        delete strategy;
    }
}
```

#### 5. Build and Install

```bash
mkdir build
cd build
cmake ..
make
sudo make install
```

---

## API Reference

### StrategyBase (Abstract Base Class)

**Lifecycle Methods:**

```cpp
// Called when strategy starts (runs on strategy thread)
virtual void onStart() = 0;

// Called when strategy stops (runs on strategy thread)
virtual void onStop() = 0;

// Called on market data updates (runs on strategy thread)
virtual void onData() = 0;
```

**Signals (Emit from Strategy Thread):**

```cpp
// Log a message to the strategy card
void logMessage(const QString& message);

// Update strategy status
void statusChanged(StrategyStatus status);

// Report an error
void errorOccurred(const QString& error);
```

### StrategySDK

**Data Access:**

```cpp
// Request historical bars
void requestHistoricalBars(
    const QDate& startDate,
    const QDate& endDate,
    std::function<void(std::shared_ptr<QVector<Bar>>)> callback
);

// Get current quote (future)
void getCurrentQuote(const QString& symbol, 
                     std::function<void(Quote)> callback);

// Subscribe to real-time bars (future)
void subscribeToRealTimeBars(const QString& symbol);
```

**Order Management (Future):**

```cpp
// Place market order
void placeMarketOrder(const QString& symbol, int quantity, 
                      OrderSide side);

// Place limit order
void placeLimitOrder(const QString& symbol, int quantity, 
                     OrderSide side, double limitPrice);

// Cancel order
void cancelOrder(const QString& orderId);
```

**Position Management (Future):**

```cpp
// Get current positions
QVector<Position> getPositions();

// Get position for symbol
std::optional<Position> getPosition(const QString& symbol);
```

---

## Configuration

### Strategy Configuration File

Strategies are configured via JSON files in `~/.config/L2Trader/Strategies/`:

**Example: `HistoricalBarsStrategy.json`**

```json
{
    "name": "Historical Bars Strategy",
    "pluginPath": "/usr/local/lib/L2Trader/strategies/libHistoricalBarsStrategy.so",
    "autoStart": false,
    "parameters": {
        "symbol": "AAPL",
        "daysBack": 30,
        "fetchInterval": 2000
    }
}
```

**Configuration Fields:**

- **name**: Display name in GUI
- **pluginPath**: Absolute path to strategy .so file
- **autoStart**: Whether to start strategy on app launch
- **parameters**: Strategy-specific configuration (key-value pairs)

### Loading Strategies

**Via GUI:**
1. Open "Strategies" tab
2. Click "Load Strategy" button
3. Select strategy plugin from dialog
4. Click "Start" button on strategy card

**Via Configuration:**
1. Create JSON config file in `~/.config/L2Trader/Strategies/`
2. Set `autoStart: true`
3. Restart L2Trader

---

## Best Practices

### Threading

1. **Never block the strategy thread**
   - Use asynchronous callbacks for long operations
   - Avoid synchronous network calls or file I/O

2. **Emit signals for communication**
   - Don't call StrategyManager methods directly
   - Use `emit logMessage()` for logging
   - Use `emit statusChanged()` for state updates

3. **Proper cleanup in onStop()**
   - Cancel any pending timers
   - Close open connections
   - Save strategy state if needed

### Resource Management

1. **Use Qt parent-child ownership**
   ```cpp
   QTimer* timer = new QTimer(this);  // Automatically cleaned up
   ```

2. **Use smart pointers for non-Qt objects**
   ```cpp
   std::unique_ptr<DataProcessor> processor;
   ```

3. **Prefer composition over pointers**
   ```cpp
   class MyStrategy {
       BarCache m_cache;  // Direct member, not pointer
   };
   ```

### Error Handling

1. **Validate all inputs**
   ```cpp
   void onStart() {
       if (m_symbol.isEmpty()) {
           emit errorOccurred("Symbol not configured");
           return;
       }
   }
   ```

2. **Handle callback failures gracefully**
   ```cpp
   m_sdk.requestHistoricalBars(start, end, [this](auto bars) {
       if (!bars || bars->empty()) {
           emit logMessage("No bars received, retrying...");
           // Implement retry logic
           return;
       }
       // Process bars
   });
   ```

3. **Log errors clearly**
   ```cpp
   emit logMessage(QString("Error: %1").arg(error.message()));
   ```

### Performance

1. **Minimize logging frequency**
   - Don't log on every bar update
   - Use rate limiting or conditional logging

2. **Cache frequently accessed data**
   - Don't re-fetch data unnecessarily
   - Use member variables for state

3. **Batch operations when possible**
   - Process multiple bars in one call
   - Group database writes

---

## Known Limitations

### Current Limitations

1. **Symbol Restriction**
   - All strategies fetch data for the currently displayed stock only
   - Cannot test multiple stocks simultaneously
   - Fix planned in Phase 8.1 (symbol-specific bar fetching)

2. **Trading Days Assumption**
   - Assumes all trading days have exactly 840 bars (6:01 AM - 8:00 PM)
   - No holiday calendar integration
   - No support for early market closes
   - Would crash on holidays or special market hours
   - Fix planned in Phase 8.2 (holiday/early-close support)

3. **Configuration UI**
   - Strategies only configurable via JSON files
   - No GUI for parameter editing
   - No validation feedback in UI
   - Fix planned in Phase 8.3 (strategy configuration UI)

4. **Order Execution**
   - Order placement API not yet implemented
   - Position tracking incomplete
   - No P&L calculation
   - Planned for future phases

### Technical Debt

- Strategy configuration objects lack class-level validation
- Error handling in config parsing could be more robust
- No database schema versioning
- No migration system for schema changes

---

## Future Enhancements

### Phase 8 - Production Readiness

**8.1 Symbol-Specific Bar Fetching**
- Modify TSClient to accept symbol parameter
- Allow each strategy to fetch its own stock data
- Remove dependency on displayed stock

**8.2 Holiday/Early-Close Support**
- Implement NASDAQ/NYSE holiday calendar
- Handle variable bar counts per day
- Graceful degradation for unknown dates

**8.3 Strategy Configuration UI**
- Parameter editor dialog in GUI
- Real-time parameter validation
- Config file auto-generation

**8.4 Developer Documentation**
- Comprehensive API reference
- Example strategy templates
- Performance optimization guide

### Phase 9 - Advanced Features

**9.1 Enhanced Monitoring**
- Real-time balance updates per strategy
- P&L tracking and display
- Performance graphs (CPU, memory, latency)

**9.2 Strategy Comparison**
- Side-by-side card comparison
- Performance metrics table
- Cumulative P&L charts
- Strategy ranking

**9.3 Advanced UI**
- Collapsible panels (orders, positions, logs)
- Parameter editing without restart
- Strategy presets and templates

---

## Appendix: Key Files

### Strategy System Core

- `Src/Strategy/StrategyManager.h/cpp` - Lifecycle orchestration
- `Src/Strategy/StrategyBase.h` - Abstract base class
- `Src/Strategy/StrategySDK.h/cpp` - Public API
- `Src/Strategy/StrategySignalHandler.h/cpp` - Crash detection

### GUI Components

- `Src/FrontEnd/GUI/StrategiesTab/StrategiesTab.h/cpp` - Main tab
- `Src/FrontEnd/GUI/StrategiesTab/StrategyCard.h/cpp` - Card widget
- `Src/FrontEnd/GUI/StrategiesTab/StrategyLoadDialog.h/cpp` - Plugin loader

### Example Strategy

- `Strategies/HistoricalBarsStrategy/` - Reference implementation
- `Example_Config/strategies/` - Example configuration files

### Planning Documents

For detailed development history and planning information:

- `Doc/plans/STRATEGY_SYSTEM_PLAN.md` - Complete development roadmap
- `Doc/plans/PHASE_6.5_SESSION_CHECKPOINT.md` - Latest session checkpoint

---

## Support and Contributions

For questions, bug reports, or feature requests related to the strategy system, please refer to the main project documentation and contribution guidelines.

**See also:**
- [Architecture Documentation](ARCHITECTURE.md) - Overall system architecture
- [Development Guide](DEVELOPMENT.md) - Build and development setup
- [Contributing Guide](CONTRIBUTING.md) - Contribution guidelines
