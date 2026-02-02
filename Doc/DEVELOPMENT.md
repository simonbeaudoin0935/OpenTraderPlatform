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
- **CMake 3.16+**
- **SQLite** (included with Qt SQL)
- **QCustomPlot** (included in repository)

**Optional (Recommended)**:
- **clang-format** - Code formatting (pre-commit hook)
- **ccache** - Build acceleration
- **QKeychain** - Secure credential storage
- **ncurses** - TUI mode support

### Installation

#### Ubuntu/Debian

```bash
# Essential packages
sudo apt-get install qt6-base-dev libqt6sql6-sqlite cmake

# Optional (recommended for development)
sudo apt-get install clang-format ccache libqtkeychain-qt6-dev libncurses-dev

# For GUI testing
sudo apt-get install xvfb xdotool x11-utils imagemagick
```

#### macOS (Homebrew)

```bash
# Essential packages
brew install qt@6 cmake

# Optional (recommended for development)
brew install clang-format ccache qtkeychain
```

### IDE Setup

#### VSCode

```bash
# Install C++ extensions
code --install-extension ms-vscode.cpptools
code --install-extension ms-vscode.cmake-tools

# Project configuration in .vscode/
.vscode/
├── tasks.json          # Build tasks
├── launch.json         # Debug configurations
└── c_cpp_properties.json  # IntelliSense config
```

**Tasks** (`.vscode/tasks.json`):
- `build`: Full build (GUI + TUI)
- `build-gui`: GUI-only build
- `build-tui`: TUI-only build
- `clean`: Clean build artifacts
- `format`: Format all code files

#### Qt Creator

1. Open `CMakeLists.txt`
2. Configure build kit (Qt 6.4.2+)
3. Select run configuration "L2Trader"
4. Build and run

### Git Hooks Setup

Install pre-commit hook for automatic formatting checks:

```bash
./Utils/install-git-hooks.sh
```

The hook checks code formatting before each commit using `clang-format`. If formatting issues are found, the commit is rejected with instructions to fix.

## Building

### Build Commands

Always run commands from repository root:

```bash
# GUI version (default)
mkdir -p build/GUI
cmake -S . -B build/GUI -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=ON -DBUILD_TESTS=OFF
cmake --build build/GUI -j$(nproc)

# TUI version (headless)
mkdir -p build/TUI
cmake -S . -B build/TUI -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=OFF -DBUILD_TESTS=OFF
cmake --build build/TUI -j$(nproc)

# Clean build
rm -rf build/
```

### Build Configurations

#### Debug Build (Development)

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=ON -DBUILD_TESTS=ON ..
```

**Features**:
- Debug symbols (`-g`)
- Assertions enabled (`Q_ASSERT`)
- No optimization
- Detailed logging

#### Release Build (Production)

```bash
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_GUI=ON -DBUILD_TESTS=OFF ..
```

**Features**:
- Optimized (`-O3`)
- Assertions disabled (no-op)
- Minimal logging
- Smaller binary

### Build Speed Optimization

#### ccache

```bash
# Install ccache
sudo apt-get install ccache

# CMake automatically detects and uses ccache
```

**Performance**: 80-90% faster incremental builds.

#### Parallel Builds

```bash
# Use all CPU cores
cmake --build build -j$(nproc)

# Or specify core count
cmake --build build -j4
```

### Running the Application

```bash
# GUI mode
./build/GUI/Src/L2Trader

# TUI mode (logs to stderr)
./build/TUI/Src/L2Trader 2>logs.txt
```

## Testing

### Unit Tests

```bash
# Build with tests enabled
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build --parallel

# Run all tests
ctest --test-dir build --output-on-failure

# Run specific test
./build/Tests/test_barcache --stock-csv=Example_Config/nasdaq_screener.csv
./build/Tests/test_tradestationclient
```

### GUI Integration Testing

```bash
# Requires xvfb, xdotool, x11-utils, imagemagick
./Utils/test-gui.sh
```

**What it tests**:
- Application launch
- Window visibility
- Basic UI interactions
- Clean shutdown

Screenshots saved to `tests/screenshots/`.

### Test Structure

```
Tests/
├── CMakeLists.txt
├── test_barcache.cpp        # BarCache unit tests
├── test_tradestationclient.cpp  # TSClient unit tests
└── RecorderIntegrationTest.cpp  # Recorder integration tests
```

### Writing Tests

```cpp
#include <QtTest/QtTest>

