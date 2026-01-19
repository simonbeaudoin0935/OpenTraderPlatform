# Copilot Instructions for L2Trader Repository

- DO NOT ADD _codeql_build_dir to .gitignore, do not commit that ever

## Building the Project

The project has a `copilot-setup-steps.yml` workflow that installs all necessary dependencies. After the setup steps complete, you can build the project following the instructions in the "Build Instructions" section below.

## General Coding Guidelines
- When generating code, use the ASSUME macros from Src/Misc/Assume.h for assertions instead of Q_ASSERT or similar, to ensure consistency and proper no-op behavior in release builds.
- When creating connections between signal and slots, prioritize using a Qt::UniqueConnection and asserting that the connection made was indeed unique and not a double. It should be extremely rare, if not never, that we should authorize multiple same connections.
- When creating new functions with return values, if it makes no sense to ignore the return value, add the [[nodiscard]] guards to make sure we get notified if we don't use the return value of a function. This should be the default for every new function that returns a value in fact, and you should only take it out when truly its not a big deal to not check the value, but this should in practice be rare.
- When using the 'new' operator or any function that returns a dynamically allocated object, you should always check the pointer with Q_CHECK_PTR() and make sure we continue with a valid pointer. An example of such function which returns an allocated object is QNetworkManager->get(networkRequest) were the doc says "Posts a request to obtain the contents of the target request and returns a new QNetworkReply object opened for reading which emits the readyRead() signal whenever new data arrives."
- with memebr variables of classes, use a prefix "m_". for global variables, use "g_", for parameters, use "p_"
- Use a early exit style of coding. Handle error/non-happy cases first with return, continue, break, or throw. Then write the main ("happy") logic with minimal indentation
- Any .md file you create, place them in the Doc folder.
- When you do structural changes, think about keeping the doc in Doc/ up to date.
  
- Everything time related must be in QDateTime/QTime/QDate with proper QTimeZone usage. Never use std::chrono or raw time_t/struct tm etc. The timezone is always NewYork since it is stock market related.

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

### Bootstrap
No bootstrap required. The repository is ready to build after cloning.

### Build Speed Optimization
The building in the github workflow uses 'ccache' to speed up the building process. The ~/.cache/ccache folder from previous runs is caches with the github's cache action and retreived in subsequent builds.

### Build Process
Always run commands in the repository root directory.

#### Github Premium Requests
When executing a Premium Request after being assigned an issue on GitHub and you want to build, use these commands : 
1. **Clean build directory** (recommended for clean builds):
   ```bash
   rm -rf build/
   ```

2. **Create build directory**:
   ```bash
   mkdir build
   cd build
   ```

3. **Configure with CMake**:
   ```bash
   cmake -S .. -B . -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=ON -DBUILD_TESTS=OFF
   ```
   - Dont build the tests
   - This generates Makefiles in the current directory
   - Takes ~10-15 seconds
   - Required flags: `-DENABLE_GUI=ON` for GUI build, `-DBUILD_TESTS=ON` for tests

4. **Build all targets**:
   ```bash
   cmake --build . -j2
   ```
   - Uses 2 cores

#### Regular Local prompts in VSCode
When executing a local prompt in VSCode, instead use the tasks defined in .vscode/tasks.json to build the project.
Use :
- build : This builds everything


### Run Application
1. **GUI Mode** (default):
   ```bash
   ./src/L2Trader --criterias=../Example_Config/selection_criteria.ini
   ```

2. **Recorder Mode**:
   ```bash
   ./src/Recorder --criterias=../Example_Config/selection_criteria.ini --cache-root-dir=/tmp/cache
   ```

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
