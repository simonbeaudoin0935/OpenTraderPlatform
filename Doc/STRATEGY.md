# Strategy System

## Overview

L2Trader includes an extensible strategy system that allows users to develop and deploy custom trading algorithms as dynamically loaded shared-library plugins (`.so` files on Linux). Each strategy runs in its own dedicated thread with crash isolation — a crash in one strategy does not affect the main application or other strategies.

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

**Strategy Management**:
- **StrategyManager**: Orchestrates strategy lifecycle (load, start, stop, unload)
- **StrategyBase**: Abstract base class; all strategies inherit from this
- **StrategySDK**: Public API giving strategies access to platform data and services
- **StrategySignalHandler**: Crash detection and isolation (catches SIGSEGV, SIGABRT, SIGTERM)

**GUI Components**:
- **StrategiesTab**: Main strategy management tab
- **StrategyCard**: Visual representation of a loaded strategy (logs, status, controls)
- **StrategyLoadDialog**: Plugin selection and configuration dialog
- **StrategyQuickView**: Compact tree widget in the Trade tab showing all strategies and their claimed symbols

### Threading Model

```
Main Thread (UI + StrategyManager)
    │
    ├─► Strategy Thread 1 (Strategy A)
    ├─► Strategy Thread 2 (Strategy B)
    └─► Strategy Thread N (Strategy N)
```

- Strategies communicate with `StrategyManager` via Qt signals using `Qt::QueuedConnection`
- `StrategyManager` dispatches callbacks to strategies on their respective threads
- UI updates always happen on the main thread

### Data Sources for Strategies

Strategies access market data and brokerage services through `StrategySDK`. Under the hood:

- **Market data** (bars, Level 2, trades) — provided by `DBClient` (Databento)
- **Order execution** — routed to `TSClient` (TradeStation) in live/sim mode, or to `OrderEmulator` in replay mode
- **Bar cache** — in-memory + SQLite via `BarCache`

---

## Strategy Lifecycle

### States

| State | Description |
|-------|-------------|
| **LOADED** | Plugin loaded, not started |
| **RUNNING** | Strategy actively executing |
| **STOPPED** | Finished or stopped by user |
| **ERROR** | Crashed or encountered fatal error |

### Lifecycle Flow

```
1. Load Plugin
   ├─► Dynamic library loading (.so file)
   ├─► Symbol resolution (create/destroy factory functions)
   └─► State: LOADED

2. Start Strategy
   ├─► Create dedicated QThread
   ├─► Move strategy object to thread
   ├─► Call onStart() on strategy thread
   └─► State: RUNNING

3. Execute Strategy
   ├─► onNewBar() / onNewLevel2() / onNewTrade() called on market events
   ├─► Strategy emits logMessage, statusChanged signals
   └─► State: RUNNING (or ERROR on crash)

4. Stop Strategy
   ├─► Call onStop() on strategy thread (BlockingQueuedConnection)
   ├─► Thread quit() and wait()
   └─► State: STOPPED

5. Unload Plugin
   ├─► Delete strategy object via destroy function
   ├─► Unload dynamic library
   └─► State: (removed from StrategyManager)
```

### Crash Handling

```cpp
// If strategy thread receives SIGSEGV / SIGABRT / SIGTERM:
StrategySignalHandler::handleSignal(signal)
    → Mark strategy state as ERROR
    → Log crash details
    → Isolate crash to strategy thread only
    → Main app and other strategies unaffected
```

---

## GUI Integration

### StrategyCard Layout

Each loaded strategy gets a card widget in the Strategies tab:

**Header**:
- Strategy name
- Status indicator (RUNNING / STOPPED / ERROR)
- **Start** button (green `#51cf66`, enabled when STOPPED or ERROR)
- **Stop** button (red `#ff6b6b`, enabled when RUNNING)

**Content Panels** (horizontal scrolling):
- **Logs Panel**: Real-time strategy logs with smart auto-scroll (pauses when user scrolls up; resumes when scrolled to bottom)
- **Info Panel**: Strategy metadata and configuration
- **Orders Panel**: Orders placed by this strategy *(future)*
- **Positions Panel**: Positions held by this strategy *(future)*

### StrategyQuickView

