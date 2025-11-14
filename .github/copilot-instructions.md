# Copilot Instructions for L2Trader Repository

## High Level Details

**Repository Summary**: L2Trader is a real-time algorithmic trading application built with Qt6 that monitors stock market data, executes trading strategies, and provides comprehensive market analysis tools. It connects to TradeStation and Financial Modeling Prep (FMP) APIs for live market data, implements automated stock screening, breaking news monitoring, Level 2 market depth visualization, and position tracking.

**Repository Information**:
- **Size**: ~50+ source files, ~10k+ lines of code
- **Type**: C++ desktop application with GUI and CLI modes
- **Languages**: C++17, QML (minimal), CMake, Shell scripts
- **Frameworks**: Qt6 (Core, Network, SQL, Widgets, Charts, WebEngineWidgets)
- **Target Runtimes**: Linux (Ubuntu 24.04), cross-platform (X86_64, ARM64)
- **Build System**: CMake 3.16+
- **Testing**: Qt Test framework with unit tests
- **CI/CD**: GitHub Actions with matrix builds for multiple architectures

## Build Instructions

### Prerequisites
- **Qt6**: Version 6.4.2+ (CI uses 6.4.2, local development uses 6.8.3)
- **CMake**: 3.16+
- **Compiler**: GCC 7+ or Clang 5+ with C++17 support
- **SQLite**: For database functionality
- **Git**: For version control

### Bootstrap
No bootstrap required. The repository is ready to build after cloning.

### Build Process
Always run commands in the repository root directory.

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
   cmake -S .. -B . -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=ON -DBUILD_TESTS=ON
   ```
   - This generates Makefiles in the current directory
   - Takes ~10-15 seconds
   - Required flags: `-DENABLE_GUI=ON` for GUI build, `-DBUILD_TESTS=ON` for tests

4. **Build all targets**:
   ```bash
   cmake --build . --parallel
   ```
   - Uses all available CPU cores
   - Takes ~2-5 minutes depending on hardware
   - Builds main application, tests, and recorder

### Test Execution
Tests must be built first (include `-DBUILD_TESTS=ON` in cmake configure).

1. **Run all tests**:
   ```bash
   cd build
   ctest --output-on-failure
   ```
   - Takes ~30-60 seconds
   - Excludes FMPClientTest (requires API key)
   - Individual test executables are in `build/Tests/`

2. **Run specific tests**:
   ```bash
   ./Tests/test_barcache --stock-csv=../Example_Config/nasdaq_screener.csv
   ./Tests/test_RunUpDetector
   ./Tests/test_tradestationclient
   ```

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
- **Qt version mismatch**: Local builds use Qt 6.8.3, CI uses 6.4.2. Some APIs differ (e.g., `QTimeZone::fromName()` not available in 6.4.2)
- **Missing dependencies**: Ensure Qt6 development packages are installed
- **Build failures**: Always clean build directory if switching Qt versions
- **Test failures**: FMPClientTest requires API key, excluded by default
- **Timing**: Full build takes 2-5 minutes, tests take 30-60 seconds

### Validation Steps
- **Build success**: Check for zero exit code from cmake --build
- **Test success**: All tests pass except FMPClientTest
- **Run success**: Application starts without crashes
- **CI validation**: GitHub Actions workflows run on push/PR

## Project Layout

### Major Architectural Elements
- **src/**: Main source code
  - **Algo/**: Trading algorithms and stock screening
  - **Clients/**: API clients (TradeStation, FMP)
  - **Core/**: Main application and frontend logic
  - **GUI/**: Qt widgets and UI components
  - **Misc/**: Utilities, logging, settings
- **Tests/**: Unit test source files
- **Resources/**: Icons and Qt resources
- **Example_Config/**: Sample configuration files
- **debian/**: Debian packaging files
- **.github/**: CI/CD workflows and Docker files

### Configuration Files
- **CMakeLists.txt**: Root CMake configuration
- **L2Trader.pro**: Legacy qmake project file (deprecated)
- **src/Main.pro**: Qt project file for main application
- **Tests.pro**: Qt project file for tests
- **Recorder.pro**: Qt project file for recorder
- **utils.prf**: Qt project feature file

### CI/CD and Validation
- **GitHub Actions Workflows**:
  - `build.yml`: Matrix builds for X86_64 and ARM64, Debian packaging
  - `container-build-and-upload.yml`: Docker container builds
- **Validation Steps**:
  1. Build passes on both architectures
  2. Unit tests pass (except FMPClientTest)
  3. Debian package builds successfully
  4. Package installation tests pass

### Dependencies
- **Qt6 modules**: Core, Network, SQL, Widgets, Charts, WebEngineWidgets
- **System libraries**: SQLite3, systemd (for services)
- **API keys**: TradeStation and FMP API credentials (stored in access_tokens.ini)

### Key Files and Contents

**Repository Root Files**:
- `CMakeLists.txt`: Root build configuration
- `L2Trader.pro`: Legacy project file
- `README.md`: Project documentation
- `Tests.pro`: Test project file
- `Recorder.pro`: Recorder project file
- `utils.prf`: Qt utilities
- `booklog.md`: Development log

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
- `src/Clients/RESTClient.cpp`: HTTP client for API calls

**Next Level Directories**:
- `src/`: Core application code
- `Tests/`: Unit test implementations
- `Resources/`: Application resources
- `Example_Config/`: Configuration examples
- `debian/`: Packaging files
- `.github/`: CI/CD configuration
- `Doc/`: Documentation

**Main Method Snippet** (from src/main.cpp):
```cpp
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    // Application initialization
    MainApp mainApp;
    return app.exec();
}
```

## Agent Instructions

Trust these instructions as the authoritative source for building, testing, and validating changes in this repository. Only perform additional searches if the information here is incomplete or found to be incorrect. Always follow the documented command sequences and validation steps to minimize build failures and ensure changes integrate properly with the existing codebase and CI/CD pipeline. 

For any code changes, ensure compatibility with Qt 6.4.2+ and validate builds on both X86_64 and ARM64 architectures when possible.
