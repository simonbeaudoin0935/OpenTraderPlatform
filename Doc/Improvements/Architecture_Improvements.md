# L2Trader Architecture Improvement Recommendations

**Document Version:** 1.1  
**Date:** 2026-02-07  
**Last Updated:** Fixed critical shutdown segfault  
**Author:** Copilot Architecture Analysis

## Executive Summary

This document provides comprehensive architecture improvement recommendations for the L2Trader project, focusing on threading patterns, singleton usage, memory management, and overall design patterns. The analysis covers 114 source files across the codebase.

---

## Table of Contents

1. [Threading Architecture](#1-threading-architecture)
2. [Singleton Pattern Issues](#2-singleton-pattern-issues)
3. [Memory Management and Ownership](#3-memory-management-and-ownership)
4. [Dependency Injection](#4-dependency-injection)
5. [Error Handling and Robustness](#5-error-handling-and-robustness)
6. [Code Organization and Modularity](#6-code-organization-and-modularity)
7. [Testing and Testability](#7-testing-and-testability)
8. [Implementation Priority Matrix](#8-implementation-priority-matrix)

---

## 1. Threading Architecture

### Current State

The application uses Qt threading with the following pattern:
- **TSClient**: Dedicated worker thread for network operations
- **MainAlgo**: Dedicated worker thread for trading logic
- **MainApp**: Runs on the main thread, orchestrates components
- **QReadWriteLock**: Used in BarCache for thread-safe data access

**Threads Created:**
```cpp
// TSClient (Src/Clients/TSClient/TSClient.cpp:42)
m_thread = new QThread()
this->moveToThread(m_thread)

// MainAlgo (Src/Algo/MainAlgo.cpp:30-32)
QThread thread; // Member variable, not heap allocated
this->moveToThread(&thread)
```

### Issues Identified

#### 1.1 Thread Lifecycle Management

**Status:** ✅ Implemented (PR #XXX)

**Problem:** No explicit shutdown mechanism for worker threads.

**Current Code (FIXED):**
```cpp
// TSClient.cpp - Now properly implements cleanup
TSClient::~TSClient()
{
    qCDebug(TSClientLog) << "TSClient shutting down";
    
    // Request thread to stop
    m_thread.quit();
    
    // Wait for thread to finish (with timeout)
    if (!m_thread.wait(5000)) {
        qCWarning(TSClientLog) << "TSClient thread did not finish within timeout, terminating";
        m_thread.terminate();
        m_thread.wait();
    }
}
```

**Implementation Notes:**
- Changed TSClient from heap-allocated QThread to stack-allocated (consistency with MainAlgo and DatabaseThread)
- Added proper thread lifecycle management with quit/wait/terminate pattern
- Thread lifetime now properly tied to object lifetime

#### 1.1.1 QSocketNotifier Cross-Thread Destruction Fix

**Status:** ✅ Fixed (2026-02-07)

**Problem:** Application segfaulted during graceful shutdown (Ctrl+Q) when QSocketNotifiers were destroyed from a different thread while their event loops were still running.

**Root Cause:** 
```cpp
// MainAlgo.cpp - BROKEN (before fix)
MainAlgo::~MainAlgo()
{
    // ❌ WRONG: Deleting QSocketNotifier while thread is still running
    m_crashNotifier.reset();          // Line 57 - Cross-thread deletion
    StrategySignalHandler::cleanup();
    
    // Thread stop happens AFTER QSocketNotifier deletion
    thread.quit();                     // Line 72
    thread.wait(5000);
}
```

**Error Symptoms:**
- Warning: "QSocketNotifier: Socket notifiers cannot be enabled or disabled from another thread"
- Segfault in `QSocketNotifier::type()` during `QEventDispatcherGlib::processEvents()`
- Crash in MainAlgoThread event loop

**Fix Applied:**
```cpp
// MainAlgo.cpp - FIXED
MainAlgo::~MainAlgo()
{
    DEBUG << "MainAlgo destructor - stopping thread";

    // Stop balance polling timer first
    if (m_balancePollingTimer && m_balancePollingTimer->isActive())
    {
        m_balancePollingTimer->stop();
    }

    // CRITICAL: Stop thread BEFORE destroying thread-owned objects
    thread.quit();
    
    if (!thread.wait(5000))
    {
        CRITICAL << "MainAlgo thread did not finish within timeout, terminating";
        thread.terminate();
        thread.wait();
    }

    // Now safe to cleanup QSocketNotifier (thread is stopped)
    m_crashNotifier.reset();
    StrategySignalHandler::cleanup();
    
    DEBUG << "Destroyed singleton instance";
}
```

**TUIFrontend Fix:**
```cpp
// TUIFrontend.cpp - Added explicit QSocketNotifier cleanup
void TUIFrontend::cleanup()
{
    if (m_initialized)
    {
        // Stop input monitoring before cleanup
        if (m_inputNotifier)
        {
            m_inputNotifier->setEnabled(false);
            delete m_inputNotifier;
            m_inputNotifier = nullptr;
        }

        // Then cleanup ncurses windows...
        endwin();
        m_initialized = false;
    }
}
```

**Threading Rule Established:**
> **Always stop thread event loops BEFORE destroying objects that belong to that thread, especially QSocketNotifiers.**

**Files Modified:**
- `Src/Algo/MainAlgo.cpp` - Reordered destructor operations
- `Src/FrontEnd/TUI/TUIFrontend.cpp` - Added explicit QSocketNotifier cleanup

**Verification:**
- No segfaults on graceful shutdown (Ctrl+Q)
- No "Socket notifiers cannot be enabled" warnings
- Clean singleton destruction order
- All threads terminate properly

#### 1.2 Thread Affinity Checks

**Current:** Some thread assertions exist but not comprehensive.

**Example Found:**
```cpp
// MainAlgo.cpp:80
Q_ASSERT(QThread::currentThread() == &thread);
```

**Recommendation:** Add systematic thread affinity checks for all methods that must run on specific threads:

```cpp
#define REQUIRE_MAIN_THREAD() \
    Q_ASSERT_X(QThread::currentThread() == QApplication::instance()->thread(), \
               Q_FUNC_INFO, "Must be called from main thread")

#define REQUIRE_ALGO_THREAD() \
    Q_ASSERT_X(QThread::currentThread() == &thread, \
               Q_FUNC_INFO, "Must be called from MainAlgo thread")
```

#### 1.3 Lock Granularity in BarCache

**Status:** ✅ REVIEWED (2026-02-08)

**Current:** Single QReadWriteLock protects entire m_barCacheByDay map.

**Code:**
```cpp
// BarCache.h:89-91
mutable QReadWriteLock m_barCacheRwLock; // Protects m_barCacheByDay
mutable QMap<QDate, QVector<Bar>> m_barCacheByDay;
```

**Original Issue:** Lock contention when multiple symbols need different days' data simultaneously.

**Analysis & Resolution:**
The original concern about lock contention between symbols is NOT an issue because:
1. **Per-Symbol Isolation**: Each symbol has its own BarCache instance (via StockInstruments)
2. **Per-Instance Locking**: Each BarCache has its own m_barCacheRwLock
3. **No Cross-Symbol Contention**: Multiple symbols accessing data simultaneously use DIFFERENT locks
4. **Rare Single-Symbol Contention**: Lock contention only occurs when the SAME symbol requests different days simultaneously, which is unlikely

**Current Design Advantages:**
- Simple and maintainable code
- QReadWriteLock allows multiple concurrent readers
- Write operations are fast (in-memory only)
- No lock contention between symbols (separate instances)
- No profiling evidence of lock contention issues

**Alternative Design (Considered but NOT Implemented):**
```cpp
struct DayCacheEntry {
    QVector<Bar> bars;
    mutable QReadWriteLock lock;
};
mutable QMap<QDate, std::shared_ptr<DayCacheEntry>> m_barCacheByDay;
mutable QReadWriteLock m_mapLock; // Only for map modifications
```

**Trade-offs:**
- **Pro:** Better concurrency for single-symbol multi-day requests
- **Con:** More complex code, higher memory overhead, more lock objects
- **Verdict:** Current design is optimal; implement per-day locking only if profiling shows need

**Implementation Notes:**
- Added comprehensive documentation in BarCache.h explaining lock granularity analysis
- Thread safety guarantees documented for read/write operations
- See `Src/Core/Cache/BarCache/BarCache.h` for full documentation

### 1.4 Signal-Slot Thread Safety

**Status:** ✅ IMPLEMENTED (2026-02-08)

**Current:** Heavy reliance on Qt's signal-slot mechanism for cross-thread communication (good!).

**Recommendation:** Document which signals are emitted from which threads:

```cpp
signals:
    // Emitted from: TSClient worker thread
    // Received on: Any thread (Qt::QueuedConnection for cross-thread)
    void authStateChanged(bool isAuthenticated, QString reason);
```

**Implementation:**
Comprehensive thread safety documentation has been added to all major signal declarations throughout the codebase:

1. **TSClient Signals** (`Src/Clients/TSClient/TSClient.h`):
   - `totalDataReceivedBytesIncreased` - Emitted from TSClient worker thread
   - `openStreamCountChanged` - Emitted from TSClient worker thread
   - `authStateChanged` - Emitted from TSClient worker thread
   - All documented with thread context and Qt::QueuedConnection behavior

2. **Stream Signals** (`Src/Clients/TSClient/Stream/Stream.h`):
   - `newAmountOfDataReceived` - Emitted from TSClient worker thread
   - `receivedNewRawData` - Emitted from TSClient worker thread
   - `endSnapshotReceived` - Emitted from TSClient worker thread
   - `streamClosed` - Emitted from TSClient worker thread

3. **MainAlgo Signals** (`Src/Algo/MainAlgo.h`):
   - `displayedStockReceivedNewBar` - Emitted from MainAlgo worker thread
   - `displayedStockReceivedNewMarketDepthQuote` - Emitted from MainAlgo worker thread
   - `receivedNewPosition` - Emitted from MainAlgo worker thread
   - `positionDeleted` - Emitted from MainAlgo worker thread
   - `receivedNewOrder` - Emitted from MainAlgo worker thread
   - `tradeStationAccountsReceived` - Emitted from MainAlgo worker thread
   - `balanceUpdated` - Emitted from MainAlgo worker thread
   - Replay signals (forwarded from ReplayEngine)

4. **Receiver Signals** (MainAlgo thread):
   - **BarReceiver** (`Src/Algo/BarReceiver/BarReceiver.h`): Cross-thread data flow documented
   - **PositionsReceiver** (`Src/Algo/PositionsReceiver/PositionsReceiver.h`): Signal chain documented
   - **OrdersReceiver** (`Src/Algo/OrdersReceiver/OrdersReceiver.h`): Thread context documented
   - **MarketDepthQuoteReceiver** (`Src/Algo/MarketDepthQuoteReceiver/MarketDepthQuoteReceiver.h`): Full data flow documented

5. **FrontEnd Signals** (`Src/FrontEnd/FrontEnd.h`):
   - All signals documented as emitted from main/GUI thread
   - Complete data flow pattern documented (Backend → MainApp → FrontEnd → GUI)
   - Qt main thread requirement for GUI updates noted

**Documentation Pattern:**
Each signal now includes:
- Thread where signal is emitted
- Thread(s) where signal is typically received
- Thread-safety guarantees (Qt::QueuedConnection for cross-thread)
- Data flow explanation for complex chains
- Reference to Architecture_Improvements.md point 1.4

**Thread Safety Summary:**
- **TSClient Thread**: Network operations, stream processing, authentication
- **MainAlgo Thread**: Trading logic, receivers, strategy management
- **Main/GUI Thread**: UI updates, user interaction
- **Qt Automatic**: Cross-thread signals automatically use Qt::QueuedConnection
- **Thread-Safe**: All signal-slot connections are thread-safe by design

---

## 2. Singleton Pattern Issues

### Current Singletons

1. **TSClient** - API client (pointer-based)
2. **MainAlgo** - Trading algorithm manager (pointer-based)
3. **ShortcutSettings** - Keyboard shortcuts (reference-based)
4. **LogBroadcaster** - Log message broadcaster (reference-based)
5. **LoggingConfig** - Logging configuration (reference-based)

### Issues Identified

#### 2.1 Mixed Singleton Patterns

**Problem:** Both pointer-based and reference-based singleton patterns are used inconsistently.

**Current Code Examples:**
```cpp
// Pointer-based (TSClient, MainAlgo)
static TSClient* getInstance() {
    if (m_instance == nullptr) {
        m_instance = new TSClient();
    }
    return m_instance;
}

// Reference-based (LogBroadcaster, ShortcutSettings)
static LogBroadcaster& instance() {
    static LogBroadcaster instance;
    return instance;
}
```

**Recommendation:** Standardize on **reference-based Meyer's singleton** (C++11 thread-safe):

```cpp
class TSClient {
public:
    static TSClient& getInstance() {
        static TSClient instance;
        return instance;
    }
    
    Q_DISABLE_COPY(TSClient)
    
private:
    TSClient() { /* ... */ }
    ~TSClient() { /* cleanup */ }
};
```

**Advantages:**
- Thread-safe initialization (C++11 guarantee)
- Automatic cleanup (destructor called at program exit)
- No manual memory management
- Simpler code

#### 2.2 Incomplete Copy Prevention

**Current:** Manually deleted copy constructors/operators, but not using Qt's convenience macros.

**Example (TSClient.h:66-69):**
```cpp
TSClient(const TSClient&) = delete;
TSClient(TSClient&&) = delete;
TSClient& operator=(const TSClient&) = delete;
TSClient& operator=(TSClient&&) = delete;
```

**Recommendation:** Use Qt's macros for consistency:

```cpp
class TSClient {
    Q_DISABLE_COPY_MOVE(TSClient)
    // This expands to (Qt 6.0+):
    // TSClient(const TSClient &) = delete;
    // TSClient &operator=(const TSClient &) = delete;
    // TSClient(TSClient &&) = delete;
    // TSClient &operator=(TSClient &&) = delete;
```

**Qt provides three macros:**
- `Q_DISABLE_COPY(Class)` - Disables copy operations only
- `Q_DISABLE_MOVE(Class)` - Disables move operations only (Qt 5.13+)
- `Q_DISABLE_COPY_MOVE(Class)` - Disables both copy and move operations (Qt 6.0+)

**For singletons, use `Q_DISABLE_COPY_MOVE`** since they should neither be copied nor moved.

#### 2.3 Unnecessary Singleton Usage

**Problem:** MainAlgo is a singleton but MainApp already has ownership:

```cpp
// MainApp.cpp:24-26
MainApp::MainApp() :
    tradeStationClient(TSClient::getInstance()),
    mainAlgo(MainAlgo::getInstance())
```

**Issue:** 
- MainAlgo is logically owned by MainApp (composition relationship)
- Singleton pattern makes testing difficult
- Creates hidden dependencies throughout codebase

**Recommendation:** Consider removing singleton pattern from MainAlgo:

```cpp
class MainApp {
private:
    TSClient& tradeStationClient;  // Singleton OK (external service)
    std::unique_ptr<MainAlgo> mainAlgo;  // Owned by MainApp
};

MainApp::MainApp() :
    tradeStationClient(TSClient::getInstance()),
    mainAlgo(std::make_unique<MainAlgo>())
{
    // ...
}
```

**Benefits:**
- Clearer ownership
- Easier to test (can create multiple MainAlgo instances in tests)
- No global state

**Trade-off:** Need to pass MainAlgo* to components that need it (see Dependency Injection section).

#### 2.4 Destructor Anti-Pattern

**Current (TSClient.cpp:36-38):**
```cpp
TSClient::~TSClient()
{
    Q_ASSERT(false); // Destructor should never be called for singleton
}
```

**Problem:** Prevents proper cleanup and makes the code fragile.

**Recommendation:** Allow and implement proper cleanup:

```cpp
TSClient::~TSClient()
{
    // Clean up resources
    qDebug() << "TSClient shutting down";
    
    // Stop and wait for thread
    m_thread.quit();
    if (!m_thread.wait(5000)) {
        qWarning() << "TSClient thread did not finish, terminating";
        m_thread.terminate();
        m_thread.wait();
    }
    
    // Clean up network connections
    // ...
}
```

---

## 3. Memory Management and Ownership

### Current Patterns

#### 3.1 Mixed Ownership Strategies

**Observations:**
- Qt parent-child ownership in some places
- Manual `new` without `delete` in others
- Smart pointers (`QPointer`, `std::unique_ptr`) in some places
- Raw pointers in many places

**Examples:**

```cpp
// No parent specified - potential memory leak
m_positionReceiver = new PositionsReceiver(m_activeAccount.getAccountId());

// Parent specified - Qt will clean up
m_balancePollingTimer = new QTimer(this);

// Smart pointer used
std::unique_ptr<QVector<Bar>> bars;

// QPointer for safety
QPointer<StreamBars> m_stream;
```

### Issues Identified

#### 3.2 Missing Parent-Child Relationships

**Problem:** Objects created with `new` but no parent specified.

**Found in MainAlgo.cpp:192, 216:**
```cpp
m_positionReceiver = new PositionsReceiver(m_activeAccount.getAccountId());
m_orderReceiver = new OrdersReceiver(m_activeAccount.getAccountId());
```

**Issue:** These objects are never deleted, causing memory leaks.

**Recommendation:** Set parent for Qt objects:

```cpp
// Option 1: Specify parent in constructor
m_positionReceiver = new PositionsReceiver(m_activeAccount.getAccountId(), this);

// Option 2: Use smart pointers
m_positionReceiver = std::make_unique<PositionsReceiver>(m_activeAccount.getAccountId());
```

#### 3.3 Frontend Ownership Confusion

**Current (MainApp.cpp:29-32):**
```cpp
#ifdef GUI_ENABLED
    appFrontend = new GUIFrontend(mainAlgo);
#else
    appFrontend = new TUIFrontend(mainAlgo);
#endif
```

**Issue:** No parent specified, no explicit deletion in destructor.

**Recommendation:**

```cpp
// MainApp.h - use smart pointer
std::unique_ptr<FrontEnd> appFrontend;

// MainApp.cpp
#ifdef GUI_ENABLED
    appFrontend = std::make_unique<GUIFrontend>(mainAlgo);
#else
    appFrontend = std::make_unique<TUIFrontend>(mainAlgo);
#endif
```

#### 3.4 StockInstruments Ownership

**Current (MainAlgo.cpp:98-101):**
```cpp
currentDisplayedStockInstrument = new StockInstruments(symbol);
Q_CHECK_PTR(currentDisplayedStockInstrument);
stockInstruments.insert(symbol, currentDisplayedStockInstrument);
```

**Issues:**
- Raw pointer stored in QMap
- No explicit deletion
- No parent-child relationship

**Recommendation:** Use smart pointers in container:

```cpp
// MainAlgo.h
QMap<QString, std::unique_ptr<StockInstruments>> stockInstruments;
StockInstruments* currentDisplayedStockInstrument = nullptr;  // Non-owning

// MainAlgo.cpp
auto instrument = std::make_unique<StockInstruments>(symbol);
currentDisplayedStockInstrument = instrument.get();
stockInstruments.insert(symbol, std::move(instrument));
```

#### 3.5 QPointer Usage

**Good Pattern Found:** QPointer used for stream objects:

```cpp
// BarCache.h:87
QPointer<StreamBars> m_stream;

// MarketDepthQuoteReceiver.h:39
QPointer<StreamMarketDepthQuote> m_stream;
```

**Recommendation:** This is good! Continue using QPointer for objects whose lifetime is managed elsewhere.

### Memory Management Guidelines

**Recommendation:** Adopt consistent rules:

1. **For QObject-derived classes:**
   - Prefer specifying parent in constructor
   - Use `std::unique_ptr<T>` when ownership is clear
   - Use `QPointer<T>` for non-owning references that may become invalid

2. **For non-QObject classes:**
   - Always use `std::unique_ptr<T>` or `std::shared_ptr<T>`
   - Avoid raw `new` unless interfacing with C APIs

3. **For collections:**
   ```cpp
   // Owning collection
   QMap<QString, std::unique_ptr<StockInstruments>> m_instruments;
   
   // Non-owning reference
   StockInstruments* m_current = nullptr;
   ```

---

## 4. Dependency Injection

### Current State

**Problem:** Heavy use of singletons creates hidden dependencies:

```cpp
// In any file, anywhere:
TSClient::getInstance()->getAccounts();
MainAlgo::getInstance()->start();
```

**Issues:**
- Hard to test (can't mock singletons easily)
- Hidden dependencies (not clear from function signature)
- Global state makes reasoning about code difficult

### Recommendations

#### 4.1 Explicit Dependency Passing

**Instead of:**
```cpp
class BarCache {
    void fetchData() {
        TSClient::getInstance()->getBars(...);  // Hidden dependency
    }
};
```

**Use Constructor Injection:**
```cpp
class BarCache {
public:
    explicit BarCache(const QString& symbol, TSClient& client)
        : m_symbol(symbol), m_client(client) {}
    
    void fetchData() {
        m_client.getBars(...);  // Explicit dependency
    }
    
private:
    TSClient& m_client;
};
```

**Benefits:**
- Clear dependencies from constructor signature
- Easy to test (can pass mock client)
- No hidden global state

#### 4.2 Service Locator Pattern (Alternative)

If full dependency injection is too invasive:

```cpp
class ServiceLocator {
public:
    static void initialize(TSClient* client, MainAlgo* algo) {
        s_tsClient = client;
        s_mainAlgo = algo;
    }
    
    static TSClient& getTSClient() { 
        Q_ASSERT(s_tsClient != nullptr);
        return *s_tsClient; 
    }
    
private:
    static TSClient* s_tsClient;
    static MainAlgo* s_mainAlgo;
};

// In main():
ServiceLocator::initialize(TSClient::getInstance(), mainAlgo.get());
```

**Benefits:**
- Centralized dependency management
- Can swap implementations for testing
- Still allows singletons but with explicit initialization

---

## 5. Error Handling and Robustness

### Current State

Good patterns observed:
- Q_ASSERT usage for preconditions
- Q_CHECK_PTR for pointer validation
- std::expected for error handling in async operations

### Issues Identified

#### 5.1 Mixed Error Handling Strategies

**Examples:**

```cpp
// Pattern 1: Assertions (development-time checks)
Q_ASSERT(QThread::currentThread() == &thread);

// Pattern 2: std::expected (runtime errors)
QFuture<std::expected<QVector<Account>, TSClient::Error>> getAccounts();

// Pattern 3: Optional
std::optional<std::unique_ptr<QVector<Bar>>> getBarsFromCache(...);

// Pattern 4: Logging and continuing
if (!file.open(mode)) {
    qCDebug(MainAlgoLog) << "Failed to open file";
    Q_ASSERT(0);  // Mix of logging and assertion
}
```

**Recommendation:** Establish clear guidelines:

| Situation | Recommended Approach | Example |
|-----------|---------------------|---------|
| Programming error (bug) | Q_ASSERT or Q_ASSERT_X | `Q_ASSERT(ptr != nullptr)` |
| Expected runtime error | std::expected | `std::expected<Data, Error>` |
| Optional data | std::optional | `std::optional<Data>` |
| Async operations | QFuture + std::expected | `QFuture<std::expected<T, E>>` |

#### 5.2 Error Recovery Strategy

**Problem:** Some errors cause application to abort, others are silently ignored.

**Example (MainAlgo.cpp:46-49):**
```cpp
if (!file.open(mode)) {
    qCDebug(MainAlgoLog) << "Failed to open file:" << filePath;
    Q_ASSERT(0);  // Crashes in debug, ignored in release
}
```

**Recommendation:** Define error severity levels:

```cpp
enum class ErrorSeverity {
    Recoverable,    // Log and continue
    Degraded,       // Log, disable feature, continue
    Fatal           // Log and exit gracefully
};

void handleError(ErrorSeverity severity, const QString& message) {
    switch (severity) {
        case ErrorSeverity::Recoverable:
            qWarning() << message;
            break;
        case ErrorSeverity::Degraded:
            qCritical() << message;
            // Disable affected feature
            break;
        case ErrorSeverity::Fatal:
            qFatal("%s", qPrintable(message));
            break;
    }
}
```

#### 5.3 Network Error Handling

**Current:** Good use of std::expected for async operations.

**Suggestion:** Add retry logic with exponential backoff:

```cpp
class RetryPolicy {
public:
    static constexpr int MAX_RETRIES = 3;
    static constexpr int BASE_DELAY_MS = 1000;
    
    static int getDelay(int attemptNumber) {
        return BASE_DELAY_MS * (1 << attemptNumber);  // Exponential backoff
    }
};
```

---

## 6. Code Organization and Modularity

### Current Structure

```
Src/
├── Algo/              # Trading logic
├── Clients/           # API clients
├── Core/              # Application core
├── FrontEnd/          # UI (GUI/TUI)
├── Misc/              # Utilities
└── Recorder/          # Companion app
```

### Recommendations

#### 6.1 Interface Segregation

**Problem:** TSClient is a large class with many responsibilities (auth, market data, brokerage, orders, streams).

**Current (TSClient.h):** 229 lines, 15+ public methods.

**Recommendation:** Split into focused interfaces:

```cpp
// Interface for market data operations
class IMarketDataClient {
public:
    virtual ~IMarketDataClient() = default;
    virtual QFuture<std::expected<QVector<Quote>, Error>> 
        getQuoteSnapshots(const QStringList& symbols) = 0;
    virtual QFuture<std::expected<std::unique_ptr<QVector<Bar>>, Error>> 
        getBars(...) = 0;
    // ...
};

// Interface for brokerage operations
class IBrokerageClient {
public:
    virtual ~IBrokerageClient() = default;
    virtual QFuture<std::expected<QVector<Account>, Error>> 
        getAccounts() = 0;
    virtual QFuture<std::expected<QVector<Balance>, Error>> 
        getBalances(const QStringList& accounts) = 0;
    // ...
};

// TSClient implements all interfaces
class TSClient : public IMarketDataClient, 
                 public IBrokerageClient,
                 public IOrderExecutionClient {
    // Implementation
};
```

**Benefits:**
- Clearer responsibilities
- Easier to mock for testing
- Clients can depend on specific interfaces only

#### 6.2 Separate Concerns in MainAlgo

**Current:** MainAlgo handles:
- Algorithm logic
- Stream management (positions, orders)
- Account management
- Balance polling
- Stock instrument lifecycle

**Recommendation:** Extract responsibilities:

```cpp
class AccountManager {
    // Manages accounts, balance polling
};

class StreamManager {
    // Manages position and order streams
};

class InstrumentRegistry {
    // Manages StockInstruments lifecycle
};

class MainAlgo {
    // Coordinates the above, implements trading logic
private:
    AccountManager m_accountManager;
    StreamManager m_streamManager;
    InstrumentRegistry m_instrumentRegistry;
};
```

---

## 7. Testing and Testability

### Current State

Tests exist in `Tests/` directory:
- BarCacheUnit
- TSClientUnit
- RecorderIntegrationTest

### Issues Identified

#### 7.1 Hard to Test Due to Singletons

**Problem:** Can't easily create isolated test instances:

```cpp
TEST_F(MainAlgoTest, TestSomething) {
    // Can't create fresh MainAlgo instance
    MainAlgo* algo = MainAlgo::getInstance();
    // This gets the global singleton!
}
```

**Recommendation:** Make classes testable:

```cpp
class MainAlgo {
public:
    // Production code uses singleton
    static MainAlgo& getInstance() {
        static MainAlgo instance;
        return instance;
    }
    
protected:
    // Tests can inherit and create instances
    MainAlgo() { /* ... */ }
    virtual ~MainAlgo() = default;
    
    Q_DISABLE_COPY(MainAlgo)
};

// In test:
class TestableMainAlgo : public MainAlgo {
public:
    TestableMainAlgo() : MainAlgo() {}
    // Can override virtual methods for testing
};
```

#### 7.2 Dependency Injection for Testing

**Current:** Hard to inject mock TSClient.

**Recommendation:** Use interfaces:

```cpp
class MainAlgo {
public:
    explicit MainAlgo(IMarketDataClient& marketData,
                      IBrokerageClient& brokerage)
        : m_marketData(marketData), m_brokerage(brokerage) {}
        
private:
    IMarketDataClient& m_marketData;
    IBrokerageClient& m_brokerage;
};

// Production:
MainAlgo algo(TSClient::getInstance(), TSClient::getInstance());

// Test:
MockMarketDataClient mockMarket;
MockBrokerageClient mockBrokerage;
MainAlgo algo(mockMarket, mockBrokerage);
```

#### 7.3 Thread-Safe Testing

**Recommendation:** Provide test mode that disables threading:

```cpp
class TSClient {
public:
    void start() {
        if (!m_testMode) {
            m_thread.start();
        }
    }
    
    void setTestMode(bool enabled) { m_testMode = enabled; }
    
private:
    bool m_testMode = false;
};
```

---

## 8. Implementation Priority Matrix

### High Priority (Do First)

| Issue | Impact | Effort | Rationale |
|-------|--------|--------|-----------|
| Add Q_DISABLE_COPY to singletons | Low | Low | Quick win, improves code clarity |
| Fix memory leaks (parent-child) | High | Medium | Prevents resource leaks |
| Standardize singleton pattern | Medium | Medium | Improves maintainability |
| Add thread lifecycle management | High | Medium | Prevents crashes on shutdown |

### Medium Priority (Do Second)

| Issue | Impact | Effort | Rationale |
|-------|--------|--------|-----------|
| Extract MainAlgo responsibilities | High | High | Improves testability and clarity |
| Add dependency injection | High | High | Enables testing, reduces coupling |
| Interface segregation for TSClient | Medium | High | Better modularity |
| Comprehensive error handling | High | Medium | Improves robustness |

### Low Priority (Nice to Have)

| Issue | Impact | Effort | Rationale |
|-------|--------|--------|-----------|
| Per-day locking in BarCache | Low | Medium | Only if profiling shows need |
| Service locator pattern | Medium | Medium | Alternative to full DI |
| Extract AccountManager | Medium | Medium | Can be done incrementally |

---

## Conclusion

The L2Trader codebase demonstrates good practices in several areas:
- Qt signal-slot for cross-thread communication
- QReadWriteLock for thread-safe data access
- std::expected for error handling
- Q_ASSERT for precondition checking

However, there are opportunities for improvement:
1. **Threading:** Inconsistent patterns, missing cleanup
2. **Singletons:** Overuse, testing difficulties
3. **Memory:** Manual management, unclear ownership
4. **Dependencies:** Hidden, hard to test

### Recommended Approach

**Phase 1 (Quick Wins):**
1. Add Q_DISABLE_COPY to all singletons
2. Fix parent-child relationships for Qt objects
3. Standardize on Meyer's singleton pattern

**Phase 2 (Foundation):**
1. Add proper thread lifecycle management
2. Convert MainAlgo from singleton to owned object
3. Use smart pointers consistently

**Phase 3 (Long-term):**
1. Implement dependency injection
2. Split large classes using interface segregation
3. Add comprehensive testing infrastructure

### Testing Strategy

Before and after each change:
1. Run existing unit tests
2. Test thread shutdown behavior
3. Check for memory leaks with valgrind
4. Verify no performance regression

---

## References

- Qt Documentation: https://doc.qt.io/qt-6/
- C++ Core Guidelines: https://isocpp.github.io/CppCoreGuidelines/
- Effective Modern C++ by Scott Meyers
- Qt Threading Best Practices: https://doc.qt.io/qt-6/threads-technologies.html
