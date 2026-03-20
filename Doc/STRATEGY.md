# Strategy System

L2Trader strategies now run as **host-supervised external executables**. The old shared-library plugin runtime has been removed from the live load path: the host starts a child process, opens a per-strategy Unix domain socket, exchanges framed Protobuf envelopes, and keeps the rest of the platform isolated from strategy crashes and memory corruption.

For concrete examples, start with:

- `Strategies/README.md`
- `Strategies/ExampleStrategyProcess/`
- `Strategies/DumpPatternStrategy/`
- `Src/StrategySDK/Public/L2Trader/StrategySDK/ExternalStrategyRuntime.h`

## Table of Contents

1. [Architecture](#architecture)
2. [Strategy Lifecycle](#strategy-lifecycle)
3. [GUI Integration](#gui-integration)
4. [Development Guide](#development-guide)
5. [API Reference](#api-reference)
6. [Configuration](#configuration)
7. [Best Practices](#best-practices)
8. [Known Limitations](#known-limitations)

---

## Architecture

### Host-side Components

| Component | Role |
|-----------|------|
| `StrategyManager` | Owns loaded strategy instances, starts/stops them, tracks state, routes symbols/orders/positions/balance updates |
| `ProcessStrategyRuntimeBackend` | Spawns the child process, accepts the Unix socket connection, performs the handshake, and translates between Qt host objects and protocol messages |
| `StrategySDK` | Host-side per-strategy facade used by the runtime backend to place/cancel orders, query historical bars, log messages, and track strategy-local positions/orders/balance |
| `StrategyConfig` | Serialized strategy configuration: runtime path, symbols, position size, risk limit, custom params |
| `ExternalStrategyManifest` | Loads JSON manifests and optional `parameterSchema` metadata for GUI prefill/custom fields |
| `StrategyLogger` | Per-strategy log file plus in-memory circular buffer for the GUI log panel |

### Strategy-side Components

| Component | Role |
|-----------|------|
| `L2Trader::StrategySDK::ExternalStrategyHandler` | Interface implemented by the external process: lifecycle hooks plus market/brokerage callbacks |
| `L2Trader::StrategySDK::ExternalStrategyRuntime` | Connects to the host, runs the message loop, and exposes helpers such as `claimSymbols()`, `requestHistoricalBars()`, `placeOrder()`, and `log()` |
| `add_l2trader_process_strategy(...)` | CMake helper used by the bundled samples to build strategy executables into `build/<Config>/bin/` |

### Runtime Model

```text
Trade tab / StrategyLoadDialog
        |
        v
StrategyManager (MainAlgo thread)
        |
        v
ProcessStrategyRuntimeBackend
        |
        |  framed Protobuf envelopes over Unix domain socket
        v
External strategy child process
        |
        v
ExternalStrategyRuntime + ExternalStrategyHandler
```

### Protocol and Process I/O

- **Machine protocol**: framed Protobuf envelopes over a per-strategy Unix domain socket.
- **Human-readable output**: child `stdout` and `stderr` are captured by the host and appended to the per-strategy log with `[stdout]` / `[stderr]` prefixes.
- **Isolation boundary**: one child process per loaded strategy. If a strategy crashes, only that strategy instance transitions to `ERROR`.
- **Process supervision**: the host is responsible for launching, stopping, and force-cleaning the child when needed.

### Data Routed to Strategies

Once a strategy is running and has claimed symbols, the host can deliver:

- bars
- Level 2 snapshots
- trades
- order updates
- position updates
- balance updates
- heartbeat messages
- explicit host error messages

The strategy can send back:

- log messages
- chart log markers
- symbol-claim requests
- historical-bar requests
- order placement intents
- order cancellation intents

---

## Strategy Lifecycle

### States

| State | Description |
|-------|-------------|
| `LOADED` | Configuration accepted; host-side runtime objects exist; process not yet started |
| `RUNNING` | Child process connected and actively receiving data |
| `STOPPED` | Strategy was stopped cleanly or never started after load |
| `ERROR` | Process failed, handshake/protocol failed, or startup/runtime error occurred |

### Lifecycle Flow

```text
1. User loads a manifest or executable
   -> StrategyLoadDialog builds a StrategyConfig
   -> StrategyManager creates host-side SDK/logger/backend
   -> state = LOADED

2. User starts the strategy
   -> ProcessStrategyRuntimeBackend launches QProcess
   -> host opens/accepts Unix socket
   -> handshake completes
   -> host sends StrategyConfiguration
   -> state = RUNNING

3. Strategy requests capabilities
   -> claimSymbols()
   -> requestHistoricalBars()
   -> placeOrder()/cancelOrder()
   -> log()/logToChart()

4. Host streams updates back
   -> bars / Level2 / trades
   -> orders / positions / balance
   -> heartbeat / errors

5. User stops or unloads the strategy
   -> host sends stop/shutdown command
   -> symbols are released
   -> process exits (or is cleaned up)
   -> state = STOPPED or removed
```

### Crash Handling

Crash isolation is now process-based, not thread-based:

- a segmentation fault in the child does **not** corrupt host memory
- the host marks that strategy as failed and keeps the rest of the platform running
- the strategy log still captures protocol/runtime errors plus any final `stdout`/`stderr` output that was flushed before exit

---

## GUI Integration

There is **no dedicated Strategies tab anymore**.

Current strategy-management surfaces are:

- **`StrategyQuickView`** (`Src/FrontEnd/GUI/Widgets/StrategyQuickView/`)
  - embedded in the Trade tab
  - shows strategy rows and claimed symbols
  - provides Load / Start / Stop / Display Logs actions
- **`StrategyLoadDialog`** (`Src/FrontEnd/GUI/Dialogs/StrategyLoadDialog.*`)
  - loads either a manifest or an executable
  - prefills standard fields from the manifest
  - renders manifest `parameterSchema` entries as dynamic custom-parameter widgets
- **`StrategyLogWidget`**
  - displays the per-strategy log stream at the bottom of the main window

---

## Development Guide

### Creating a Process Strategy

#### 1. Project Structure

```text
Strategies/
└── MyStrategyProcess/
    ├── CMakeLists.txt
    ├── main.cpp
    └── my_strategy.json
```

#### 2. CMakeLists.txt

Inside this repository, the bundled samples use the helper from `Strategies/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyStrategyProcess VERSION 1.0.0 LANGUAGES CXX)

add_l2trader_process_strategy(MyStrategyProcess
    main.cpp
)
```

That helper builds the executable into `build/<config>/bin/`.

#### 3. Minimal Strategy Process

```cpp
#include <iostream>
#include <string>

#include "L2Trader/StrategySDK/ExternalStrategyRuntime.h"

namespace Protocol = l2trader::strategy::v1;

class MyStrategyProcess final : public L2Trader::StrategySDK::ExternalStrategyHandler
{
  public:
    std::string strategyName() const override { return "MyStrategyProcess"; }
    std::string strategyVersion() const override { return "1.0.0"; }

    void setRuntime(L2Trader::StrategySDK::ExternalStrategyRuntime* runtime)
    {
        m_runtime = runtime;
    }

    void onStart(const Protocol::StrategyConfiguration&) override
    {
        m_runtime->log("strategy started");
        const auto claims = m_runtime->claimSymbols({"AAPL"});
        if (claims.grantedSymbols.empty())
        {
            m_runtime->log("failed to claim AAPL", Protocol::LOG_LEVEL_ERROR);
        }
    }

    void onBar(const Protocol::Bar& bar) override
    {
        m_runtime->log("close=" + std::to_string(bar.close()), Protocol::LOG_LEVEL_DEBUG);
    }

    void onStop(std::string_view reason) override
    {
        m_runtime->log("stop requested: " + std::string(reason));
    }

  private:
    L2Trader::StrategySDK::ExternalStrategyRuntime* m_runtime = nullptr;
};

int main()
{
    MyStrategyProcess strategy;
    L2Trader::StrategySDK::ExternalStrategyRuntime runtime(strategy);
    strategy.setRuntime(&runtime);
    return runtime.run();
}
```

The authoritative sample is `Strategies/ExampleStrategyProcess/main.cpp`.

#### 4. Build and Stage the SDK

```bash
cmake --build build/GUI --target MyStrategyProcess
cmake --build build/GUI --target stage-strategy-sdk
```

Useful outputs:

- `build/GUI/bin/MyStrategyProcess`
- `build/GUI/bin/l2trader-mcp-server`
- `build/GUI/strategy-sdk/` — install-style SDK prefix for out-of-tree strategy builds

There is **no shared top-level `build/bin/`**. Binaries live under the active build tree such as `build/GUI/bin/` or `build/TUI/bin/`.

Bundled sample strategies also copy themselves and their manifests into:

- `~/.local/share/L2Trader/Strategies/`
- `~/.config/L2Trader/Strategies/`

---

## API Reference

### Strategy-side API (public SDK)

The external process implements `ExternalStrategyHandler` and is typically driven by `ExternalStrategyRuntime`.

#### Lifecycle callbacks

- `onStart(const Protocol::StrategyConfiguration&)`
- `onPause(std::string_view reason)`
- `onResume(std::string_view reason)`
- `onStop(std::string_view reason)`
- `onShutdown(std::string_view reason)`
- `onHeartbeat(const Protocol::Heartbeat&)`

#### Market and brokerage callbacks

- `onBar(const Protocol::Bar&)`
- `onLevel2Snapshot(const Protocol::Level2Snapshot&)`
- `onTrade(const Protocol::Trade&)`
- `onOrderUpdate(const Protocol::OrderUpdate&)`
- `onPositionUpdate(const Protocol::PositionUpdate&)`
- `onBalanceUpdate(const Protocol::BalanceUpdate&)`
- `onHostError(const Protocol::ErrorMessage&)`

#### `ExternalStrategyRuntime` helpers

- `log(...)`
- `claimSymbols(...)`
- `requestCurrentTimeUnixNanos()`
- `logToChart(...)`
- `requestHistoricalBars(...)`
- `placeOrder(...)`
- `cancelOrder(...)`
- `startTimer(...)` / `cancelTimer(...)`
- accessors for current configuration, cash balance, orders, and positions

### Host-side Runtime (internal)

These classes are useful when editing the platform, not when authoring external strategies:

- `StrategyManager`
- `ProcessStrategyRuntimeBackend`
- `StrategySDK` (host-side Qt object)
- `StrategyLogger`
- `ExternalStrategyManifest`

---

## Configuration

### Strategy Manifest File

Strategies are commonly loaded from JSON manifests in `~/.config/L2Trader/Strategies/`.

```json
{
  "name": "Example Strategy Process",
  "runtimeType": "external-process",
  "executablePath": "~/.local/share/L2Trader/Strategies/ExampleStrategyProcess",
  "symbols": ["AAPL"],
  "positionSize": 1,
  "riskLimit": 100.0,
  "customParams": {}
}
```

Optional `parameterSchema` metadata can be added so the GUI can render extra fields in `StrategyLoadDialog`.

### Loading Strategies in the GUI

1. Open the **Trade** tab.
2. Use the **Load** button in `StrategyQuickView`.
3. Select either:
   - a manifest (`*.json`), or
   - a strategy executable directly.
4. Review/edit standard fields and any manifest-defined custom parameters.
5. Start the strategy from the context menu.

If you load the executable directly and a matching manifest exists, the dialog attempts to find that manifest and apply its defaults.

---

## Best Practices

1. **Claim symbols before trading or chart logging**.
   The host expects strategies to establish symbol ownership first.

2. **Read configuration from `onStart(...)` rather than hardcoding everything**.
   The protobuf `StrategyConfiguration` contains symbols plus custom params.

3. **Keep the machine protocol on the Unix socket**.
   `stdout`/`stderr` are captured as human-readable log lines, not as a transport.

4. **Use replay mode and the sample strategies for validation**.
   `HistoricalBarsStrategy`, `OrderTestStrategy`, and `DumpPatternStrategy` cover different slices of the runtime.

5. **Treat order/position updates as the source of truth**.
   A request ID confirms that an intent was sent; the actual state arrives later through update callbacks.

6. **Log deliberately**.
   Strategy logs are persisted and surfaced in the GUI, so concise structured messages are easier to debug than noisy per-tick spam.

---

## Known Limitations

- The **official SDK is C++ today**. The wire protocol is process-based and language-neutral, but non-C++ strategies currently need to implement the protocol themselves.
- Strategies must currently run as **local executables** supervised by the host.
- The removed shared-library runtime is still represented in some compatibility structures (`StrategyRuntimeType::PluginSharedLibrary`), but live manifest loading rejects it.
- Strategy lifecycle control already exists in the GUI/runtime, but not every lifecycle surface is exported over MCP yet.

---

## Reference Map

| Path | Why it matters |
|------|----------------|
| `Src/Strategy/StrategyManager.*` | Host-side lifecycle orchestration |
| `Src/Strategy/ProcessStrategyRuntimeBackend.*` | Process launch, socket handshake, protocol bridge |
| `Src/Strategy/StrategyConfig.*` | Stored runtime configuration |
| `Src/Strategy/ExternalStrategyManifest.*` | Manifest parsing and manifest/executable matching |
| `Src/Strategy/StrategyLogger.*` | Per-strategy log persistence |
| `Src/StrategySDK/Public/L2Trader/StrategySDK/ExternalStrategyRuntime.h` | Strategy-author-facing runtime API |
| `Strategies/README.md` | Sample build/install workflow |
| `Strategies/ExampleStrategyProcess/` | Minimal working sample |
| `Strategies/DumpPatternStrategy/` | Rich sample with timers, current-time queries, chart logs, and orders |
