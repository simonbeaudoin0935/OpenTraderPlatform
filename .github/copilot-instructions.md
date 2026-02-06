# Copilot Instructions for L2Trader Repository

## Navigation Guide

For detailed information about specific components, see the AGENTS.md files:
- **Root AGENTS.md**: Project overview, architecture, and navigation to subsystems
- **Src/AGENTS.md**: Source code structure and patterns
- **Src/Clients/TSClient/AGENTS.md**: TradeStation API client details
- **Src/Core/AGENTS.md**: Core application components
- **Src/Algo/AGENTS.md**: Trading algorithm coordination
- **Src/FrontEnd/AGENTS.md**: Frontend architecture (GUI/TUI)
- **Src/Misc/AGENTS.md**: Utilities, constants, logging
- **Src/SQL/AGENTS.md**: SQL query management
- **Src/Strategy/AGENTS.md**: Strategy plugin system

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

# Format code before committing (REQUIRED)
find Src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

### Build Times
- Configure: ~10-15 seconds
- Full build: ~2-5 minutes (with Ninja)
- Incremental: ~10-30 seconds

### Run Application

```bash
# GUI mode
./build/GUI/Src/L2Trader

# TUI mode (stdout is ncurses UI, stderr is logging)
./build/TUI/Src/L2Trader 2>app.log
```


## Core Coding Guidelines

### ASSERT-First Strategy
Instead of defensive null checks, assert expected state upfront:
```cpp
// ✗ WRONG - Defensive check hides bugs
if (m_replayEngine != nullptr) {
    m_replayEngine->pauseReplay();
}

// ✓ CORRECT - Assert expectation upfront (pre-condition)
OBJ_ASSUME_DIFF(m_replayEngine, nullptr);
m_replayEngine->pauseReplay();

// ✓ CORRECT - Assert valid results (post-condition)
auto result = processData();
VALUE_ASSUME_GT(result, 0);  // Ensure valid output
```
Use ASSUME macros from `Src/Misc/Assume.h` for both pre-conditions and post-conditions instead of Q_ASSERT.

### Naming Conventions
- Member variables: `m_` prefix (e.g., `m_barCache`)
- Global variables: `g_` prefix (e.g., `g_mainAlgo`)
- Parameters: `p_` prefix (e.g., `p_symbol`)

### Code Style
- Early exit: Handle errors first, minimize indentation
- [[nodiscard]]: Mark functions where return values must be checked
- Qt::UniqueConnection: Always use for signal/slot connections
- Q_CHECK_PTR(): Always validate dynamically allocated objects

### Centralization Rules
- **Constants**: ALL in `Src/Misc/CONSTANTS.h` with appropriate namespaces
- **SQL Queries**: ALL in `Src/SQL/<ClassName>Queries.h` headers
- **Documentation**: New .md files go in `Doc/` folder

### Memory Management
- **Composition first**: Use direct member objects when possible
- **Qt parent-child**: For Qt objects with parents (Qt manages memory)
- **QPointer**: For Qt objects with uncertain lifetime
- **std::unique_ptr**: For exclusive ownership
- **std::shared_ptr**: For shared ownership across async operations
- **Stack-allocated threads**: `QThread m_thread;` NOT `QThread* m_thread;`

### Threading Pattern
Always use stack-allocated threads with proper cleanup:
```cpp
~MyClass() {
    m_thread.quit();
    if (!m_thread.wait(5000)) {
        qWarning() << "Thread did not finish, terminating";
        m_thread.terminate();
        m_thread.wait();
    }
}
```

### Time Handling
Use Qt time classes with proper timezone (always NewYork for stock market):
- QDateTime/QTime/QDate with QTimeZone
- NEVER use std::chrono or raw time_t/struct tm


## Prerequisites

- **Qt6**: Version 6.4.2+ (Ubuntu 24.04 default)
- **CMake**: 3.16+
- **Compiler**: GCC 7+ or Clang 5+ with C++23 support
- **SQLite**: For database functionality
- **Build optimization**: ccache (cached in CI)

## Development Workflow

### VSCode Tasks
Use predefined tasks in `.vscode/tasks.json`:
- **build**: Builds everything

### Testing
```bash
cmake --build build/GUI --target test
```

### CI/CD
GitHub Actions runs on push/PR:
- Matrix builds (X86_64, ARM64)
- Debian packaging
- Installation tests

## Project Architecture

**Pattern**: Model-View-Controller (MVC)
- **Model**: Data structures (Bar, Position, Order, MarketDepthQuote)
- **View**: FrontEnd (GUIFrontend/TUIFrontend)
- **Controller**: MainAlgo (trading algorithm coordination)

**Key Directories**:
- `Src/`: Source code (see Src/AGENTS.md for details)
- `Doc/`: Documentation (ARCHITECTURE.md, AUTHENTICATION.md, etc.)
- `Tests/`: Unit tests

**Singletons** (run in dedicated threads):
- **TSClient**: TradeStation API client
- **MainAlgo**: Trading algorithm coordinator
- **LogBroadcaster**: Centralized logging

For detailed component information, refer to the AGENTS.md files in each directory.
