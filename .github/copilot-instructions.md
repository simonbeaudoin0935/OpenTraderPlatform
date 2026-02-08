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

## Graceful Teardown & Destructor Guidelines

### Critical Threading Rules (MUST FOLLOW)

**Rule 1: Stop threads BEFORE destroying thread-owned objects**
```cpp
// ✓ CORRECT - Stop thread first
~MyClass() {
    // 1. Stop thread
    m_thread.quit();
    m_thread.wait(5000);
    
    // 2. THEN destroy thread-owned objects
    m_socketNotifier.reset();
    m_timer.reset();
}

// ✗ WRONG - Destroys objects while thread running
~MyClass() {
    m_socketNotifier.reset();  // ❌ Thread still processing events!
    m_thread.quit();
}
```

**Rule 2: Delete child QObjects on their thread before stopping**
```cpp
// ✓ CORRECT - Delete children on their thread
~MyWorkerClass() {
    // Schedule deletion on correct thread
    QMetaObject::invokeMethod(this, [this]() {
        for (QObject* child : children()) {
            child->deleteLater();
        }
    }, Qt::BlockingQueuedConnection);
    
    // Process deleteLater queue
    QMetaObject::invokeMethod(this, []() {
        QCoreApplication::processEvents();
    }, Qt::BlockingQueuedConnection);
    
    // Now safe to stop thread
    m_thread.quit();
    m_thread.wait(5000);
}
```

**Rule 3: Use deleteLater() for QObjects with signals/events**
```cpp
// ✓ CORRECT - Use deleteLater() for objects with pending events
void cleanup() {
    m_stream->deleteLater();        // Has timers, network replies, signals
    m_networkReply->deleteLater();  // Qt requirement
}

// ✗ WRONG - Direct delete can crash
void cleanup() {
    delete m_stream;        // ❌ Signals may fire during destruction
    delete m_networkReply;  // ❌ Violates Qt guidelines
}
```

**Rule 4: Add thread affinity assertions in destructors**
```cpp
~MyThreadedClass() {
    // Assert destructor called from expected thread
    OBJ_ASSUME_EQUAL(QThread::currentThread(), QCoreApplication::instance()->thread());
    
    // Or for objects that should be on their own thread:
    OBJ_ASSUME_EQUAL(QThread::currentThread(), this->thread());
    
    // Rest of cleanup...
}
```

### When to Use delete vs deleteLater()

**Use `deleteLater()` when QObject has:**
- ✅ Signal connections (pending signals may fire)
- ✅ Pending events in event queue
- ✅ Child QObjects that emit signals
- ✅ QTimer or QSocketNotifier members
- ✅ QNetworkReply (Qt requirement)
- ✅ Called from signal/slot handlers
- ✅ Complex object graph with cross-dependencies

**Use direct `delete` only when:**
- ✅ Thread stopped and event loop exited
- ✅ No signal connections exist
- ✅ No pending events possible
- ✅ Not a QNetworkReply
- ✅ Simple utility objects with no Qt features

**Example Patterns:**
```cpp
// QNetworkReply - ALWAYS use deleteLater()
void onRequestFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    reply->deleteLater();  // ✓ CORRECT
}

// Stream objects - Use deleteLater() (have timers/signals)
void closeStream() {
    m_stream->deleteLater();  // ✓ CORRECT
}

// After stopping thread - Direct delete is safe
~TSClient() {
    m_thread.quit();
    m_thread.wait();
    // Thread stopped, direct delete safe for simple objects
    delete m_simpleResource;  // ✓ CORRECT (if no Qt features)
}
```

### Destructor Best Practices

**1. Document cleanup order:**
```cpp
~MyClass() {
    DEBUG << "MyClass destructor - cleaning up";
    
    // 1. Stop timers/notifiers
    m_timer->stop();
    
    // 2. Stop threads
    m_thread.quit();
    m_thread.wait(5000);
    
    // 3. Delete thread-owned objects
    m_threadOwnedObject.reset();
    
    // 4. Cleanup in reverse order of initialization
    m_networkResources.clear();
    
    DEBUG << "MyClass destroyed successfully";
}
```

**2. Always log destruction:**
```cpp
~MyClass() {
    DEBUG << "MyClass destructor - stopping thread";
    // cleanup...
    DEBUG << "Destroyed singleton instance";
}
```

**3. Handle thread termination:**
```cpp
m_thread.quit();
if (!m_thread.wait(5000)) {
    CRITICAL << "Thread did not finish within timeout, terminating";
    m_thread.terminate();
    m_thread.wait();
}
```

### Singleton Destruction Order

In `MainApp::cleanupSingletons()`, destroy in dependency order:
```cpp
void MainApp::cleanupSingletons() {
    Stream::setShuttingDown(true);    // Prevent promise resolution
    
    MainApp::destroyInstance();       // Frontend first
    MainAlgo::destroyInstance();      // Algo before client
    TSClient::destroyInstance();      // Client before database
    DatabaseThread::destroyInstance(); // Database last
}
```

### Common Pitfalls to Avoid

❌ **Deleting QSocketNotifier while thread running**
- Symptom: "Socket notifiers cannot be enabled from another thread"
- Fix: Stop thread first, then delete

❌ **Direct delete on Stream objects**
- Symptom: Segfault in QNetworkAccessManager::finished()
- Fix: Use deleteLater() to handle pending signals

❌ **Cross-thread QObject deletion**
- Symptom: Assertion failure "QThread::currentThread() != this->thread()"
- Fix: Use BlockingQueuedConnection to delete on correct thread

❌ **Forgetting to process deleteLater() before thread stop**
- Symptom: Objects not deleted, memory leaks
- Fix: Call QCoreApplication::processEvents() after deleteLater()

### Lessons Learned from Real Bugs

**Bug #1: QSocketNotifier cross-thread deletion (2026-02-07)**
- Root cause: `m_crashNotifier.reset()` called before `thread.quit()`
- Impact: Segfault during event processing
- Fix: Reorder to stop thread first

**Bug #2: Stream child deletion from wrong thread (2026-02-07)**
- Root cause: Qt parent-child auto-deletion from main thread
- Impact: Thread affinity assertion failure
- Fix: Use BlockingQueuedConnection to delete on correct thread

**Bug #3: QNetworkAccessManager signals during deletion (2026-02-07)**
- Root cause: Direct `delete` on Stream objects with active network
- Impact: Segfault in finished() signal during destruction
- Fix: Use deleteLater() + processEvents()

### References

See `Doc/Improvements/Architecture_Improvements.md` section 1.1.1 for detailed analysis of these threading issues and fixes.

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
