[![Build](https://github.com/simonbeaudoin0935/L2Trader/actions/workflows/build.yml/badge.svg?branch=main&event=push)](https://github.com/simonbeaudoin0935/L2Trader/actions/workflows/build.yml)

# L2Trader

A real-time algorithmic trading application built with Qt6 that monitors stock market data, executes trading strategies, and provides comprehensive market analysis tools.

## Overview

L2Trader is a sophisticated trading platform with a dual-API architecture:

- **Databento** — provides all live market data: Level 2 order book (10-level), trades, and historical bars via the `databento-cpp` C++ client library.
- **TradeStation** — handles all brokerage operations: OAuth authentication, order placement/cancellation, position streaming, and account management.

The application features a rich GUI with interactive candlestick charts, a 10-level market depth display, order entry, position tracking, and a historical replay system for backtesting.

## Key Features

- **Real-Time Market Data via Databento**: Live Level 2 (10-level order book), trades, and per-minute bars streamed from NASDAQ TotalView (`XNAS.ITCH`) using `databento-cpp`
- **Brokerage Integration via TradeStation**: Order placement and cancellation, real-time position and order streaming, account balances — authenticated via OAuth 2.0
- **Level 2 Market Depth**: 10-level bid/ask display with per-level size and count, updated on every book change
- **Interactive Price Charts**: Candlestick charts (QCustomPlot) with pre-market/after-hours session backgrounds, volume bars, last-price line, and buy/sell order markers
- **Position & Order Tracking**: Real-time positions with unrealized P&L overlay on chart, order history with fill markers
- **Historical Replay**: Download and replay Databento `.dbn.zst` archive files at configurable speeds (0.01× to as-fast-as-possible) with emulated order execution
- **Order Emulation in Replay**: Realistic order lifecycle simulation (reception delay, execution delay, limit-order fill logic against live depth) via `OrderEmulator`
- **External Strategy Processes**: Load custom trading strategies as host-supervised executables communicating over Unix sockets + Protobuf, so strategy crashes stay isolated from the platform
- **Trading Status Indicators**: Live HALTED and Hard-to-Borrow (HTB/SSR) flag display, driven by Databento `StatusMsg` records
- **Market Calendar Awareness**: Holiday and early-close calendar for 2026; distinguishes `PreMarket`, `Regular`, `AfterHours`, `Weekend`, and `Holiday` sessions
- **Memory-Efficient Bar Cache**: Two-tier in-memory + SQLite bar storage with thread-safe access; bars are ~104 bytes each
- **Dual Frontend**: Full-featured Qt Widgets GUI or lightweight ncurses TUI for headless servers
- **Comprehensive Logging**: Qt logging categories with runtime filter controls and persistent log files

## Architecture

L2Trader follows a Model-View-Controller architecture with Qt's signal/slot mechanism for asynchronous, thread-safe communication. The three core singletons each run in a dedicated worker thread:

| Singleton | Thread | Responsibility |
|-----------|--------|----------------|
| **DBClient** | Databento internal thread | Live market data (Level 2, trades, bars) |
| **TSClient** | TSClient worker thread | Brokerage (OAuth, orders, positions, accounts) |
| **MainAlgo** | MainAlgo worker thread | Trading logic, bar processing, strategy coordination |

For detailed documentation, see:
- **[Doc/README.md](Doc/README.md)** — Documentation index and quick reference
- **[Doc/ARCHITECTURE.md](Doc/ARCHITECTURE.md)** — System architecture, components, threading, and design patterns
- **[Doc/AUTHENTICATION.md](Doc/AUTHENTICATION.md)** — OAuth 2.0 (TradeStation) and Databento API key management
- **[Doc/FRONTEND.md](Doc/FRONTEND.md)** — GUI and TUI frontend architecture and components
- **[Doc/DEVELOPMENT.md](Doc/DEVELOPMENT.md)** — Development setup, building, testing, and coding guidelines
- **[Doc/CONTRIBUTING.md](Doc/CONTRIBUTING.md)** — Contribution process and standards
- **[Doc/STRATEGY.md](Doc/STRATEGY.md)** — Strategy process system and SDK guide

## Prerequisites

### Required Dependencies

- **Qt 6.4.2+**: Core, Network, SQL, Widgets, PrintSupport
- **C++23 compliant compiler**: GCC 11+ or Clang 12+
- **CMake 3.16+**: Build system
- **SQLite**: Database support (included with Qt SQL)
- **Protocol Buffers (protobuf)**: `protoc` + `libprotobuf` for the new out-of-process strategy protocol / SDK targets
- **databento-cpp**: Bundled in `Lib/` — requires `libssl-dev` and `libzstd-dev`
- **QCustomPlot**: Charting library (included in `Lib/QCustomPlot/`)

### API Keys

- **Databento API key**: For live market data streaming and historical data downloads. Enter via the in-app Databento connection dialog (stored securely in `SecureStorage.ini`).
- **TradeStation API credentials** (Client ID + Secret): For brokerage operations. Obtained from the TradeStation developer portal; entered on first launch via OAuth browser flow.

## Installation

### Building from Source

1. **Clone the repository**:
   ```bash
   git clone https://github.com/simonbeaudoin0935/L2Trader.git
   cd L2Trader
   ```

2. **Install dependencies**:
   ```bash
   # Ubuntu/Debian — essential packages
    sudo apt-get install cmake build-essential qt6-base-dev libqt6sql6-sqlite \
                         libprotobuf-dev protobuf-compiler libssl-dev libzstd-dev ninja-build

   # Ubuntu/Debian — optional (recommended for development)
   sudo apt-get install clang-format ccache libqtkeychain-qt6-dev libncurses-dev

   # macOS (Homebrew) — essential packages
   brew install qt@6 cmake openssl zstd ninja

   # macOS (Homebrew) — optional
   brew install clang-format ccache
   ```

   > **Note**: `libqtkeychain-qt6-dev` enables OS-level secure storage for OAuth tokens. Without it, tokens are stored with XOR obfuscation (development only).

3. **Set up Git hooks** (optional but recommended for contributors):
   ```bash
   ./Utils/install-git-hooks.sh
   ```
   This installs a pre-commit hook that enforces `clang-format` on staged files.

4. **Build the application**:
    ```bash
    mkdir -p build/GUI
    cmake -S . -B build/GUI -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_GUI=ON -DBUILD_TESTS=OFF
    cmake --build build/GUI -j4
    ```

    This also builds the first `MCP/` integration binary under `build/GUI/bin/`:
    - `l2trader-mcp-server`

    `l2trader-mcp-server` exposes the platform control operations, replay-data
    discovery/download tools, chart-log creation, account and balance queries,
    position/order queries, poll-style market-data tools, and order
    placement/cancellation (including optional order-bound logs) over MCP
    stdio, then talks to the platform's local control socket internally.

5. **Run the application**:
    ```bash
    ./build/GUI/Src/L2Trader
    ```

    Once the app is running, the local control socket listens at:
    `~/.local/state/L2Trader/platform-control.sock`

    The control socket is intended to be consumed through `l2trader-mcp-server`
    rather than a separate standalone CLI.

6. **Stage the external strategy SDK from a build tree** (optional):
   ```bash
   cmake --build build/GUI --target stage-strategy-sdk
   ```

    This creates an install-style SDK layout under `build/GUI/strategy-sdk/` with:
    - `include/L2Trader/...` public headers
    - `lib*/libL2TraderStrategySDK.so*` shared library artifacts
    - `share/l2trader/proto/` canonical `.proto` files
    - `lib*/cmake/L2TraderStrategySDK/` `find_package()` metadata

    The GUI strategy load dialog loads external strategy executables directly.
    Each executable is introspected via `--describe-strategy` before launch, and
    external strategies run as host-supervised child processes over the
    Unix-socket + Protobuf runtime.

### Building with Sanitizers (for development)

**UndefinedBehaviorSanitizer (UBSan)**:
```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=ON -DENABLE_UBSAN=ON
cmake --build build/debug -j4
```

**AddressSanitizer (ASan)**:
```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=ON -DENABLE_ASAN=ON
cmake --build build/debug -j4
```

See [DEVELOPMENT.md](Doc/DEVELOPMENT.md#code-quality-tools) for more details on sanitizers.

### LTTng Kernel Tracing (for thread/memory diagnostics)

LTTng captures kernel-level events (heap growth, thread scheduling) and L2Trader userspace tracepoints with nanosecond precision. Use it to diagnose performance issues, memory growth, or unexpected thread behaviour.

**Install once:**
```bash
sudo apt install lttng-tools lttng-modules-dkms babeltrace2
```

**Run the app under LTTng (via VSCode task `run-lttng`, or manually):**
```bash
# Kernel-only tracing (normal build)
.sanitizers/lttng/run-with-lttng.sh

# Kernel + UST userspace tracepoints (instrumented build)
APP=./build/LTTng/Src/L2Trader .sanitizers/lttng/run-with-lttng.sh
```

Traces are saved to `~/.local/state/L2Trader/lttng-traces/<timestamp>/kernel/` and `.../ust/`. No sudo password is needed — see `/etc/sudoers.d/l2trader-lttng`.

**Query a trace with babeltrace2 or open in TraceCompass.** For query recipes and event reference, see the [LTTng Copilot skill](.github/skills/lttng/SKILL.md).

**VSCode tasks available:**

| Task | Description |
|------|-------------|
| `run-lttng` | Launch app under kernel-only LTTng |
| `run-lttng-instrumented` | Launch instrumented build (kernel + UST) |
| `build-with-lttng` | Build the LTTNG_ENABLED binary |

### Building TUI Mode (headless)

For terminal-only mode without Qt Widgets or GUI dependencies:
```bash
mkdir -p build/TUI
cmake -S . -B build/TUI -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_GUI=OFF -DBUILD_TESTS=OFF
cmake --build build/TUI -j4
./build/TUI/Src/L2Trader 2>logs.txt   # logs to stderr; stdout is ncurses UI
```

See [FRONTEND.md](Doc/FRONTEND.md) for TUI features and keyboard shortcuts.

## Configuration

### Databento API Key

On first launch, click the **Databento** connection button in the toolbar. Enter your API key in the dialog. The key is stored securely (XOR-obfuscated in `~/.config/L2Trader/SecureStorage.ini` under `[Databento]/api_key`). Once connected, the app subscribes to live Level 2 + Trades + Status streams for the selected symbol.

### TradeStation OAuth

On first launch, you will be prompted to enter your TradeStation Client ID and Client Secret. The app opens your system browser to complete the OAuth 2.0 Authorization Code Flow. Tokens are stored securely via QKeychain (or the obfuscated fallback). See [AUTHENTICATION.md](Doc/AUTHENTICATION.md) for details.

### Logging

Qt logging categories with runtime filter controls. Logs stored at:
- Linux: `~/.local/state/L2Trader/logs/`
- macOS: `~/Library/Application Support/L2Trader/logs/`

## Usage

### GUI Mode

```bash
./build/GUI/Src/L2Trader
```

The main window provides:
- **Trade Tab**: Real-time candlestick chart with order/position overlays, 10-level market depth display, order entry panel, and strategy quick-view
- **Config Tab**: Databento connection management (API key, dataset selection), Databento status display
- **Cache Tab**: Bar cache management (view, clear, preload)
- **Logging Tab**: Live log display with category filter controls
- **Strategies Tab**: Load, start, stop, and monitor external strategy processes
- **Dock widgets** (bottom): Orders table, Positions table, Balance display
- **Status bar**: Network data usage, memory usage
- **Toolbar**: Symbol input, account selector, data source selector (Live / Replay), replay playback controls

### Replay Mode

1. From the toolbar, click the data source label to enter **Replay Mode**.
2. Select a date. If replay data is not yet downloaded, use the **Records Info** tab to download Databento `.dbn.zst` files for the desired symbol and date.
3. Use the play/pause button and speed selector (0.01× to as-fast-as-possible) to control playback.
4. Orders placed during replay are handled locally by the `OrderEmulator` — no real brokerage calls are made.

### TUI Mode

```bash
./build/TUI/Src/L2Trader 2>logs.txt
```

The TUI provides a ncurses-based monitoring view with order and position tables. Order entry is not available in TUI mode.

## Development

### Project Structure

```
L2Trader/
├── Src/
│   ├── Algo/              # MainAlgo, stock instruments, receivers (bars, L2, trades)
│   ├── Clients/
│   │   ├── DBClient/      # Databento client (live streaming + historical + replay download)
│   │   └── TSClient/      # TradeStation brokerage client (OAuth, orders, positions)
│   ├── Core/
│   │   ├── Cache/BarCache/ # Two-tier bar cache (memory + SQLite)
│   │   └── Replay/        # ReplayEngine + OrderEmulator
│   ├── FrontEnd/
│   │   ├── GUI/           # Qt Widgets interface (charts, Level2, order entry)
│   │   └── TUI/           # ncurses terminal interface
│   ├── Misc/              # Constants, logging, settings, secure storage
│   ├── SQL/               # Centralized SQL query headers
│   ├── Strategy/          # Strategy host orchestration and process supervision backends
│   ├── StrategyProtocol/  # Protobuf schemas for out-of-process strategy IPC
│   └── StrategySDK/       # Installable public SDK for external strategy executables
├── Strategies/            # Self-describing out-of-process strategy executable samples
├── Tests/                 # Unit tests
├── Lib/                   # Third-party libraries (databento-cpp, QCustomPlot)
├── Resources/             # Icons and resources
├── Example_Config/        # Example configuration files
└── Doc/                   # Architecture documentation
```

### Running Tests

```bash
cmake -S . -B build/test -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=ON -DBUILD_TESTS=ON
cmake --build build/test -j4
ctest --test-dir build/test --output-on-failure
```

### Code Style

- C++23 standard; 4-space indentation; Unix line endings
- Member variables: `m_` prefix; global: `g_`; parameters: `p_`
- `camelCase` for methods and local variables; `PascalCase` for classes
- `#pragma once` header guards; early-return error handling
- Automatic formatting via `clang-format` (see `.clang-format`)

Install the pre-commit formatting hook:
```bash
./Utils/install-git-hooks.sh
```

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](Doc/CONTRIBUTING.md) for detailed guidelines.

Quick start:
1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Install Git hooks: `./Utils/install-git-hooks.sh`
4. Follow the [coding guidelines](Doc/DEVELOPMENT.md#coding-guidelines)
5. Ensure tests pass: `ctest --test-dir build/test --output-on-failure`
6. Open a Pull Request

## Security

- **Never commit API keys or tokens** to the repository
- TradeStation OAuth tokens stored via QKeychain (OS-level encryption)
- Databento API key stored with XOR obfuscation in `SecureStorage.ini` (not committed)
- All brokerage communication over HTTPS/WSS only

## License

This project is licensed under the MIT License — see the LICENSE file for details.

## Acknowledgments

- Built with the **Qt6** framework
- Uses **QCustomPlot** for high-performance charting
- Uses **databento-cpp** for NASDAQ TotalView market data
- Integrates with **TradeStation API** for brokerage services

## Support

For issues, questions, or feature requests, please open an issue on the GitHub repository.

---

**Note**: This is a live trading application. Always test thoroughly with paper trading accounts before using real capital.
