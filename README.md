# L2Trader

A real-time algorithmic trading application built with Qt6 that monitors stock market data, executes trading strategies, and provides comprehensive market analysis tools.

## Overview

L2Trader is a sophisticated trading platform that connects to TradeStation and Financial Modeling Prep (FMP) APIs to provide real-time market data analysis, Level 2 market depth visualization, automated stock screening, and breaking news monitoring. The application features both a rich GUI interface and a terminal-based mode for different use cases.

## Key Features

- **Real-Time Market Data**: Live streaming of stock prices, market depth (Level 2) quotes, and order book data
- **Automated Stock Screening**: Filter stocks based on customizable criteria including price range, float, relative volume, and gap percentage
- **Breaking News Monitoring**: Continuous monitoring and filtering of breaking news for selected stocks
- **Level 2 Market Depth**: Visual representation of bid/ask depth and order book imbalances
- **Position Tracking**: Real-time monitoring of trading positions across multiple accounts
- **Interactive Price Charts**: Candlestick charts with zoom/pan capabilities and market hours indicators
- **Algorithmic Trading**: Run-up detection and pattern recognition algorithms
- **Memory-Efficient Caching**: Smart bar data caching with SQLite persistence
- **Logging & Diagnostics**: Comprehensive logging system with runtime category controls

## Architecture

L2Trader follows a modular Model-View-Controller architecture with Qt's signal/slot mechanism for asynchronous communication. The application consists of several key components:

- **API Clients**: TradeStation and FMP REST clients for external data
- **Algorithm Core**: MainAlgo coordinates stock screening, news fetching, and trading signals
- **GUI Frontend**: Interactive charts, market depth tables, and configuration panels
- **Data Management**: Bar cache, position receiver, and market depth quote handler

For detailed architecture diagrams, see:
- [Application Architecture Diagram](Doc/Application_Architecture_Diagram.md)
- [Stock Price Chart Diagram](Doc/StockPriceChart_Diagram.md)
- [Stock Price Chart Architecture (Comprehensive)](Doc/StockPriceChart_Architecture.md) - Detailed documentation with crash analysis
- [Program Sequence](Doc/ProgramSequence.md)
- [OAuth Authentication Process](Doc/OAuth_Authentication_Process.md)

## Prerequisites

### Required Dependencies

- **Qt 6.x**: Core, Network, SQL, Widgets, Charts, WebEngineWidgets
- **C++17 compliant compiler**: GCC 7+ or Clang 5+
- **CMake 3.16+**: Build system
- **SQLite**: Database support (included with Qt SQL)

### API Keys

You will need API credentials for:
- **TradeStation API**: For real-time market data and trading
- **FMP API**: For financial data and stock screening

## Installation

### Building from Source

1. **Clone the repository**:
   ```bash
   git clone https://github.com/simonbeaudoin0935/L2Trader.git
   cd L2Trader
   ```

2. **Install Qt dependencies**:
   ```bash
   # On Ubuntu/Debian
   sudo apt-get install qt6-base-dev qt6-charts-dev qt6-webengine-dev libqt6sql6-sqlite cmake

   # On macOS with Homebrew
   brew install qt@6 cmake
   ```

3. **Build the application**:
   ```bash
   mkdir build
   cd build
   cmake ..
   cmake --build . --parallel
   ```

4. **Run the application**:
   ```bash
   ./src/L2Trader
   ```

### Building with GUI Disabled

For terminal-only mode, build without GUI support:
```bash
mkdir build
cd build
cmake .. -DENABLE_GUI=OFF
cmake --build . --parallel
```

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

Store your API credentials in `access_tokens.ini` (this file is gitignored for security):

```ini
[TradeStation]
api_key=your_tradestation_api_key
api_secret=your_tradestation_api_secret
refresh_token=your_refresh_token

[FMP]
api_key=your_fmp_api_key
```

### Logging Configuration

The application uses Qt's logging categories for runtime control. Logs are stored in:
- Linux: `~/.local/state/L2Trader/logs/`
- macOS: `~/Library/Application Support/L2Trader/logs/`
- Windows: `%LOCALAPPDATA%/L2Trader/logs/`

## Usage

### GUI Mode

Launch the application with the GUI interface:

```bash
./L2Trader
```

The GUI provides:
- **Trade Tab**: Real-time charts, market depth tables, and position tracking
- **Settings Tab**: Cache management and logging controls
- Interactive stock symbol selection with auto-complete
- Keyboard shortcuts (Ctrl+Q to quit)

### Terminal Mode

For headless operation, build the application without GUI support (see "Building with GUI Disabled" above). The terminal frontend will be used automatically when GUI is not compiled in.

### Development Workflow

The application automatically:
1. Authenticates with TradeStation and FMP APIs
2. Loads stock screening criteria
3. Performs initial stock screening based on configured parameters
4. Begins monitoring breaking news for screened stocks
5. Streams real-time market data for selected instruments
6. Updates charts and market depth displays in real-time

## Development

### Project Structure

```
L2Trader/
├── src/
│   ├── Algo/              # Trading algorithms and stock screening
│   ├── Clients/           # API clients (TradeStation, FMP)
│   ├── Core/              # Main application and frontend
│   ├── GUI/               # Qt widgets and UI components
│   └── Misc/              # Utilities, logging, settings
├── Tests/                 # Unit tests
├── Resources/             # Icons and resources
├── Example_Config/        # Example configuration files
└── *.md                   # Architecture documentation
```

### Running Tests

Build and run the test suite:

```bash
mkdir build
cd build
cmake .. -DBUILD_TESTS=ON
cmake --build . --parallel
ctest
# Or run individual tests:
./Tests/test_barcache
./Tests/test_fmpclient
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

- C++17 standard
- Qt naming conventions for Qt classes
- camelCase for methods and variables
- Header guards using `#pragma once`

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Security

- **Never commit API keys** to the repository
- Store credentials in `access_tokens.ini` (gitignored)
- Use secure token storage mechanisms
- Review API rate limits to avoid account suspension

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Built with Qt6 framework
- Integrates with TradeStation API for market data
- Uses Financial Modeling Prep (FMP) API for stock screening
- Inspired by professional trading platforms and market analysis tools

## Support

For issues, questions, or contributions, please open an issue on the GitHub repository.

---

**Note**: This is a live trading application. Use with caution and test thoroughly with paper trading accounts before using real capital.