class TestBarCache : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();    // Run once before all tests
    void init();            // Run before each test
    void testAddBar();      // Test case
    void testGetBars();     // Test case
    void cleanup();         // Run after each test
    void cleanupTestCase(); // Run once after all tests
};

QTEST_MAIN(TestBarCache)
#include "test_barcache.moc"
```

## Code Quality Tools

### 1. UndefinedBehaviorSanitizer (UBSan)

Detects undefined behavior at runtime:

```bash
cmake -S . -B build -DENABLE_UBSAN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/Src/L2Trader
```

**Detects**:
- Integer overflows
- Division by zero
- Null pointer dereferences
- Invalid shifts
- Out-of-bounds array access

**Overhead**: ~20-30% runtime slowdown

### 2. AddressSanitizer (ASan)

Detects memory errors:

```bash
cmake -S . -B build -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/Src/L2Trader
```

**Detects**:
- Use-after-free
- Heap/stack/global buffer overflows
- Memory leaks
- Use-after-scope

**Overhead**: ~2x runtime slowdown, 2-3x memory usage

**Note**: Cannot use simultaneously with UBSan. Run separately.

### 3. Code Formatting

```bash
# Check formatting (dry run)
clang-format --dry-run --Werror <file>

# Format file in-place
clang-format -i <file>

# Format all source files
find Src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

**CI Enforcement**: Formatting checked on every push/PR.

### 4. Compiler Warnings

Project uses strict compiler warnings:

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
-Werror  # Treat warnings as errors
```

## Coding Guidelines

### General Style

- **C++23 standard**
- **4 spaces indentation** (no tabs)
- **Unix line endings** (LF)
- **Header guards**: `#pragma once`
- **Early return/exit** for error handling (minimize indentation)

### Naming Conventions

```cpp
// Member variables: m_ prefix
class MyClass {
private:
    int m_count;
    QString m_name;
};

// Global variables: g_ prefix
int g_instanceCount;

// Parameters: p_ prefix
void function(int p_value, QString p_name);

// Local variables: camelCase (no prefix)
int localCount = 0;
QString userName = "admin";

// Constants: UPPER_SNAKE_CASE
const int MAX_RETRIES = 3;

// Functions/Methods: camelCase
void processData();
QString getUserName();

// Classes: PascalCase
class StockPriceChart;
class BarCache;
```

### Assertions & Checks

Use ASSUME macros from `Src/Misc/Assume.h` instead of Q_ASSERT:

```cpp
#include "Misc/Assume.h"

// Debug build: assertion, Release build: no-op
ASSUME(ptr != nullptr);
ASSUME(index >= 0 && index < size);

// Always check pointer allocations
Bar* bar = new Bar();
Q_CHECK_PTR(bar);
```

### Signal/Slot Connections

Always use `Qt::UniqueConnection` and assert uniqueness:

```cpp
bool connected = connect(sender, &Sender::signal,
                         receiver, &Receiver::slot,
                         Qt::UniqueConnection);
ASSUME(connected);  // Assert connection was unique
```

### Return Value Checking

Use `[[nodiscard]]` for functions where ignoring return value is an error:

```cpp
[[nodiscard]] bool saveToDatabase();
[[nodiscard]] std::optional<Bar> getBar(int index);

// Compiler warns if return value ignored
saveToDatabase();  // Warning!
```

### Memory Management

#### Composition First

```cpp
// PREFERRED: Direct member (composition)
class StockInstruments {
    BarCache m_barCache;              // ✅ Direct member
    MarketDepthQuoteReceiver m_mdqr;  // ✅ Direct member
};

// AVOID: Unnecessary pointers
class StockInstruments {
    BarCache* m_barCache;             // ❌ Pointer not needed
};
```

#### Smart Pointers

```cpp
// Qt parent-child ownership (no smart pointer needed)
QTimer* timer = new QTimer(this);  // Qt deletes when parent deleted

// QPointer for Qt objects with uncertain lifetime
QPointer<StreamBars> m_stream;  // Auto-nulls when stream deleted

// std::unique_ptr for exclusive ownership
std::unique_ptr<LiveStreamDB> m_database;

// std::shared_ptr for shared ownership (Bar vectors)
std::shared_ptr<QVector<Bar>> bars;
```

#### Threading

