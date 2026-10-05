# OpenTraderPlatform Project - Agent Instructions

This file provides AI agents with key information about the OpenTraderPlatform project structure, architecture, and development workflows.

## Project Overview

**OpenTraderPlatform** is a real-time algorithmic trading application built with Qt6 that monitors stock market data, executes trading strategies, and provides comprehensive market analysis tools. It uses **TradeStation** for brokerage plus live/sim market data (bars, Level 2, quotes), and **Databento** for replay/download workflows and historical replay data.

**Architecture Pattern**: Model-View-Controller (MVC)
- **Model**: Data structures (Bar, Level2, Trade, Position, Order) in `Src/Core/Models/`
- **View**: `GUIFrontend` (Qt Widgets)
- **Controller**: MainAlgo (trading algorithm coordination)

## Quick Facts

- **Languages**: C++23, QML (minimal), CMake, Shell scripts
- **Framework**: Qt6 (Core, Network, SQL, Widgets) + QCustomPlot + databento-cpp (in `Lib/`)
- **Build System**: CMake 3.16+ with Ninja recommended
- **Target**: Linux (Ubuntu 24.04), cross-platform (X86_64, ARM64)
- **Lines of Code**: ~10,000+
- **Automation Binary**: `opentraderplatform-mcp-server` under `build/bin/`

## Navigation to Detailed Documentation

### Component-Specific AGENTS.md Files

For detailed information about specific subsystems, navigate to:

**Source Code Structure**:
- **Src/AGENTS.md** - Source directory organization, architectural patterns, memory management rules

**Core Components**:
- **Src/Core/AGENTS.md** - MainApp orchestration, MemoryMonitor, OrdersDatabase
- **Src/Core/Cache/BarCache/AGENTS.md** - Two-tier caching (memory + SQLite), thread-safe access
- **Src/Core/Replay/AGENTS.md** - Market data replay system, recording, playback controls
- **Src/Core/Replay/OrderEmulator/AGENTS.md** - Order/position emulation for replay mode

