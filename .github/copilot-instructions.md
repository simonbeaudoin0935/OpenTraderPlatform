# Copilot Instructions for L2Trader Repository

## Building the Project

1. **Clean build directory** (recommended for clean builds):
   ```bash
   rm -rf build/
   ```

2. **Create build directory**:
   ```bash
   mkdir build
   cd build
   ```

3. **Configure GUI version with CMake** (using Ninja):

   ```bash
   mkdir -p ./build/GUI
   cmake -S .. -B ./build/GUI -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=ON -DBUILD_TESTS=OFF
   ```
   - Takes ~10-15 seconds

3. **Configure TUI version with CMake** (using Ninja):

   ```bash
   mkdir -p ./build/TUI
   cmake -S .. -B ./build/TUI -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=OFF -DBUILD_TESTS=OFF
   ```
   - Takes ~10-15 seconds


4. **formatting**:
   ```bash
   find Src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
   ```

   When committing code, make sure to run the above command to format the code according to the project's coding style.

5. **Build GUI version**:
   ```bash
   cmake --build ./build/GUI -j$(nproc)
   ```
   - Takes ~2-5 minutes (faster with Ninja)

6. **Build TUI version**:
   ```bash
   cmake --build ./build/TUI -j$(nproc)
   ```
   - Takes ~2-5 minutes (faster with Ninja)

## Run Application
1. **TUI Mode** (default):
   ```bash
   ./build/TUI/Src/L2Trader
   ```

   The stdout is the ncurses TUI interface. The stderr is the logging output. Therefore, you can 
   either redirect stderr to a file or a separate terminal to see the logs, or dump stderr to /dev/null if you don't care about logs in live and can check log file later in ~/.local/share/L2Trader/logs/

## General Coding Guidelines
- Use the ASSUME macros from Src/Misc/Assume.h for assertions instead of Q_ASSERT or similar, to ensure consistency and proper no-op behavior in release builds.

- **ASSERT-First Strategy**: Instead of defensive null checks like `if (ptr != nullptr) { ... }`, assert the expected state upfront with `OBJ_ASSUME_DIFF(ptr, nullptr)` and proceed without conditionals. This:
  1. Makes assumptions explicit and documents invariants
  2. Catches logic errors early in debug builds
  3. Removes defensive code that hides bugs
  4. Example - instead of:
     ```cpp
     if (m_replayEngine != nullptr) {
         m_replayEngine->pauseReplay();
     }
     ```
     Write:
     ```cpp
     OBJ_ASSUME_DIFF(m_replayEngine, nullptr);
     m_replayEngine->pauseReplay();
     ```
  5. Only use conditional checks when the null/empty state is a **valid runtime possibility**, not a logic error.

- When creating connections between signal and slots, prioritize using a Qt::UniqueConnection and asserting that the connection made was indeed unique and not a double. It should be extremely rare, if not never, that we should authorize multiple same connections.

- When creating new functions with return values, if it makes no sense to ignore the return value, add the [[nodiscard]] guards to make sure we get notified if we don't use the return value of a function. This should be the default for every new function that returns a value in fact, and you should only take it out when truly its not a big deal to not check the value, but this should in practice be rare.

- When using the 'new' operator or any function that returns a dynamically allocated object, you should always check the pointer with Q_CHECK_PTR() and make sure we continue with a valid pointer. An example of such function which returns an allocated object is QNetworkManager->get(networkRequest) were the doc says "Posts a request to obtain the contents of the target request and returns a new QNetworkReply object opened for reading which emits the readyRead() signal whenever new data arrives."

- with memebr variables of classes, use a prefix "m_". for global variables, use "g_", for parameters, use "p_"

- Use a early exit style of coding. Handle error/non-happy cases first with return, continue, break, or throw. Then write the main ("happy") logic with minimal indentation

- Any .md file you create, place them in the Doc folder.

- When you do structural changes, think about keeping the doc in Doc/ up to date.

- Everything time related must be in QDateTime/QTime/QDate with proper QTimeZone usage. Never use std::chrono or raw time_t/struct tm etc. The timezone is always NewYork since it is stock market related.

## Constants Management
- **ALL** application constants must be defined in `Src/Misc/CONSTANTS.h` organized into appropriate namespaces.
- **NEVER** define constants inline in source files or individual header files unless they are truly private implementation details.
- When adding a new constant, determine the appropriate namespace in CONSTANTS.h:
  - `TradingHours` - Trading hours, timing, and timezone constants
  - `TSClientEndpoints` - TradeStation API endpoint URLs
  - `AuthConstants` - Authentication and token-related constants
  - `ChartConstants` - Chart display and rendering constants
  - `BarFlags` - Bit flags for Bar status encoding
  - `FileSystemConstants` - File paths and directory names
- If a suitable namespace doesn't exist, create a new one with appropriate documentation.
- Use constants from CONSTANTS.h by including the header and referencing them via namespace (e.g., `TradingHours::TRADING_START_TIME`, `TSClientEndpoints::GET_BARS`).
- This centralization makes constants easy to find, maintain, and update without searching through the entire codebase.