```cpp
// PREFERRED: Stack-allocated thread
class TSClient {
    QThread m_thread;  // ✅ Member variable
    
    ~TSClient() {
        m_thread.quit();
        if (!m_thread.wait(5000)) {
            m_thread.terminate();
            m_thread.wait();
        }
    }
};

// AVOID: Heap-allocated thread
QThread* m_thread = new QThread();  // ❌ Memory leak risk
```

## Constants & SQL Management

### Constants

**ALL** application constants must be in `Src/Misc/CONSTANTS.h`:

```cpp
namespace TradingHours {
    constexpr QTime TRADING_START_TIME(9, 30, 0);
    constexpr QTime TRADING_END_TIME(16, 0, 0);
    inline const QTimeZone NEW_YORK_TZ("America/New_York");
}

namespace TSClientEndpoints {
    inline const QString GET_BARS = "https://api.tradestation.com/v3/marketdata/barcharts/";
    inline const QString GET_QUOTES = "https://api.tradestation.com/v3/marketdata/quotes/";
}

namespace AuthConstants {
    constexpr int TOKEN_EXPIRES_IN = 1200;  // 20 minutes
    constexpr int REFRESH_BUFFER_SECONDS = 5;
}
```

**Never** define constants inline in source files unless truly private implementation details.

### SQL Queries

**ALL** SQL queries must be in `Src/SQL/*.h` files:

```cpp
// Src/SQL/OrdersDatabaseQueries.h
namespace OrdersDatabaseQueries {
    inline const QString CREATE_ORDERS_TABLE = R"(
        CREATE TABLE IF NOT EXISTS orders (
            order_id TEXT PRIMARY KEY,
            symbol TEXT NOT NULL,
            action TEXT NOT NULL,
            quantity INTEGER NOT NULL,
            status TEXT NOT NULL
        )
    )";
    
    inline const QString INSERT_ORDER = R"(
        INSERT OR REPLACE INTO orders 
        (order_id, symbol, action, quantity, status)
        VALUES (?, ?, ?, ?, ?)
    )";
}
```

**Usage**:
```cpp
#include "SQL/OrdersDatabaseQueries.h"

QSqlQuery query;
query.prepare(OrdersDatabaseQueries::CREATE_ORDERS_TABLE);
query.exec();
```

## Common Patterns

### Error Handling

```cpp
// Use std::expected for operations that can fail
QFuture<std::expected<QVector<Bar>, TSClient::Error>> getBars();

// Use std::optional for optional values
std::optional<Bar> getBar(int index);

// Early return for error cases
if (!file.open(QIODevice::ReadOnly)) {
    qCritical() << "Failed to open file:" << file.fileName();
    return false;
}
// Happy path continues with minimal indentation
```

### Async Operations

```cpp
// Request tracking
struct RequestInfo {
    QDateTime requestTime;
    std::function<void(QNetworkReply*)> callback;
    int timeoutMs;
};
QMap<QNetworkReply*, RequestInfo> m_pendingRequests;

// Initiate async request
auto future = QtConcurrent::run([this, params]() {
    // Background work
    return result;
});

// Handle completion
auto watcher = new QFutureWatcher<Result>(this);
connect(watcher, &QFutureWatcher<Result>::finished, [this, watcher]() {
    Result result = watcher->result();
    // Process result on calling thread
    watcher->deleteLater();
});
watcher->setFuture(future);
```

### Logging

```cpp
// Define category
Q_LOGGING_CATEGORY(MyClassLog, "MyClass")

// Use in code
qCDebug(MyClassLog) << "Debug message";
qCInfo(MyClassLog) << "Info message";
qCWarning(MyClassLog) << "Warning message";
qCCritical(MyClassLog) << "Critical error";

// Never log sensitive data
qCDebug(TSClientLog) << "Requesting bars for" << symbol;  // ✅ OK
qCDebug(TSClientLog) << "Access token:" << token;         // ❌ NEVER!
```

### Documentation

```cpp
/**
 * @brief Retrieves bars for specified date range
 * @param start Start date (inclusive)
 * @param end End date (inclusive)
 * @return Vector of bars, or empty if none found
 * 
 * This method first checks memory cache, then queries
 * the database if needed. Results are cached for
 * subsequent requests.
 */
QVector<Bar> getBars(const QDate& start, const QDate& end);
```

## Related Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture
- [AUTHENTICATION.md](AUTHENTICATION.md) - OAuth and security
- [FRONTEND.md](FRONTEND.md) - GUI and TUI implementation
- [CONTRIBUTING.md](CONTRIBUTING.md) - Contribution process