A compact tree widget in the Trade tab. Each row shows a loaded strategy and the symbol it has claimed (if any). Provides at-a-glance status without switching to the Strategies tab.

---

## Development Guide

### Creating a Strategy Plugin

#### 1. Project Structure

```
Strategies/
└── MyStrategy/
    ├── CMakeLists.txt
    ├── MyStrategy.h
    └── MyStrategy.cpp
```

#### 2. CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyStrategy)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(MyStrategy SHARED
    MyStrategy.h
    MyStrategy.cpp
)

target_link_libraries(MyStrategy PRIVATE Qt6::Core)

install(TARGETS MyStrategy
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/L2Trader/strategies
)
```

#### 3. Strategy Header

```cpp
#pragma once
#include "Src/Strategy/StrategyBase.h"
#include "Src/Strategy/StrategySDK.h"

class MyStrategy : public StrategyBase {
    Q_OBJECT
public:
    explicit MyStrategy(QObject* parent = nullptr);
    ~MyStrategy() override;

    void onStart() override;
    void onStop() override;
    void onNewBar(const QString& symbol, const Bar& bar) override;
    void onNewLevel2(const QString& symbol, const Level2& level2) override;
    void onNewTrade(const QString& symbol, const Trade& trade) override;

private:
    StrategySDK m_sdk;
};

extern "C" {
    StrategyBase* createStrategy();
    void destroyStrategy(StrategyBase* strategy);
}
```

#### 4. Strategy Implementation

```cpp
#include "MyStrategy.h"

MyStrategy::MyStrategy(QObject* parent)
    : StrategyBase(parent)
{}

MyStrategy::~MyStrategy() = default;

void MyStrategy::onStart()
{
    emit logMessage("Strategy started");

    // Request historical bars for analysis
    m_sdk.requestHistoricalBars("AAPL", QDate::currentDate().addDays(-5),
                                QDate::currentDate(),
                                [this](std::shared_ptr<QVector<Bar>> bars) {
        if (!bars || bars->isEmpty()) {
            emit logMessage("No bars received");
            return;
        }
        emit logMessage(QString("Received %1 bars").arg(bars->size()));
    });
}

void MyStrategy::onStop()
{
    emit logMessage("Strategy stopping");
}

void MyStrategy::onNewBar(const QString& symbol, const Bar& bar)
{
    Q_UNUSED(symbol)
    // React to each completed 1-minute bar
    if (bar.close > bar.open)
        emit logMessage("Bullish bar: " + QString::number(bar.close));
}

void MyStrategy::onNewLevel2(const QString& symbol, const Level2& level2)
{
    Q_UNUSED(symbol)
    Q_UNUSED(level2)
    // React to order book changes
}

void MyStrategy::onNewTrade(const QString& symbol, const Trade& trade)
{
    Q_UNUSED(symbol)
    Q_UNUSED(trade)
    // React to individual trade prints
}

extern "C" {
    StrategyBase* createStrategy() { return new MyStrategy(); }
    void destroyStrategy(StrategyBase* s) { delete s; }
}
```

#### 5. Build and Install

```bash
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build .
cmake --install .
```

---

## API Reference

### StrategyBase (Abstract Base Class)

**Lifecycle methods** (run on strategy thread):
```cpp
virtual void onStart() = 0;    // Called when strategy starts
virtual void onStop() = 0;     // Called when strategy stops

// Market data callbacks (optional — default implementations do nothing)
virtual void onNewBar(const QString& symbol, const Bar& bar);
virtual void onNewLevel2(const QString& symbol, const Level2& level2);
virtual void onNewTrade(const QString& symbol, const Trade& trade);
```

**Signals** (emit from strategy thread):
```cpp
void logMessage(const QString& message);         // Log to StrategyCard
void statusChanged(StrategyStatus status);       // Update status indicator
void errorOccurred(const QString& error);        // Report fatal error
```

### StrategySDK

**Data Access**:
```cpp
// Request historical bars (async, result via callback on strategy thread)
void requestHistoricalBars(const QString& symbol,
                           const QDate& startDate,
                           const QDate& endDate,
                           std::function<void(std::shared_ptr<QVector<Bar>>)> callback);

// Claim a symbol — strategy receives onNewBar/onNewLevel2/onNewTrade for this symbol
void claimSymbol(const QString& symbol);

