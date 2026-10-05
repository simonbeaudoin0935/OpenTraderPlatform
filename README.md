[![Build](https://github.com/simonbeaudoin0935/OpenTraderPlatform/actions/workflows/pull-request.yml/badge.svg?branch=main&event=push)](https://github.com/simonbeaudoin0935/OpenTraderPlatform/actions/workflows/pull-request.yml)

# OpenTraderPlatform

A real-time algorithmic trading application built with Qt6 that monitors stock market data, executes trading strategies, and provides comprehensive market analysis tools.

## Overview

OpenTraderPlatform is a sophisticated trading platform with a dual-API architecture:

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
- **Comprehensive Logging**: Qt logging categories with runtime filter controls and persistent log files

## Architecture

OpenTraderPlatform follows a Model-View-Controller architecture with Qt's signal/slot mechanism for asynchronous, thread-safe communication. The three core singletons each run in a dedicated worker thread:

| Singleton | Thread | Responsibility |
|-----------|--------|----------------|
| **DBClient** | Databento internal thread | Live market data (Level 2, trades, bars) |
| **TSClient** | TSClient worker thread | Brokerage (OAuth, orders, positions, accounts) |
| **MainAlgo** | MainAlgo worker thread | Trading logic, bar processing, strategy coordination |

For detailed documentation, see:
- **[Doc/README.md](Doc/README.md)** — Documentation index and quick reference
- **[Doc/ARCHITECTURE.md](Doc/ARCHITECTURE.md)** — System architecture, components, threading, and design patterns
- **[Doc/AUTHENTICATION.md](Doc/AUTHENTICATION.md)** — OAuth 2.0 (TradeStation) and Databento API key management
- **[Doc/DEVELOPMENT.md](Doc/DEVELOPMENT.md)** — Development setup, building, testing, and coding guidelines
- **[Doc/CONTRIBUTING.md](Doc/CONTRIBUTING.md)** — Contribution process and standards
- **[Doc/STRATEGY.md](Doc/STRATEGY.md)** — Strategy process system and SDK guide

## Prerequisites

### Required Dependencies

- **Qt 6.4.2+**: Core, Network, SQL, Widgets, PrintSupport
- **Qt6Keychain**: Required OS keyring integration (`qtkeychain-qt6-dev` on Ubuntu/Debian, `qtkeychain-qt6` on Arch)
- **C++23 compiler and standard library**: GCC 13 is the verified toolchain. Other compilers must provide `std::expected` and `std::stacktrace` with a supported `libstdc++` stacktrace backend.
- **CMake 3.16+**: Build system
- **SQLite**: Database support (included with Qt SQL)
- **Protocol Buffers (protobuf)**: `protoc` + `libprotobuf` for the new out-of-process strategy protocol / SDK targets
- **Python 3**: Generates Qt logging categories during the build
- **databento-cpp**: Bundled in `Lib/` — requires `libssl-dev` and `libzstd-dev`
- **QCustomPlot**: Charting library (included in `Lib/QCustomPlot/`)
- **Optional YubiKey storage**: `yubikey-manager` (`ykman`) and an OTP-capable YubiKey with touch-enabled HMAC challenge-response in Slot 2

### API Keys

- **Databento API key**: For live market data streaming and historical data downloads. Enter via the in-app Databento connection dialog; stored in the OS keyring.
- **TradeStation API credentials** (Client ID + Secret): For brokerage operations. Obtained from the TradeStation developer portal; entered on first launch via OAuth browser flow.

## Installation

### Building from Source

1. **Clone the repository**:
   ```bash
   git clone --recurse-submodules https://github.com/simonbeaudoin0935/OpenTraderPlatform.git
   cd OpenTraderPlatform
   ```

   If you already cloned without submodules, initialize them with:
   ```bash
   git submodule update --init --recursive
   ```

   The first CMake configure also fetches databento-cpp's third-party dependencies, so network access is required during configuration.

2. **Install dependencies**:
   ```bash
   # Ubuntu/Debian — essential packages
   sudo apt-get update
   sudo apt-get install git python3 cmake build-essential qt6-base-dev libqt6sql6-sqlite \
                         qtkeychain-qt6-dev libprotobuf-dev protobuf-compiler libssl-dev libzstd-dev ninja-build

   # Arch/Omarchy
   sudo pacman -S git python cmake base-devel qt6-base qtkeychain-qt6 protobuf openssl zstd ninja gcc gcc-libs
   ```

   > **Credential storage**: Qt6Keychain is mandatory. Run the app in a desktop session with a working, unlocked native keyring (for example GNOME Keyring or KWallet on Linux). Saving credentials fails explicitly if the keyring is unavailable; there is no file-backed fallback. On first use after upgrading, obsolete obfuscated credential entries are discarded without migration; re-enter TradeStation and Databento credentials. This does not erase credentials from backups or old logs.

   > **Arch/Omarchy note**: No extra packages are needed beyond the list above — `CMakeLists.txt`
   > auto-detects the correct `std::stacktrace` backend library (Arch's newer GCC ships
   > `libstdc++exp.a` instead of Debian/Ubuntu's `libstdc++_libbacktrace`). `Src/CMakeLists.txt`
   > prefers Protobuf's upstream CONFIG package, which carries Abseil dependencies on Arch, and
   > falls back to CMake's `FindProtobuf` module on distributions such as Ubuntu that package only
   > the module with `libprotobuf-dev`.

3. **Set up Git hooks** (optional but recommended for contributors):
   ```bash
   ./Utils/install-git-hooks.sh
   ```
   This installs a pre-commit hook that enforces `clang-format` on staged files.

4. **Build the application**:
    ```bash
    mkdir -p build
    cmake -Wno-deprecated -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
    cmake --build build -j4
    ```

    This also builds the first `MCP/` integration binary under `build/bin/`:
    - `opentraderplatform-mcp-server`

    `opentraderplatform-mcp-server` exposes the platform control operations, replay-data
    discovery/download tools, chart-log creation, account and balance queries,
    position/order queries, poll-style market-data tools, and order
    placement/cancellation (including optional order-bound logs) over MCP
    stdio, then talks to the platform's local control socket internally.

5. **Run the application**:
    ```bash
    ./build/Src/OpenTraderPlatform
    ```

    Once the app is running, the local control socket listens at:
    `~/.local/state/OpenTraderPlatform/platform-control.sock`

    The control socket is intended to be consumed through `opentraderplatform-mcp-server`
    rather than a separate standalone CLI.

6. **Stage the external strategy SDK from a build tree** (optional):
   ```bash
   cmake --build build --target stage-strategy-sdk
   ```

    This creates an install-style SDK layout under `build/strategy-sdk/` with:
    - `include/OpenTraderPlatform/...` public headers
    - `lib*/libOpenTraderPlatformStrategySDK.so*` shared library artifacts
    - `share/opentraderplatform/proto/` canonical `.proto` files
    - `lib*/cmake/OpenTraderPlatformStrategySDK/` `find_package()` metadata

    The GUI strategy load dialog loads external strategy executables directly.
    Each executable is introspected via `--describe-strategy` before launch, and
    external strategies run as host-supervised child processes over the
    Unix-socket + Protobuf runtime.

### Building with Sanitizers (for development)

**UndefinedBehaviorSanitizer (UBSan)**:
```bash
cmake -Wno-deprecated -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_UBSAN=ON
cmake --build build/debug -j4
```

**AddressSanitizer (ASan)**:
```bash
cmake -Wno-deprecated -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build/debug -j4
```

See [DEVELOPMENT.md](Doc/DEVELOPMENT.md#code-quality-tools) for more details on sanitizers.

### LTTng Kernel Tracing (for thread/memory diagnostics)

LTTng captures kernel-level events (heap growth, thread scheduling) and OpenTraderPlatform userspace tracepoints with nanosecond precision. Use it to diagnose performance issues, memory growth, or unexpected thread behaviour.

**Install once:**
```bash
sudo apt install lttng-tools lttng-modules-dkms liblttng-ust-dev pkg-config babeltrace2
```

**Run the app under LTTng (via VSCode task `run-lttng`, or manually):**
```bash
# Kernel-only tracing (normal build)
.sanitizers/lttng/run-with-lttng.sh

# Kernel + UST userspace tracepoints (instrumented build)
APP=./build/LTTng/Src/OpenTraderPlatform .sanitizers/lttng/run-with-lttng.sh
```

Traces are saved to `~/.local/state/OpenTraderPlatform/lttng-traces/<timestamp>/kernel/` and `.../ust/`. No sudo password is needed — see `/etc/sudoers.d/opentraderplatform-lttng`.

**Query a trace with babeltrace2 or open in TraceCompass.** For query recipes and event reference, see the [LTTng Copilot skill](.github/skills/lttng/SKILL.md).

**VSCode tasks available:**

| Task | Description |
|------|-------------|
| `run-lttng` | Launch app under kernel-only LTTng |
| `run-lttng-instrumented` | Launch instrumented build (kernel + UST) |
| `build-with-lttng` | Build the LTTNG_ENABLED binary |

## Configuration

### Databento API Key

In the **Credentials** tab, click the **Databento** connection button and enter your API key. It is saved in the selected secure storage backend. OS Keyring is the default; YubiKey storage is optional.

### TradeStation OAuth

In the **Credentials** tab, click **Login to TradeStation** and enter your Client ID and Client Secret if needed. The app opens your system browser to complete OAuth. Credentials and tokens are saved in the selected storage backend. See [AUTHENTICATION.md](Doc/AUTHENTICATION.md).

The slider at the top of **Credentials** selects **OS Keyring** or **YubiKey** for the next launch. Restart to apply it; stores remain independent and no credentials are migrated or deleted. YubiKey mode requires one hardware touch to unlock the encrypted vault per session. Removing the key afterward does not stop trading or token refreshes. Use **Unlock / retry YubiKey** after a failed unlock.

Install `yubikey-manager` to enable the optional backend. Slot 2 must already be configured for HMAC challenge-response with touch required; the app will never program it. Reprogramming that slot or losing the matching key can make saved credentials unrecoverable. Existing files from the historical YubiKey branch are not imported.

### Logging

Qt logging categories with runtime filter controls. Logs stored at:
- Linux: `~/.local/state/OpenTraderPlatform/logs/`
- macOS: `~/Library/Application Support/OpenTraderPlatform/logs/`

## Usage

### GUI Mode

```bash
./build/Src/OpenTraderPlatform
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

## Development

### Project Structure

```
OpenTraderPlatform/
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
│   ├── Misc/              # Constants, logging, settings, secure storage
│   ├── SQL/               # Centralized SQL query headers
│   ├── Strategy/          # Strategy host orchestration and process supervision backends
│   ├── StrategyProtocol/  # Protobuf schemas for out-of-process strategy IPC
│   └── StrategySDK/       # Installable public SDK for external strategy executables
├── Strategies/            # Self-describing out-of-process strategy executable samples
├── Lib/                   # Third-party libraries (databento-cpp, QCustomPlot)
├── Rsc/                   # Icons and Qt resources
├── Stock-Lists/           # Stock-list inputs and screener recordings
└── Doc/                   # Architecture documentation
```

### Tests

`BUILD_TESTS=ON` enables authentication/storage regression tests, including hardware-independent authenticated-vault tests. Native keyring integration tests require a working unlocked keyring.

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
5. Build successfully and run the available authentication/storage regression tests
6. Open a Pull Request

## Security

- **Never commit API keys or tokens** to the repository
- Credentials use the OS keyring by default, or an optional YubiKey-unlocked AES-256-GCM vault; neither backend silently falls back to the other
- All brokerage communication over HTTPS/WSS only

## License

OpenTraderPlatform is licensed under **GPL-3.0-or-later** (GNU General Public License version 3 or, at your option, any later version). See [LICENSE](LICENSE) and [COPYING](COPYING).

Third-party components retain their own licenses; bundled QCustomPlot is GPL-3.0-or-later and databento-cpp is Apache-2.0. See [licensing and distribution notes](Doc/LICENSING.md) and [debian/copyright](debian/copyright). Distributing application binaries requires compliance with the GPL's corresponding-source obligations; an MIT-only license does not cover the combined application.

## Acknowledgments

- Built with the **Qt6** framework
- Uses **QCustomPlot** for high-performance charting
- Uses **databento-cpp** for NASDAQ TotalView market data
- Integrates with **TradeStation API** for brokerage services

## Support

For issues, questions, or feature requests, please open an issue on the GitHub repository.

---

**Note**: This is a live trading application. Always test thoroughly with paper trading accounts before using real capital.