**API Communication**:
- **Src/Clients/TSClient/AGENTS.md** - TradeStation API client (brokerage + live/sim market data)
- **Src/Clients/DBClient/** - Databento client (replay/download data, optional live session support)
- **MCP/** - Local control-socket bridge and MCP stdio server for automation/OpenClaw

**Trading Logic**:
- **Src/Algo/AGENTS.md** - MainAlgo coordinator, receivers (bars, positions, orders), data processing

**User Interfaces**:
- **Src/FrontEnd/AGENTS.md** - Abstract FrontEnd interface, data flow, threading
- **Src/FrontEnd/GUI/AGENTS.md** - Qt Widgets GUI, charts, market depth, order entry

**Infrastructure**:
- **Src/Misc/AGENTS.md** - Constants (CONSTANTS.h), logging, settings, secure storage, utilities
- **Src/SQL/AGENTS.md** - SQL query centralization and organization

**Extensions**:
- **Src/Strategy/AGENTS.md** - Strategy management infrastructure (loading, threading, SDK)
- **Strategies/Examples/** - Public external-strategy SDK examples
- **Strategies/Tests/** - Opt-in crash and order-flow test harnesses

### Comprehensive Documentation (Doc/)
- **Doc/ARCHITECTURE.md** - Complete architecture, threading model, design patterns
- **Doc/AUTHENTICATION.md** - OAuth 2.0 flow, token management, security
- **Doc/RISK_MANAGEMENT.md** - Risk engine formulas, persistence, and GUI behavior
- **Doc/MANAGED_BRACKETS.md** - Platform-owned bracket engine (virtual + native + chart drag-adjust)
- **Doc/STRATEGY.md** - Strategy process runtime documentation
- **Doc/DEVELOPMENT.md** - Development setup and guidelines
- **Doc/CONTRIBUTING.md** - Contribution guidelines

## Building the Project

### Quick Build Commands

```bash
# GUI version (default)
mkdir -p build
cmake -Wno-deprecated -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=OFF
cmake --build build -j4

# Format code before committing
find Src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

### Build Times
- Configure: ~10-15 seconds
- Full build: ~2-5 minutes (with Ninja)
- Incremental build: ~10-30 seconds

## Essential Coding Conventions

> **Note**: For complete coding guidelines, see `.github/copilot-instructions.md` and component-specific AGENTS.md files.

### Quick Reference

**Memory Management**:
- Composition > Pointers when possible
- Qt parent-child for Qt objects with parents
- `QThread m_thread;` (stack-allocated, NOT `QThread*`)

**Naming**:
- Members: `m_barCache`, Globals: `g_mainAlgo`, Parameters: `p_symbol`

**Centralization**:
- Constants → `Src/Misc/CONSTANTS.h`
- SQL Queries → `Src/SQL/<ClassName>Queries.h`
- Documentation → `Doc/` folder

**ASSERT-First**:
```cpp
// Pre-conditions (expected state)
OBJ_ASSUME_DIFF(m_stream, nullptr);  // Assert upfront
m_stream->start();                    // Use without checks

// Post-conditions (validate results)
auto result = processData();
VALUE_ASSUME_GT(result, 0);          // Ensure valid output
```
Use ASSUME macros from `Src/Misc/Assume.h` for both pre-conditions and post-conditions.

## Testing

```bash
cmake --build build --target test
```

## CI/CD

GitHub Actions workflows:
- **build.yml**: Matrix builds (X86_64, ARM64), Debian packaging
- **container-build-and-upload.yml**: Docker containers

## Important Notes for Agents

1. **Custom instructions**: Full project instructions in `.github/copilot-instructions.md`
2. **Documentation first**: Check `Doc/` folder before making architectural changes
3. **Build before commit**: Always build and test before using report_progress
4. **Format code**: Run clang-format on changed files
5. **Keep changes minimal**: Make surgical, precise changes
6. **Security**: Run codeql_checker and gh-advisory-database before finalizing

## Where to Find Information

### Need to Understand:
- **Brokerage + Live/Sim Market Data?** → `Src/Clients/TSClient/AGENTS.md`
- **Replay/Download Data (Databento)?** → `Src/Clients/DBClient/`
- **Trading Logic?** → `Src/Algo/AGENTS.md`
- **Risk engine (limits, drawdown basis, lock/cooldown, persistence)?** → `Doc/RISK_MANAGEMENT.md`, `Src/Algo/AGENTS.md`, `Src/SQL/AGENTS.md`
- **Caching System?** → `Src/Core/Cache/BarCache/AGENTS.md`
- **GUI Components?** → `Src/FrontEnd/GUI/AGENTS.md`
- **Dynamic Timescale?** → `Src/FrontEnd/GUI/StockPriceChart/AGENTS.md` (search "Dynamic Timescale Feature")
- **Managed bracket engine?** → `Doc/MANAGED_BRACKETS.md`, `Src/Algo/AGENTS.md`, `Src/Strategy/AGENTS.md`
- **Constants/Utilities?** → `Src/Misc/AGENTS.md`
- **SQL Patterns?** → `Src/SQL/AGENTS.md`
- **Strategy runtime (external processes)?** → `Src/Strategy/AGENTS.md`
- **Overall Architecture?** → `Doc/ARCHITECTURE.md`
- **OAuth/Security?** → `Doc/AUTHENTICATION.md`

## Key Features

### Dynamic Timescale (Chart)

The GUI chart supports multiple timeframes with seamless switching:
- **Manual selection**: Keyboard shortcuts `1-9` for 1m, 5m, 15m, 30m, 1h, 4h, 1d, 1w, 1M
- **Auto-timeframe**: Automatically switches timeframe based on zoom level (configurable thresholds in Config tab)
- **Range preservation**: X and Y axis ranges preserved when switching
- **Candle alignment**: Left edge aligned with bar open time
- See `Src/FrontEnd/GUI/StockPriceChart/AGENTS.md` for implementation details.

### Risk Management Engine

- Dedicated `Risk Management` tab (second tab after Trade) for account-scoped limits and drawdown-basis selection
- Compact `RiskStatusWidget` above Time & Sales showing remaining drawdown headroom and lock/cooldown state
- Central risk gate in `MainAlgo::processPlaceOrder(...)` shared by GUI and strategy order paths
- Live/sim risk runtime persistence in ledger DB; replay risk state is per-session and resets on replay restart
- Strategy user-confirm flow supports preview brackets with risk validation and a hard mute mode (`M`) that auto-rejects confirmations

Start with the root AGENTS.md (this file), then navigate to component-specific files as needed.