// Release claimed symbol
void releaseSymbol(const QString& symbol);
```

**Order Management** *(in progress)*:
```cpp
void placeMarketOrder(const QString& symbol, int quantity, OrderSide side);
void placeLimitOrder(const QString& symbol, int quantity, OrderSide side, double limitPrice);
void cancelOrder(const QString& orderId);
```

---

## Configuration

### Strategy Configuration File

Strategies are configured via JSON files in `~/.config/L2Trader/Strategies/`:

```json
{
    "name": "My AAPL Strategy",
    "pluginPath": "/usr/local/lib/L2Trader/strategies/libMyStrategy.so",
    "autoStart": false,
    "parameters": {
        "symbol": "AAPL",
        "lookbackDays": 5
    }
}
```

### Loading Strategies

**Via GUI**:
1. Open the **Strategies** tab
2. Click **Load Strategy**
3. Select the `.so` plugin file in the dialog
4. Click **Start** on the strategy card

**Via Configuration**:
1. Create a JSON config file in `~/.config/L2Trader/Strategies/`
2. Set `"autoStart": true`
3. Restart L2Trader

---

## Best Practices

### Threading

1. **Never block the strategy thread** — use async callbacks for all long operations
2. **Use signals for all outward communication** — `emit logMessage()`, `emit statusChanged()`
3. **Clean up in `onStop()`** — cancel timers, release resources, save state

### Resource Management

```cpp
// Qt parent-child for QObjects
QTimer* timer = new QTimer(this);  // Auto-cleaned up

// Smart pointers for non-Qt objects
std::unique_ptr<MyProcessor> m_processor;
```

### Error Handling

```cpp
void MyStrategy::onStart()
{
    if (m_symbol.isEmpty()) {
        emit errorOccurred("Symbol not configured");
        return;
    }
    // ... continue
}

m_sdk.requestHistoricalBars(symbol, start, end, [this](auto bars) {
    if (!bars || bars->isEmpty()) {
        emit logMessage("Warning: no bars received");
        return;
    }
    // process bars
});
```

### Performance

- Minimize logging frequency in hot paths (e.g., do not log on every Level 2 update)
- Cache frequently accessed data in member variables
- Use `onNewBar` for slow decision-making; `onNewLevel2` and `onNewTrade` for fast-path logic

---

## Known Limitations

1. **One symbol per live session** — Databento subscriptions accumulate; switching the displayed symbol adds a new subscription but old data still flows. Strategies should call `claimSymbol()` rather than rely on the displayed symbol.

2. **Order execution UI incomplete** — `StrategySDK` order placement and position tracking are in progress. Currently strategies can log orders but full P&L tracking per strategy is not yet available.

3. **No configuration GUI** — Strategy parameters are only editable via JSON files; no in-app parameter editor exists yet.

4. **Bar count assumption** — The system targets 900 bars/day (4:00 AM – 6:59 PM XNAS.ITCH hours). Partial days (holidays, early closes) will have fewer bars; the `MarketCalendar` class provides holiday/early-close data for 2026.

---

## Appendix: Key Files

| File | Purpose |
|------|---------|
| `Src/Strategy/StrategyManager.h/cpp` | Lifecycle orchestration |
| `Src/Strategy/StrategyBase.h` | Abstract base class |
| `Src/Strategy/StrategySDK.h/cpp` | Public API for strategies |
| `Src/Strategy/StrategySignalHandler.h/cpp` | Crash detection |
| `Src/FrontEnd/GUI/Tabs/StrategiesTab/` | GUI tab (StrategyCard, StrategyLoadDialog) |
| `Src/FrontEnd/GUI/Widgets/StrategyQuickView/` | Quick-view tree widget |
| `Strategies/HistoricalBarsStrategy/` | Reference example strategy |

---

## See Also

- [STRATEGY_GUIDE.md](STRATEGY_GUIDE.md) — Non-technical overview for strategy developers
- [ARCHITECTURE.md](ARCHITECTURE.md) — Overall system architecture
- [DEVELOPMENT.md](DEVELOPMENT.md) — Build and coding guidelines
- [CONTRIBUTING.md](CONTRIBUTING.md) — Contribution guidelines