## Threading Patterns
- **ALWAYS** use stack-allocated QThread member variables, never heap-allocated pointers.
- **Pattern**: `QThread m_thread;` (member variable), NOT `QThread* m_thread;` (pointer with new)
- **Rationale**: Stack allocation is simpler, avoids memory leaks, and thread lifetime is automatically tied to object lifetime.
- **Thread Lifecycle**: Implement proper cleanup in destructors with quit/wait/terminate pattern:
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
- **Consistency**: This pattern is used throughout the codebase (TSClient, MainAlgo, DatabaseThread).
- See Doc/Architecture_Improvements.md section 1.1 for historical context.

## Composition Over Pointers
- **PREFER** composition (direct member objects) over pointers when designing classes.
- If an object can be owned directly by a class and doesn't need polymorphism or optional lifetime, make it a direct member rather than a pointer.
- Example of good composition: `class MyClass { BarCache m_cache; RunUpDetector m_detector; };` instead of `class MyClass { BarCache* m_cache; RunUpDetector* m_detector; };`
- Use pointers (raw or smart) only when necessary:
  - Polymorphism is required (base class pointers to derived objects)
  - Object lifetime needs to extend beyond the containing class
  - Object is optional (may or may not exist)
  - Object is very large and copying would be expensive
  - Forward declaration is needed to break circular dependencies
  - Qt parent-child ownership is being used
- Composition provides better encapsulation, eliminates null checks, and makes ownership clear at compile-time.

## Smart Pointer Usage
This project uses smart pointers throughout to ensure proper memory management and prevent memory leaks. Follow these guidelines:

### Qt Objects with Parent-Child Ownership
- **DO NOT** use smart pointers for Qt objects (QWidget, QTimer, QLayout, etc.) that have a parent specified in their constructor.
- Qt's parent-child ownership system automatically manages memory: when a parent is deleted, it deletes all its children.
- Example: `new QTimer(this)` - the `this` pointer makes the timer a child, so no smart pointer needed.
- Example: `new QVBoxLayout(parentWidget)` - the layout is owned by the parent widget.

### QPointer for Qt Objects That May Be Deleted
- **USE** `QPointer<T>` for Qt objects that may be deleted independently or whose lifetime you don't control.
- `QPointer` automatically becomes null when the pointed-to object is deleted.
- Primary use case: Holding references to stream objects (StreamBars, StreamOrders, StreamPositions, StreamMarketDepthQuote).
- Example: `QPointer<StreamBars> m_stream;` - the stream may close/delete itself on error.

### std::unique_ptr for Exclusive Ownership
- **USE** `std::unique_ptr<T>` for objects with exclusive ownership that are NOT Qt objects with parents.
- Use for non-Qt objects, or Qt objects without parents that need explicit lifetime management.
- Prefer `std::make_unique<T>()` for construction.
- Use `std::move()` when transferring ownership.
- Example: `std::unique_ptr<QVector<Bar>>` for owned data containers.
- Example: Database objects without Qt parent-child relationships.

### std::shared_ptr for Shared Ownership
- **USE** `std::shared_ptr<T>` when multiple owners need to share ownership of an object.
- Use for data that needs to be shared across asynchronous operations or multiple components.
- Prefer `std::make_shared<T>()` for construction.
- Primary use case: Bar data vectors shared between cache, API calls, and UI components.
- Example: `std::shared_ptr<QVector<Bar>>` passed to callbacks and UI.

### std::weak_ptr for Non-Owning References
- **USE** `std::weak_ptr<T>` when you need to observe a `std::shared_ptr` without extending its lifetime.
- Prevents circular reference issues with `std::shared_ptr`.
- Always check if the weak_ptr is still valid with `lock()` before using.

### When NOT to Use Smart Pointers
- Qt objects with parents (use raw pointers, Qt manages them).
- Function parameters (use raw pointers or references for non-owning access).
- Stack-allocated objects (no pointers needed).
- QStandardItem and similar Qt model items added to models (model takes ownership).

### General Rules
- **NEVER** use raw `new` without a corresponding smart pointer or Qt parent.
- **NEVER** manually `delete` objects managed by smart pointers or Qt parent-child relationships.
- When in doubt, prefer `std::unique_ptr` for non-Qt objects.
- Document the ownership semantics clearly when the pattern is not obvious.

## SQL Queries
- **ALL** SQL queries must be defined as constants in dedicated header files in the `Src/SQL/` folder. Never write SQL queries directly in implementation files.
- Each class that uses SQL queries should have its own header file in `Src/SQL/` (e.g., `OrdersDatabaseQueries.h`, `LiveStreamDBQueries.h`).
- When you need to add a new SQL query, add it to the appropriate header file in `Src/SQL/` with a descriptive name and appropriate comments indicating its purpose.
- Each query file should have its own namespace matching the class name (e.g., `OrdersDatabaseQueries`, `LiveStreamDBQueries`).
- Use the queries by including `SQL/<ClassName>Queries.h` and referencing them via the namespace (e.g., `OrdersDatabaseQueries::CREATE_ORDERS_TABLE`).
- This organization makes SQL queries easy to find, maintain, and update without hunting through the entire codebase.

