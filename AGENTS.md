# L2Trader Project - Agent Instructions

This file provides AI agents with key information about the L2Trader project structure, architecture, and development workflows.

## Project Overview

**L2Trader** is a real-time algorithmic trading application built with Qt6 that monitors stock market data, executes trading strategies, and provides comprehensive market analysis tools. It connects to TradeStation APIs for live market data, Level 2 market depth visualization, and position tracking.

**Architecture Pattern**: Model-View-Controller (MVC)
- **Model**: Data structures (Bar, Position, Order, MarketDepthQuote)
- **View**: FrontEnd (GUIFrontend/TUIFrontend)
- **Controller**: MainAlgo (trading algorithm coordination)

## Quick Facts

- **Languages**: C++23, QML (minimal), CMake, Shell scripts
- **Framework**: Qt6 (Core, Network, SQL, Widgets) + QCustomPlot (third-party charting library)
- **Build System**: CMake 3.16+ with Ninja recommended
- **Target**: Linux (Ubuntu 24.04), cross-platform (X86_64, ARM64)
- **Lines of Code**: ~10,000+
- **Main Executable**: L2Trader (GUI or TUI mode)

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
- **Src/Clients/TSClient/AGENTS.md** - TradeStation API client, OAuth flow, async requests, streams

**Trading Logic**:
- **Src/Algo/AGENTS.md** - MainAlgo coordinator, receivers (bars, positions, orders), data processing

**User Interfaces**:
- **Src/FrontEnd/AGENTS.md** - Abstract FrontEnd interface, data flow, threading
- **Src/FrontEnd/GUI/AGENTS.md** - Qt Widgets GUI, charts, market depth, order entry
- **Src/FrontEnd/TUI/AGENTS.md** - ncurses terminal interface

**Infrastructure**:
- **Src/Misc/AGENTS.md** - Constants (CONSTANTS.h), logging, settings, secure storage, utilities
- **Src/SQL/AGENTS.md** - SQL query centralization and organization

**Extensions**:
- **Src/Strategy/AGENTS.md** - Strategy management infrastructure (loading, threading, SDK)
- **Strategies/** - Template and test strategy implementations (shared objects loaded by Src/Strategy)

### Comprehensive Documentation (Doc/)
- **Doc/ARCHITECTURE.md** - Complete architecture, threading model, design patterns
- **Doc/AUTHENTICATION.md** - OAuth 2.0 flow, token management, security
- **Doc/FRONTEND.md** - GUI/TUI implementation details
- **Doc/STRATEGY.md** - Strategy plugin system documentation
- **Doc/DEVELOPMENT.md** - Development setup and guidelines
- **Doc/CONTRIBUTING.md** - Contribution guidelines

## Building the Project

### Quick Build Commands

```bash
# GUI version (default)
mkdir -p build/GUI
cmake -S . -B build/GUI -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=ON -DBUILD_TESTS=OFF
cmake --build build/GUI -j$(nproc)

# TUI version (headless)
mkdir -p build/TUI
cmake -S . -B build/TUI -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=OFF -DBUILD_TESTS=OFF
cmake --build build/TUI -j$(nproc)

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
cmake --build build/GUI --target test
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
7. **Logs to stderr**: In TUI mode, logs go to stderr to not interfere with ncurses

## Where to Find Information

### Need to Understand:
- **API Communication?** → `Src/Clients/TSClient/AGENTS.md`
- **Trading Logic?** → `Src/Algo/AGENTS.md`
- **Caching System?** → `Src/Core/Cache/BarCache/AGENTS.md`
- **GUI Components?** → `Src/FrontEnd/GUI/AGENTS.md`
- **Constants/Utilities?** → `Src/Misc/AGENTS.md`
- **SQL Patterns?** → `Src/SQL/AGENTS.md`
- **Strategy Plugins?** → `Src/Strategy/AGENTS.md`
- **Overall Architecture?** → `Doc/ARCHITECTURE.md`
- **OAuth/Security?** → `Doc/AUTHENTICATION.md`

Start with the root AGENTS.md (this file), then navigate to component-specific files as needed.
