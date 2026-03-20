# L2Trader Development Guide

## Table of Contents

1. [Development Setup](#development-setup)
2. [Building](#building)
3. [Testing](#testing)
4. [Code Quality Tools](#code-quality-tools)
5. [Coding Guidelines](#coding-guidelines)
6. [Constants & SQL Management](#constants--sql-management)
7. [Common Patterns](#common-patterns)

## Development Setup

### Prerequisites

**Required**:
- **Qt 6.4.2+** (Core, Network, SQL, Widgets, PrintSupport)
- **C++23 compliant compiler** (GCC 11+ or Clang 12+)
- **CMake 3.16+** with Ninja (recommended)
- **SQLite** (included with Qt SQL)
- **libssl-dev** — Required by `databento-cpp` (OpenSSL)
- **libzstd-dev** — Required by `databento-cpp` (Zstandard decompression for `.dbn.zst` files)
- **databento-cpp** — Bundled as a submodule in `Lib/`; built automatically by CMake

**Optional (Recommended)**:
- **clang-format** — Code formatting (pre-commit hook)
- **ccache** — Build acceleration (80–90% faster incremental builds)
- **libqtkeychain-qt6-dev** — Secure OAuth token storage (OS-level encryption); without it, XOR-obfuscated fallback is used
- **libncurses-dev** — TUI mode support
- **ninja-build** — Faster builds than make

### Installation

#### Ubuntu/Debian

```bash
# Essential packages
sudo apt-get install qt6-base-dev libqt6sql6-sqlite cmake libssl-dev libzstd-dev ninja-build

# Optional (recommended for development)
sudo apt-get install clang-format ccache libqtkeychain-qt6-dev libncurses-dev

# For GUI integration testing
sudo apt-get install xvfb xdotool x11-utils imagemagick
```

#### macOS (Homebrew)

```bash
# Essential packages
brew install qt@6 cmake openssl zstd ninja

# Optional (recommended for development)
brew install clang-format ccache qtkeychain
```

### IDE Setup

#### VSCode

```bash
# Install C++ and CMake extensions
code --install-extension ms-vscode.cpptools
code --install-extension ms-vscode.cmake-tools
```

Predefined tasks in `.vscode/tasks.json`:
- `build`: Full build (GUI + TUI)
- `build-gui`: GUI-only build
- `build-tui`: TUI-only build
- `clean`: Clean build artifacts
- `format`: Format all source files

#### Qt Creator

1. Open `CMakeLists.txt`
2. Configure build kit (Qt 6.4.2+)
3. Select run configuration "L2Trader"
4. Build and run

### Git Hooks Setup

Install the pre-commit hook for automatic formatting checks:

```bash
./Utils/install-git-hooks.sh
```

The hook runs `clang-format` on staged files before each commit. If formatting issues are found, the commit is rejected with instructions to fix.

---

## Building

### Build Commands

Always run from the repository root:

> **Important**: Do not use `-j$(nproc)` in this repository's documented commands.
> The shell security policy blocks nested command substitution. Use a literal
> value such as `-j4`, or compute the core count first and pass the expanded
> value separately.

```bash
# GUI version (default) — recommended for development
mkdir -p build/GUI
cmake -S . -B build/GUI -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=ON -DBUILD_TESTS=OFF
cmake --build build/GUI -j4

# TUI version (headless)
mkdir -p build/TUI
cmake -S . -B build/TUI -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=OFF -DBUILD_TESTS=OFF
cmake --build build/TUI -j4

# Clean build
rm -rf build/
```

Useful build outputs:

- `build/GUI/bin/l2trader-mcp-server`
- `build/TUI/bin/l2trader-mcp-server`
- bundled sample strategy executables under the matching `build/<config>/bin/`

There is **no shared top-level `build/bin/`**.

To stage the install-style external strategy SDK from a GUI build tree:

```bash
cmake --build build/GUI --target stage-strategy-sdk
```

### Build Configurations

#### Debug (Development)

```bash
cmake -S . -B build/GUI -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=ON -DBUILD_TESTS=ON
```

Features: debug symbols (`-g`), assertions enabled, no optimization, detailed logging.

#### Release (Production)

```bash
cmake -S . -B build/GUI -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_GUI=ON -DBUILD_TESTS=OFF
```

Features: optimized (`-O3`), assertions disabled, minimal logging, smaller binary.

### Running the Application

```bash
# GUI mode
./build/GUI/Src/L2Trader

# TUI mode (stdout = ncurses UI, stderr = logs)
./build/TUI/Src/L2Trader 2>app.log
```

---

## Testing

### Unit Tests

```bash
# Build with tests enabled
cmake -S . -B build/test -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DENABLE_GUI=ON
cmake --build build/test -j4

# Run all tests
ctest --test-dir build/test --output-on-failure
```

### GUI Integration Testing

```bash
# Requires xvfb, xdotool, x11-utils, imagemagick
./Utils/test-gui.sh
```

What it tests: application launch, window visibility, basic UI interactions, clean shutdown.
Screenshots saved to `tests/screenshots/`.

### Test Structure

```
Tests/
└── CMakeLists.txt    # Unit tests are added here as the project grows
```

### Writing Tests

```cpp
#include <QtTest/QtTest>

class TestBarCache : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();    // Run once before all tests
    void init();            // Run before each test
    void testAddBar();
    void testGetBars();
    void cleanup();
    void cleanupTestCase();
};

QTEST_MAIN(TestBarCache)
#include "test_barcache.moc"
```

---

## Code Quality Tools

### 1. UndefinedBehaviorSanitizer (UBSan)

```bash
cmake -S . -B build/ubsan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_UBSAN=ON -DENABLE_GUI=ON
cmake --build build/ubsan -j4
./build/ubsan/Src/L2Trader
```

Detects: integer overflows, division by zero, null pointer dereferences, invalid shifts, out-of-bounds array access.
Overhead: ~20–30% runtime slowdown.

### 2. AddressSanitizer (ASan)

```bash
cmake -S . -B build/asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_GUI=ON
cmake --build build/asan -j4
./build/asan/Src/L2Trader
```

Detects: use-after-free, heap/stack/global buffer overflows, memory leaks, use-after-scope.
Overhead: ~2× runtime slowdown; ~2–3× memory usage.

> **Note**: UBSan and ASan cannot be used simultaneously. Run separately.

### 3. Code Formatting

```bash
# Format all source files (run before committing)
find Src -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Check formatting without modifying (CI uses this)
clang-format --dry-run --Werror <file>
```

**CI Enforcement**: Formatting is checked on every push and PR.

### 4. Compiler Warnings

The project uses strict warnings treated as errors:
```cmake
-Wall -Wextra -Wpedantic
-Wcast-align -Wcast-qual
-Wdouble-promotion
-Wformat=2
-Wimplicit-fallthrough
-Wnon-virtual-dtor
-Wnull-dereference
-Woverloaded-virtual
-Wshadow
-Wunused
-Werror
```

---

## Coding Guidelines

### General Style

- **Standard**: C++23
- **Indentation**: 4 spaces (no tabs)
- **Line endings**: Unix (LF)
- **Header guards**: `#pragma once`
- **Error handling**: Early return/exit to minimize indentation nesting

### Naming Conventions

```cpp
// Member variables: m_ prefix
class MyClass {
    int m_count;
    QString m_name;
};

// Global variables: g_ prefix
int g_instanceCount;

// Parameters: p_ prefix
void function(int p_value, const QString& p_name);

// Local variables: camelCase (no prefix)
int localCount = 0;

// Constants: UPPER_SNAKE_CASE
constexpr int MAX_RETRIES = 3;

// Methods/functions: camelCase
void processData();

// Classes: PascalCase
class StockPriceChart;
```

### ASSUME Macros (Mandatory — Use Instead of Defensive Checks)

Use `ASSUME` macros from `Src/Misc/Assume.h` for pre-conditions and post-conditions. Defensive null-checks hide bugs; ASSUME macros catch them early in debug builds.

```cpp
#include "Misc/Assume.h"

// ✓ CORRECT — Assert pre-condition then use
OBJ_ASSUME_DIFF(m_replayEngine, nullptr);
m_replayEngine->pauseReplay();

// ✗ WRONG — Silent failure hides bugs
if (m_replayEngine != nullptr) {
    m_replayEngine->pauseReplay();
}

// Post-conditions
auto result = processData();
ASSUME_GT(result, 0);

// Acceptable — destructor cleanup
~MyClass() {
    if (m_resource)
        m_resource->cleanup();
}
```

Available macros: `OBJ_ASSUME_DIFF`, `OBJ_ASSUME_TRUE`, `OBJ_ASSUME_FALSE`, `OBJ_ASSUME_EQUAL`, `OBJ_ASSUME_GT/GTE/LT/LTE` (and non-OBJ variants using function name instead of object name).

### Signal/Slot Connections

Always use `Qt::UniqueConnection`:

```cpp
bool connected = connect(sender, &Sender::signal,
                         receiver, &Receiver::slot,
                         Qt::UniqueConnection);
OBJ_ASSUME_TRUE(connected);  // Assert uniqueness
```

### Return Value Checking

```cpp
[[nodiscard]] bool saveToDatabase();
[[nodiscard]] std::optional<Bar> getBar(int index);
```

### Memory Management

```cpp
// Composition first — direct member (preferred)
class SymbolContext {
    BarCache m_barCache;              // ✓ Direct member
    Level2Receiver m_level2Receiver;  // ✓ Direct member
};

// Qt parent-child (no smart pointer needed)
QTimer* timer = new QTimer(this);

// QPointer for Qt objects with uncertain lifetime
QPointer<StreamOrders> m_streamOrders;

// unique_ptr for exclusive ownership
std::unique_ptr<ReplayEngine> m_replayEngine;

// shared_ptr for shared ownership (e.g., Bar vectors)
std::shared_ptr<QVector<Bar>> bars;
```

### Stack-Allocated Threads (Mandatory Pattern)

```cpp
class TSClient {
    QThread m_thread;  // ✓ Stack-allocated member

    ~TSClient() {
        m_thread.quit();
        if (!m_thread.wait(5000)) {
            qWarning() << "Thread did not finish, terminating";
            m_thread.terminate();
            m_thread.wait();
        }
    }
};
```

### Signal Documentation (Required for New Signals)

```cpp
signals:
    /**
     * @brief Emitted when a new Level 2 snapshot is received for the displayed symbol.
     * Thread context: Emitted from Databento callback thread (auto-queued to receiver).
     * @param symbol  Ticker string
     * @param level2  10-level bid/ask snapshot
     */
    void newLevel2(const QString& symbol, const Level2& level2);
```

---

## Constants & SQL Management

### Constants

**ALL** application constants must be in `Src/Misc/CONSTANTS.h`:

```cpp
namespace TradingHours {
    constexpr QTime TRADING_START_TIME(9, 30, 0);
    constexpr QTime TRADING_END_TIME(16, 0, 0);
    inline const QTimeZone NEW_YORK_TZ("America/New_York");
}

namespace BarsConstants {
    constexpr int MINUTE_BARS_PER_DAY = 900;   // 4:00 AM – 6:59 PM XNAS.ITCH
}

namespace AuthConstants {
    constexpr int TOKEN_EXPIRY_SECONDS = 1200;       // 20 minutes
    constexpr int TOKEN_REFRESH_BUFFER_SECONDS = 5;  // Refresh 5 s early
}
```

Never define constants inline in source files unless they are truly private implementation details.

### SQL Queries

**ALL** SQL queries must be in `Src/SQL/<ClassName>Queries.h` files:

```cpp
// Src/SQL/OrdersDatabaseQueries.h
namespace OrdersDatabaseQueries {
    inline const QString CREATE_ORDERS_TABLE = R"(
        CREATE TABLE IF NOT EXISTS orders (
            order_id TEXT PRIMARY KEY,
            account_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            trade_action TEXT NOT NULL,
            status TEXT NOT NULL,
            timestamp TEXT NOT NULL
        )
    )";

    inline const QString INSERT_ORDER = R"(
        INSERT OR REPLACE INTO orders
        (order_id, account_id, symbol, trade_action, status, timestamp)
        VALUES (?, ?, ?, ?, ?, ?)
    )";
}
```

Usage:
```cpp
#include "SQL/OrdersDatabaseQueries.h"
QSqlQuery query;
query.prepare(OrdersDatabaseQueries::CREATE_ORDERS_TABLE);
query.exec();
```

---

## Common Patterns

### Error Handling

```cpp
// std::expected for fallible operations
QFuture<std::expected<QVector<Bar>, Error>> getBars();

// std::optional for optional values
std::optional<Bar> getBar(int index);

// Early return for error cases
if (!file.open(QIODevice::ReadOnly)) {
    qCCritical() << "Failed to open file:" << file.fileName();
    return false;
}
// Happy path continues here
```

### Async Operations

```cpp
// Use QtConcurrent::run for CPU or I/O work off the main thread
auto future = QtConcurrent::run([this, symbol, date]() -> std::expected<QVector<Bar>, Error> {
    // Background work (e.g., Databento Historical call)
    return result;
});

auto* watcher = new QFutureWatcher<std::expected<QVector<Bar>, Error>>(this);
connect(watcher, &QFutureWatcher<std::expected<QVector<Bar>, Error>>::finished,
        this, [this, watcher]() {
            auto result = watcher->result();
            // Process on calling thread
            watcher->deleteLater();
        });
watcher->setFuture(future);
```

### Logging

```cpp
Q_LOGGING_CATEGORY(MyClassLog, "MyClass")

qCDebug(MyClassLog)    << "Debug message";
qCInfo(MyClassLog)     << "Info message";
qCWarning(MyClassLog)  << "Warning message";
qCCritical(MyClassLog) << "Critical error";

// Never log sensitive data
qCDebug(dbClientLog) << "Subscribing to symbol" << symbol;  // ✓ OK
qCDebug(dbClientLog) << "API key:" << apiKey;               // ✗ NEVER
```

### Documentation

```cpp
/**
 * @brief Retrieves bars for the specified date range.
 * @param start Start date (inclusive)
 * @param end   End date (inclusive)
 * @return Vector of bars, or empty if none found.
 *
 * Checks the in-memory cache first; queries SQLite if needed.
 * Results are cached for subsequent requests.
 */
QVector<Bar> getBars(const QDate& start, const QDate& end) const;
```

---

## Related Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) — System architecture and components
- [AUTHENTICATION.md](AUTHENTICATION.md) — OAuth 2.0 and Databento API key management
- [FRONTEND.md](FRONTEND.md) — GUI and TUI implementation
- [CONTRIBUTING.md](CONTRIBUTING.md) — Contribution process
- [STRATEGY.md](STRATEGY.md) — Strategy process development
