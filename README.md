[![Build](https://github.com/simonbeaudoin0935/L2Trader/actions/workflows/build.yml/badge.svg?branch=main&event=push)](https://github.com/simonbeaudoin0935/L2Trader/actions/workflows/build.yml)

# L2Trader

A real-time algorithmic trading application built with Qt6 that monitors stock market data, executes trading strategies, and provides comprehensive market analysis tools.

## Overview

L2Trader is a sophisticated trading platform that connects to the TradeStation API to provide real-time market data analysis, Level 2 market depth visualization, and position tracking. The application features a rich GUI interface with interactive charts and market depth displays.

## Key Features

- **Real-Time Market Data**: Live streaming of stock prices, market depth (Level 2) quotes, and order book data via TradeStation API
- **Stock Screening**: Filter stocks based on customizable criteria including price range, float, relative volume, and gap percentage
- **Level 2 Market Depth**: Visual representation of bid/ask depth and order book imbalances
- **Position Tracking**: Real-time monitoring of trading positions across multiple accounts
- **Interactive Price Charts**: Candlestick charts using QCustomPlot with zoom/pan capabilities and market hours indicators
- **Pattern Recognition**: Run-up detection and algorithmic trading signals
- **Memory-Efficient Caching**: Smart bar data caching with SQLite persistence
- **Live Data Recording**: Recorder application for capturing market data to SQLite databases
- **Logging & Diagnostics**: Comprehensive logging system with runtime category controls

## Architecture

L2Trader follows a modular Model-View-Controller architecture with Qt's signal/slot mechanism for asynchronous communication. The application consists of several key components:

- **API Client**: TradeStation REST client for external market data
- **Algorithm Core**: MainAlgo coordinates stock screening and trading signals
- **GUI Frontend**: Interactive charts using QCustomPlot, market depth tables, and configuration panels
- **Data Management**: Bar cache, position receiver, and market depth quote handler

For detailed architecture diagrams, see:
- [Application Architecture Diagram](Doc/Application_Architecture_Diagram.md)
- [Stock Price Chart Diagram](Doc/StockPriceChart_Diagram.md)
- [Stock Price Chart Architecture (Comprehensive)](Doc/StockPriceChart_Architecture.md) - Detailed documentation with crash analysis
- [Program Sequence](Doc/ProgramSequence.md)
- [OAuth Authentication Process](Doc/OAuth_Authentication_Process.md)

## Prerequisites

### Required Dependencies

- **Qt 6.x**: Core, Network, SQL, Widgets, PrintSupport
- **C++23 compliant compiler**: GCC 11+ or Clang 12+
- **CMake 3.16+**: Build system
- **SQLite**: Database support (included with Qt SQL)
- **QCustomPlot**: Charting library (included in repository)

### API Keys

You will need API credentials for:
- **TradeStation API**: For real-time market data and trading (OAuth-based authentication using system browser)

## Installation

### Building from Source

1. **Clone the repository**:
   ```bash
   git clone https://github.com/simonbeaudoin0935/L2Trader.git
   cd L2Trader
   ```

2. **Install dependencies**:
   ```bash
   # On Ubuntu/Debian - Essential packages
   sudo apt-get install qt6-base-dev libqt6sql6-sqlite cmake

   # On Ubuntu/Debian - Optional packages (recommended for development)
   sudo apt-get install uncrustify ccache

   # On macOS with Homebrew - Essential packages
   brew install qt@6 cmake

   # On macOS with Homebrew - Optional packages (recommended for development)
   brew install uncrustify ccache
   ```
   
   **Note**: `uncrustify` is needed for the pre-commit formatting hook. `ccache` speeds up rebuilds significantly.

3. **Set up Git hooks** (optional but recommended for contributors):
   ```bash
   ./Utils/install-git-hooks.sh
   ```
   This installs a pre-commit hook that checks code formatting. See [Git Pre-Commit Hook documentation](Doc/Git_Pre_Commit_Hook.md) for details.

4. **Build the application**:
   ```bash
   mkdir build
   cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_GUI=ON
   cmake --build . --parallel
   ```

5. **Run the application**:
   ```bash
   ./src/L2Trader
   ```

### Building with GUI Disabled

For terminal-only mode without GUI dependencies:
```bash
mkdir build
cd build
cmake .. -DENABLE_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

**Note**: The TUI (Terminal User Interface) frontend is currently a skeleton implementation and is not fully functional.

## Configuration

### Selection Criteria

Create a `selection_criteria.ini` file to define stock screening parameters. See `Example_Config/selection_criteria.ini` for reference:

```ini
[Criterias]
PriceRangeLow=2.00
PriceRangeHigh=10.00
PreferedFloat=20000000
MaxFloat=100000000
RelativeVolume=5.0
GapPercentage=5.0
```

### API Credentials

API credentials are managed through the application's OAuth authentication flow:

- **TradeStation**: The application uses OAuth authentication via your system's default web browser. When you first run the application, you'll be prompted to log in to TradeStation to authorize the app. Keep the authentication window open while completing the login in your browser.

### Logging Configuration

The application uses Qt's logging categories for runtime control. Logs are stored in:
- Linux: `~/.local/state/L2Trader/logs/`
- macOS: `~/Library/Application Support/L2Trader/logs/`
- Windows: `%LOCALAPPDATA%/L2Trader/logs/`

## Usage

### GUI Mode

Launch the application with the GUI interface:

```bash
./L2Trader --criterias=../Example_Config/selection_criteria.ini
```

The GUI provides:
- **Trade Tab**: Real-time charts, market depth tables, and position tracking
- **Settings Tab**: Cache management and logging controls
- Interactive stock symbol selection with auto-complete
- Keyboard shortcuts (Ctrl+Q to quit)

### Terminal Mode

Terminal mode (TUI) is currently not fully implemented. Building with `-DENABLE_GUI=OFF` will create a console-only application, but the TUI frontend is a skeleton implementation with placeholder functionality.

### Recorder Mode

The Recorder application captures live market data (bars and market depth) to SQLite databases for later analysis or replay.

```bash
./Recorder --stock-csv=../Example_Config/nasdaq_screener.csv
```

By default, data is written to `~/.cache/Recorder/RecordedLiveData/`. To specify a custom directory (e.g., external USB drive):

```bash
./Recorder --stock-csv=../Example_Config/nasdaq_screener.csv --recorded-data-dir=/mnt/ssd
```

Alternatively, you can change the cache root directory (which will store data under `<dir>/Recorder/RecordedLiveData/`):

```bash
./Recorder --stock-csv=../Example_Config/nasdaq_screener.csv --cache-root-dir=/mnt/ssd
```

### Development Workflow

The application automatically:
1. Authenticates with TradeStation API via OAuth
2. Loads stock screening criteria from configuration file
3. Begins monitoring selected stocks
4. Streams real-time market data for selected instruments
5. Updates charts and market depth displays in real-time

## Development

### Project Structure

```
L2Trader/
├── Src/
│   ├── Algo/              # Trading algorithms and stock screening
│   ├── Clients/           # API clients (TradeStation only)
│   ├── Core/              # Main application logic
│   ├── FrontEnd/          # GUI and TUI frontend implementations
│   │   ├── GUI/           # Qt widgets, charts, and UI components
│   │   └── TUI/           # Terminal UI (skeleton/placeholder)
│   ├── Misc/              # Utilities, logging, settings
│   └── Recorder/          # Recorder application for live data capture
├── Tests/                 # Unit tests
├── qcustomplot/           # QCustomPlot charting library
├── Resources/             # Icons and resources
├── Example_Config/        # Example configuration files
└── Doc/                   # Architecture documentation
```

### Running Tests

Build and run the test suite:

```bash
mkdir build
cd build
cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel
ctest --output-on-failure
# Or run individual tests:
./Tests/test_barcache --stock-csv=../Example_Config/nasdaq_screener.csv
./Tests/test_RunUpDetector
./Tests/test_tradestationclient
```

### IDE Setup

The project includes VSCode configuration in `.vscode/`. For Qt Creator:

1. Open `CMakeLists.txt`
2. Configure build settings for your kit
3. Select run configuration "L2Trader"
4. Build and run

### Code Style

- C++23 standard
- Qt naming conventions for Qt classes
- Member variables: `m_` prefix
- Global variables: `g_` prefix
- Parameters: `p_` prefix
- camelCase for methods and local variables
- Header guards using `#pragma once`
- Early return/exit style for error handling
- Indentation: 4 spaces (no tabs)
- Line endings: Unix (LF)
- Automatic formatting via Uncrustify (see `.uncrustify.cfg`)

**Code Formatting**: Contributors should install the pre-commit hook to automatically check formatting before commits:
```bash
./Utils/install-git-hooks.sh
```
See [Git Pre-Commit Hook documentation](Doc/Git_Pre_Commit_Hook.md) for more information.

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Security

- **Never commit API keys** to the repository
- OAuth credentials are managed through TradeStation's authentication flow using your system browser
- Use secure token storage mechanisms (QKeychain for OAuth tokens)
- Review API rate limits to avoid account suspension

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Built with Qt6 framework
- Uses QCustomPlot for high-performance charting
- Integrates with TradeStation API for market data
- Inspired by professional trading platforms and market analysis tools

## Support

For issues, questions, or contributions, please open an issue on the GitHub repository.

---

**Note**: This is a live trading application. Use with caution and test thoroughly with paper trading accounts before using real capital.