## High Level Details

**Repository Summary**: L2Trader is a real-time algorithmic trading application built with Qt6 that monitors stock market data, executes trading strategies, and provides comprehensive market analysis tools. It connects to TradeStation APIs for live market data, Level 2 market depth visualization, and position tracking.

**Repository Information**:
- **Outputs**: Outputs the L2Trader main application
- **Size**: ~50+ source files, ~10k+ lines of code
- **Type**: C++ desktop application with GUI and TUI modes (Although, the TUI boiletplace is in place, but not implemented yet. This will be in the last things I do, don't bother touching it)
- **Languages**: C++23, QML (minimal), CMake, Shell scripts
- **Frameworks**: Qt6 (Core, Network, SQL, Widgets)
- **Target Runtimes**: Linux (Ubuntu 24.04), cross-platform (X86_64, ARM64)
- **Build System**: CMake 3.16+
- **Testing**: Qt Test framework with unit tests
- **CI/CD**: GitHub Actions with matrix builds for multiple architectures

## Build Instructions

### Prerequisites
- **Qt6**: Version 6.4.2+ (CI uses 6.4.2 because it is what Ubuntu 24.04 uses)
- **CMake**: 3.16+
- **Compiler**: GCC 7+ or Clang 5+ with C++23 support
- **SQLite**: For database functionality
- **Git**: For version control

### Build Speed Optimization
The building in the github workflow uses 'ccache' to speed up the building process. The ~/.cache/ccache folder from previous runs is caches with the github's cache action and retreived in subsequent builds.

### Build Process
Always run commands in the repository root directory.


#### Regular Local prompts in VSCode
When executing a local prompt in VSCode, instead use the tasks defined in .vscode/tasks.json to build the project.
Use :
- build : This builds everything




### Lint and Validation
No dedicated linting tools configured. Code follows Qt coding conventions.

### Common Issues and Workarounds
- **Missing dependencies**: Ensure Qt6 development packages are installed
- **Timing**: Full build takes 2-5 minutes, tests take 30-60 seconds

### Validation Steps
- **Build success**: Check for zero exit code from cmake --build
- **Test success**: All tests pass
- **Run success**: Application starts without crashes
- **CI validation**: GitHub Actions workflows run on push/PR

## Project Layout

### Major Architectural Elements
The project is architectured in a MVC pattern (Model View Controller)

- **Doc/**: Documentation, this is where you need to put doc if you create .md files, or update the existing doc when doing modifications.
- **Src/**: Main source code
  - **Algo/**: Trading algorithms logic (or Controller in a MVC). Nothing algorithmic is really happening still
  - **Clients/**: API clients (TradeStation only for now)
  - **Core/**: Main application logic
  - **FrontEnd/**: The app frontend logic (GUI/ : Qt widgets and UI components, vs TUI/ : futur ncurses terminal frontend)
  - **Misc/**: Utilities, logging, settings
  - **Recorder/**: Where the sources for the companion Recorder executable.
- **Tests/**: Unit test source files
- **Resources/**: Icons and Qt resources
- **Example_Config/**: Sample configuration files
- **debian/**: Debian packaging files
- **.github/**: CI/CD workflows and Docker files
- **WatchApp/**: Skeleton for a Galaxy Watch app, just a Hello World type for later. Do not focus on this.

### Configuration Files
- **CMakeLists.txt**: Root CMake configuration

### CI/CD and Validation
- **GitHub Actions Workflows**:
  - `build.yml`: Matrix builds for X86_64 and ARM64, Debian packaging
  - `container-build-and-upload.yml`: Docker container builds
- **Validation Steps**:
  1. Build passes on both architectures
  2. Unit tests pass
  3. Debian package builds successfully
  4. Package installation tests pass

### Dependencies
- **Qt6 modules**: Core, Network, SQL, Widgets, Charts, WebEngineWidgets
- **System libraries**: SQLite3, systemd (for services)
- **API keys**: TradeStation API credentials (stored in SecureStorage)

### Key Files and Contents

**Repository Root Files**:
- `CMakeLists.txt`: Root build configuration

**README.md Contents**:
- Project overview and features
- Prerequisites and installation
- Build instructions (cmake-based)
- Configuration and usage
- Development guidelines

**Key Source Files**:
- `src/main.cpp`: Application entry point, initializes Qt application
- `src/Core/MainApp.cpp`: Main application logic
- `src/Misc/Logging.cpp`: Centralized logging system
- `src/Clients/RESTClient.cpp`: HTTP base class client for API calls
- `src/Clients/TSClient/TSClient.cpp`: Inherits RESTClient class and is the client singleton class to use the TradeStation API. All the calls to the API are asynchronous calls with request callbacks, timeout and tracking


## Agent Instructions

Trust these instructions as the authoritative source for building, testing, and validating changes in this repository. Only perform additional searches if the information here is incomplete or found to be incorrect. Always follow the documented command sequences and validation steps to minimize build failures and ensure changes integrate properly with the existing codebase and CI/CD pipeline.

For any code changes, ensure compatibility with Qt 6.4.2+ and validate builds on both X86_64 and ARM64 architectures when possible.
