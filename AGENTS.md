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
- **Framework**: Qt6 (Core, Network, SQL, Widgets, Charts, WebEngineWidgets)
- **Build System**: CMake 3.16+ with Ninja recommended
- **Target**: Linux (Ubuntu 24.04), cross-platform (X86_64, ARM64)
- **Lines of Code**: ~10,000+
- **Main Executable**: L2Trader (GUI or TUI mode)
- **Companion Executable**: Recorder (for data collection)

## Key Directories

### Source Code (`Src/`)
- **Clients/TSClient/**: TradeStation API client - REST and WebSocket communication
- **Core/**: Main application logic, cache management, replay functionality
- **Algo/**: Trading algorithm components (MainAlgo, receivers, processors)
- **FrontEnd/**: UI implementations (GUI with Qt Widgets, TUI with ncurses)
- **Misc/**: Utilities (logging, settings, secure storage, constants)
- **SQL/**: Centralized SQL query definitions
- **Strategy/**: Plugin-based strategy system
- **Recorder/**: Data recording companion app

### Documentation (`Doc/`)
- **ARCHITECTURE.md**: Complete architecture details, threading model, design patterns
- **AUTHENTICATION.md**: OAuth 2.0 flow, token management, security
- **FRONTEND.md**: GUI/TUI implementation details
- **STRATEGY.md**: Strategy plugin system documentation
- **DEVELOPMENT.md**: Development setup and guidelines
- **CONTRIBUTING.md**: Contribution guidelines

### Build Artifacts
- `build/`: Build output directory (not in repo)
- `.vscode/tasks.json`: VSCode tasks for building

## Core Components

### Singletons
- **TSClient**: TradeStation API client (runs in dedicated thread)
- **MainAlgo**: Trading algorithm coordinator (runs in dedicated thread)
- **LogBroadcaster**: Centralized logging distribution

### Key Classes
- **MainApp**: Application orchestrator (lives in main thread)
- **StockInstruments**: Per-symbol data container (BarCache, MarketDepthQuoteReceiver)
- **BarCache**: Bar data caching with SQLite persistence
- **GUIFrontend/TUIFrontend**: UI implementations conforming to FrontEnd interface

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

## Coding Conventions

### Memory Management
1. **Prefer composition over pointers**: Use direct member objects when possible
2. **Qt parent-child ownership**: For Qt objects with parents, no smart pointers needed
3. **QPointer**: For Qt objects that may be deleted independently (streams)
4. **std::unique_ptr**: For exclusive ownership (non-Qt or Qt without parents)
5. **std::shared_ptr**: For shared ownership (async operations, data sharing)
6. **Stack-allocated threads**: Use `QThread m_thread` not `QThread* m_thread`

### Code Style
- **Member variables**: Prefix with `m_` (e.g., `m_barCache`)
- **Global variables**: Prefix with `g_` (e.g., `g_mainAlgo`)
- **Parameters**: Prefix with `p_` (e.g., `p_symbol`)
- **Early exit**: Handle errors first, then main logic (minimize indentation)
- **Assertions**: Use ASSUME macros from `Src/Misc/Assume.h` instead of Q_ASSERT
- **ASSERT-First Strategy**: Assert expected state upfront, don't use defensive null checks
- **[[nodiscard]]**: Mark functions where return values should not be ignored

### Constants Management
- **ALL** constants defined in `Src/Misc/CONSTANTS.h` in appropriate namespaces
- Never define constants inline in source files
- Reference via namespace: `TradingHours::TRADING_START_TIME`

### SQL Queries
- **ALL** SQL queries defined in `Src/SQL/<ClassName>Queries.h`
- Each class has its own header with namespace
- Never write SQL directly in implementation files

### Threading Patterns
- Stack-allocated thread members: `QThread m_thread;`
- Proper cleanup in destructors with quit/wait/terminate pattern
- Thread affinity assertions in debug builds
- Signal/slot cross-thread communication with Qt::QueuedConnection

## Testing

```bash
# Run tests
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

## Related Agent Instructions

- `Src/AGENTS.md`: Source code structure details
- `Src/Clients/TSClient/AGENTS.md`: TradeStation API client specifics
- `Src/Core/AGENTS.md`: Core application components
- `Src/Algo/AGENTS.md`: Trading algorithm details
- `Src/FrontEnd/AGENTS.md`: Frontend architecture
- `Src/Strategy/AGENTS.md`: Strategy plugin system

## Getting Help

For detailed information on specific subsystems, refer to:
1. Corresponding AGENTS.md file in that directory
2. Documentation in `Doc/` folder
3. Source code comments and headers
