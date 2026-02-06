# Src/ Directory - Agent Instructions

This directory contains all source code for the L2Trader application.

## Directory Structure

### Core Application Components

**Clients/** - External API clients
- `TSClient/`: TradeStation API client (REST + WebSocket)
  - Primary singleton for all market data and trading operations
  - Runs in dedicated thread for async operations
  - See `Clients/TSClient/AGENTS.md` for details

**Core/** - Core application logic
- `MainApp.cpp/h`: Application orchestrator (connects all components)
- `MemoryMonitor.cpp/h`: System resource monitoring
- `OrdersDatabase.cpp/h`: Order persistence to SQLite
- `Cache/`: Data caching subsystems
  - `BarCache/`: Historical and live bar data caching
- `Replay/`: Market replay functionality for testing/analysis

**Algo/** - Trading algorithm components
- `MainAlgo.cpp/h`: Main algorithm coordinator singleton
- `BarReceiver/`: Bar data reception and processing
- `MarketDepthQuoteReceiver/`: Level 2 market depth processing
- `OrdersReceiver/`: Order status tracking
- `PositionsReceiver/`: Position tracking
- `StreamReceiver/`: Base class for stream receivers

**FrontEnd/** - User interface implementations
- `FrontEnd.h`: Abstract base class defining UI interface
- `GUI/`: Qt Widgets graphical interface
- `TUI/`: ncurses terminal interface (monitoring only)

**Strategy/** - Plugin-based strategy system
- Strategy management and lifecycle
- SDK for strategy development
- Signal handling for crash isolation

### Supporting Components

**Misc/** - Utilities and helpers
- `Logging.cpp/h`: Centralized logging with categories
- `Settings.cpp/h`: Application settings management
- `SecureStorage.cpp/h`: Encrypted credential storage (QKeychain)
- `CONSTANTS.h`: **ALL** application constants (centralized)
- `Assume.h`: Assertion macros for debug builds
- `TimeFrame.cpp/h`: Trading timeframe definitions
- `ArgumentParser.cpp/h`: CLI argument parsing
- `ShortcutSettings.cpp/h`: Keyboard shortcut management
- `ThreadStats.cpp/h`: Thread performance monitoring

**SQL/** - SQL query definitions
- **ALL** SQL queries centralized here
- One header file per class/component
- Organized by namespace
- Examples: `OrdersDatabaseQueries.h`, `LiveStreamDBQueries.h`, `StockPriceChartQueries.h`

**main.cpp** - Application entry point
- Creates QApplication/QCoreApplication
- Initializes logging
- Creates and starts MainApp

## Key Architectural Patterns

### Threading Model

Three main threads:
1. **Main Thread**: GUI/TUI event loop, MainApp orchestration
2. **TSClient Thread**: All API communication (REST + WebSocket)
3. **MainAlgo Thread**: Trading logic, bar processing, position tracking

Additional per-component threads:
- **Database Threads**: One per BarCache instance for SQLite operations
- **Strategy Threads**: One per loaded strategy plugin

### Signal/Slot Communication

Cross-thread communication uses Qt signals/slots with automatic queuing:

```
TSClient (thread) ─[authStateChanged]→ MainAlgo (thread)
                 ─[newBar]→ MainAlgo (thread)
                 
MainAlgo (thread) ─[displayedStockReceivedNewBar]→ GUIFrontend (main)
                  ─[receivedNewPosition]→ GUIFrontend (main)
                  
GUIFrontend (main) ─[selectedDisplayedStock]→ MainAlgo (thread)
```

### Memory Management Rules

1. **Composition first**: Use direct member objects when possible
   - Example: `class StockInstruments { BarCache m_barCache; };`
   
2. **Qt parent-child**: For Qt objects with parents, Qt manages memory
   - Example: `new QTimer(this)` - parent handles deletion
   
3. **QPointer**: For Qt objects with uncertain lifetime
   - Example: `QPointer<StreamBars> m_stream;` - auto-nulls on delete
   
4. **std::unique_ptr**: For exclusive ownership
   - Example: `std::unique_ptr<PositionsReceiver> m_positionReceiver;`
   
5. **std::shared_ptr**: For shared ownership across async operations
   - Example: `std::shared_ptr<QVector<Bar>> bars;` - shared between cache/UI/callbacks

6. **Stack-allocated threads**: ALWAYS use member not pointer
   - Example: `QThread m_thread;` NOT `QThread* m_thread;`

### Singleton Pattern

Classes using Meyer's Singleton (thread-safe C++11):
- **TSClient**: Global API access point
- **MainAlgo**: Centralized trading logic
- **LogBroadcaster**: Single log distribution
- **LoggingConfig**: Logging configuration

Access pattern:
```cpp
TSClient& client = TSClient::getInstance();
MainAlgo& algo = MainAlgo::getInstance();
```

### Constants and SQL Centralization

**NEVER** define constants or SQL queries inline in source files:

```cpp
// ✓ CORRECT - Use centralized constants
#include "Misc/CONSTANTS.h"
QTime start = TradingHours::TRADING_START_TIME;

// ✗ WRONG - Don't define inline
const QTime TRADING_START = QTime(9, 30, 0); // NO!

// ✓ CORRECT - Use centralized SQL
#include "SQL/OrdersDatabaseQueries.h"
query.exec(OrdersDatabaseQueries::CREATE_ORDERS_TABLE);

// ✗ WRONG - Don't write SQL inline
query.exec("CREATE TABLE orders (...)"); // NO!
```

## File Organization Best Practices

### Header Files (.h)
- Include guards or `#pragma once`
- Forward declarations where possible
- Minimal includes (prefer forward declarations)
- Public interface first, private last
- Documentation comments for public API

### Source Files (.cpp)
- Include corresponding header first
- Group includes: Qt, system, project
- Use anonymous namespace for file-local functions
- Thread affinity assertions in debug builds

### CMakeLists.txt
- Located in each directory with compilable code
- Defines targets and dependencies
- Linked from parent CMakeLists.txt

## Common Patterns

### ASSERT-First Strategy

Instead of defensive programming:
```cpp
// ✗ WRONG - Defensive null check hides bugs
if (m_replayEngine != nullptr) {
    m_replayEngine->pauseReplay();
}

// ✓ CORRECT - Assert expectation upfront
OBJ_ASSUME_DIFF(m_replayEngine, nullptr);
m_replayEngine->pauseReplay();
```

### Connection Safety

Always use unique connections and verify:
```cpp
bool connected = connect(sender, &Sender::signal, 
                        receiver, &Receiver::slot,
                        Qt::UniqueConnection);
OBJ_ASSUME_EQ(connected, true); // Assert connection succeeded and was unique
```

### Return Value Checks

Mark functions with `[[nodiscard]]` when return must be checked:
```cpp
[[nodiscard]] bool loadData();  // Caller must check result
```

### Pointer Validation

Check dynamically allocated objects:
```cpp
QNetworkReply* reply = networkManager->get(request);
Q_CHECK_PTR(reply); // Validates allocation succeeded
```

## Building and Testing

### Build from Src Directory Context

When making changes in specific subdirectories, build from root:
```bash
# From repository root
cmake --build build/GUI -j$(nproc)
```

### Formatting

Format all changed files:
```bash
find Src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

### Logging

Use Qt logging categories (defined per component):
```cpp
Q_LOGGING_CATEGORY(TSClientLog, "TSClient")
qCDebug(TSClientLog) << "Debug message";
qCInfo(TSClientLog) << "Info message";
qCWarning(TSClientLog) << "Warning message";
```

## Related Agent Instructions

Navigate to subdirectory AGENTS.md files for detailed component information:
- `Clients/TSClient/AGENTS.md`: TradeStation API client
- `Core/AGENTS.md`: Core application components
- `Algo/AGENTS.md`: Trading algorithm logic
- `FrontEnd/AGENTS.md`: UI implementations
- `Strategy/AGENTS.md`: Strategy plugin system
- `Misc/AGENTS.md`: Utilities and helpers
- `SQL/AGENTS.md`: SQL query management
