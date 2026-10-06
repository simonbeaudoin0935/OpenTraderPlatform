# Strategy System

OpenTraderPlatform strategies now run as **host-supervised external executables**. The old shared-library plugin runtime has been removed from the live load path: the host starts a child process, opens a per-strategy Unix domain socket, exchanges framed Protobuf envelopes, and keeps the rest of the platform isolated from strategy crashes and memory corruption.

For concrete examples, start with:

- `Strategies/README.md`
- `Strategies/Examples/ExampleStrategyProcess/`
- `Strategies/Examples/HistoricalBarsStrategy/`
- `Src/StrategySDK/Public/OpenTraderPlatform/StrategySDK/ExternalStrategyRuntime.h`

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
| `StrategySDK` | Host-side per-strategy facade used by the runtime backend to place/cancel orders, query historical bars, emit chart logs/status and bracket overlays, and track strategy-local positions/orders/balance |
| `StrategyConfig` | Serialized strategy configuration: runtime path, introspected strategy name/version, and opaque `fieldValues` |
| `ExternalStrategyDescription` | Runs `<executable> --describe-strategy`, parses the returned schema, and validates stored field values |
| `StrategyLogger` | Per-strategy log file plus in-memory circular buffer for the GUI log panel |

### Strategy-side Components

| Component | Role |
|-----------|------|
| `OpenTraderPlatform::StrategySDK::ExternalStrategyHandler` | Interface implemented by the external process: lifecycle hooks plus market/brokerage callbacks |
| `OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime` | Connects to the host, runs the message loop, and exposes helpers such as `claimSymbols()`, `requestHistoricalBars()`, `placeOrder()`, `log()`, `logToChart()`, `setChartStatus()/clearChartStatus()`, and managed-bracket intents |
| `add_opentraderplatform_process_strategy(...)` | CMake helper used by the bundled samples to build strategy executables into `build/<Config>/bin/` |

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
- **Frame limit**: each payload is limited to 16 MiB (excluding the four-byte
  length prefix). Host handshake/runtime readers and SDK readers reject larger
  advertised lengths before allocating the payload; serialization enforces the
  same limit. An oversized inbound frame is a fatal protocol error, not a frame
  to skip and resume after.
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
- chart status updates (top-left rich-text status zone per symbol)
- chart bracket-overlay intents (upsert/clear logical stop/take bands)
- symbol-claim requests
- historical-bar requests
- order placement intents
- order cancellation intents

Chart status updates are live-session state (in-memory), unlike chart log markers which are persisted.

---

## Strategy Lifecycle

### States

| State | Description |
|-------|-------------|
| `LOADED` | Configuration accepted; host-side runtime objects exist; process not yet started |
| `PRIMED` | Replay-specific state: strategy is armed for the next first-Play start, but the child process has not been launched yet |
| `RUNNING` | Child process connected and actively executing while live/sim or replay playback is advancing |
| `PAUSED` | Replay-specific state: child process is started, but replay is paused so strategy logic is suspended |
| `STOPPED` | Strategy was stopped cleanly or failed after startup |
| `ERROR` | Process failed, handshake/protocol failed, or startup/runtime error occurred |

### Lifecycle Flow

```text
1. User loads an executable
   -> StrategyLoadDialog runs `--describe-strategy`
   -> StrategyLoadDialog builds a StrategyConfig from the returned metadata + field values
   -> StrategyManager creates host-side SDK/logger/backend
   -> state = LOADED

2. User starts the strategy
    -> ProcessStrategyRuntimeBackend launches QProcess
    -> host opens/accepts Unix socket
    -> handshake completes
    -> host sends StrategyConfiguration
    -> strategy completes onStart/setup
    -> strategy sends explicit ready acknowledgement
    -> state = RUNNING (or PAUSED if replay is currently paused)

3. Strategy requests capabilities
   -> claimSymbols()
   -> requestHistoricalBars()
   -> placeOrder()/cancelOrder()
   -> log()/logToChart()
   -> upsertBracketOverlay()/clearBracketOverlay()

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

### Replay-specific lifecycle nuance

- **Preloaded replay is visible but non-tradable.** Before the first Play press, replay strategies do not start executing yet and replay order placement is rejected.
- **Manual Start during replay preload primes the strategy** instead of launching it immediately. The child process starts only when the user presses Play the first time.
- **First Play is gated.** The host starts all primed replay strategies, waits for each one to finish `onStart()` and send the ready acknowledgement, and only then resumes replay playback.
- **Strategy-local replay inputs must follow host replay clock.** A strategy may ingest local replay artifacts but must still key processing from host `currentReplayTime`.
- **Keep `onStart()` cheap.** The ready acknowledgement should mean the strategy is safely initialized and can receive replay updates, not that all optional warm-up work has completed. Large background tasks such as seeding hundreds of symbols from historical bars should continue after startup so replay does not stall on cold caches.
- **If startup fails or times out, replay stays paused.** The user must resolve the strategy problem and retry.
- **After replay has started once**, replay pause/resume maps to strategy pause/resume.
- **Manual order confirmations also pause with replay.** If a strategy order is awaiting user confirmation, its timeout
  countdown freezes while replay is paused and resumes from the remaining time when replay resumes.
- **Replay restart/fresh-session reset** re-primes previously active replay strategies so the next Play is again a fresh gated start.
- **Rewind determinism is required.** If replay time rewinds, strategy-local replay inputs should reset internal extraction state and reprocess from the rewound replay cursor.

### Strategy manual-confirm UX and risk/mute behavior

When strategies submit user-confirm orders:

- the GUI can render a preview bracket before acceptance when stop metadata is provided;
- user adjustments to the preview stop are forwarded on `Y` accept;
- final acceptance still routes through `MainAlgo` risk checks (including planned-loss-per-trade limits);
- `Shift+N` reject keeps the symbol blocked for future confirmations;
- global `M` hard-mute rejects active + queued confirmations and auto-rejects future ones until unmuted.

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
  - loads an executable path
  - introspects it via `--describe-strategy`
  - renders returned `parameterSchema` entries as dynamic configuration widgets
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
    └── main.cpp
```

#### 2. CMakeLists.txt

Inside this repository, the bundled samples use the helper from `Strategies/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyStrategyProcess VERSION 1.0.0 LANGUAGES CXX)

add_opentraderplatform_process_strategy(MyStrategyProcess
    main.cpp
)
```

That helper builds the executable into `build/<config>/bin/`.

Normal full builds also stage every configured strategy executable linked to the
SDK into `~/.local/share/OpenTraderPlatform/Strategies`. Staging runs even when
the executable is already up to date, using `copy_if_different` to restore missing
or stale deployed copies without rewriting unchanged files. Existing explicit
calls to `stage_opentraderplatform_process_strategy` remain supported.

The default VS Code `build` / `build-and-run` workflow enables
`BUILD_PRIVATE_STRATEGIES=ON` and requires the `Strategies/Privates` checkout.
Other configurations can keep private strategies disabled. Only strategies whose
source is included in the configured CMake project can be rebuilt and staged;
old executables left in the deployment directory are not automatically rebuilt
or deleted. A targeted build of only the platform is not a full strategy build.
Restart a loaded strategy to use its updated executable; stop strategies before
building to avoid replacing an executable that is still running.

#### 3. Minimal Strategy Process

```cpp
#include <iostream>
#include <string>

#include "OpenTraderPlatform/StrategySDK/StrategyDescription.h"
#include "OpenTraderPlatform/StrategySDK/ExternalStrategyRuntime.h"
#include "OpenTraderPlatform/StrategySDK/StrategyProcessMain.h"

namespace Protocol = opentraderplatform::strategy::v1;

class MyStrategyProcess final : public OpenTraderPlatform::StrategySDK::ExternalStrategyHandler
{
  public:
    void bindRuntime(OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime* runtime) override
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
    OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime* m_runtime = nullptr;
};

static OpenTraderPlatform::StrategySDK::StrategyDescription describeStrategy()
{
    return {
        .name = "MyStrategyProcess",
        .version = "1.0.0",
        .parameterSchema = {
            OpenTraderPlatform::StrategySDK::stringField("symbol", "Symbol", "AAPL"),
        },
    };
}

int main(int argc, char** argv)
{
    MyStrategyProcess strategy;
    return OpenTraderPlatform::StrategySDK::runStrategyProcessMain(argc, argv, describeStrategy(), strategy);
}
```

The authoritative sample is `Strategies/Examples/ExampleStrategyProcess/main.cpp`.

#### 4. Build and Stage the SDK

```bash
cmake --build build --target MyStrategyProcess
cmake --build build --target stage-strategy-sdk
```

Useful outputs:

- `build/bin/MyStrategyProcess`
- `build/bin/opentraderplatform-mcp-server`
- `build/strategy-sdk/` — install-style SDK prefix for out-of-tree strategy builds

Runtime binaries are emitted under `build/bin/`.

Bundled sample strategies also copy themselves into:

- `~/.local/share/OpenTraderPlatform/Strategies/`

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

#### Live subscription failures

A successful subscription/claim registers the symbol with the host; it does not
guarantee that the broker has accepted every feed. Later bars, depth, or quote
failures are delivered asynchronously to each strategy monitoring the symbol
through `onHostError`. The host also writes the failure into that strategy's log.

Subscription errors use code `market_data_subscription_rejected` for terminal
errors or `market_data_subscription_retrying` for transient errors. The message
includes the symbol, feed, broker reason and description. SDKs built with the
updated protocol can additionally inspect `error.has_subscription_error()` and
`error.subscription_error()` for `symbol`, `feed` (`bars`, `depth`, `quotes`),
`reason`, `terminal`, and `retry_delay_ms` (zero when terminal).

For example, an invalid symbol produces a terminal `BadRequest` error; the host
stops retrying that feed. The strategy decides whether to stop, alert, or select
another symbol. Other feeds or strategies are not automatically stopped.
The broker's `InvalidSymbol` and `FAILED, INVALID SYMBOL` feed errors are also
classified as terminal `BadRequest` errors.
An already-failed subscription's last error is also sent when another strategy
attaches to that symbol context; receiving valid data clears the cached error.

Selecting a rejected symbol in a chart stops historical requests for that symbol
and displays an invalid-symbol status on the chart. Main and detached charts use
the same policy: terminal rejection is reset when selecting a symbol again or
switching between live and replay;
transient historical errors retry with 1-to-30-second exponential backoff. Errors
are not treated as empty trading days; genuinely empty responses remain eligible
for holiday/non-trading-day backfill without repeatedly fetching the empty day.

This is an additive field on the existing error envelope, so older strategy
binaries still receive the code and descriptive message through `onHostError`.
No new handler virtual method or protocol-major change is required. Rebuild
against the updated SDK to access structured subscription details.

#### Failing a strategy intentionally

Call `runtime.fail(reason)` from a runtime callback for an unrecoverable strategy
condition. This sends a structured failure to the host, disables timers and
further outgoing intents, and makes `run()` return nonzero without deliberately
crashing. `runtime.hasFailed()` lets startup code stop after a nested helper such
as `claimSymbols()` dispatches an error callback. The first failure reason wins;
startup never sends a ready acknowledgement after failure. After callbacks unwind,
the runtime calls `onStop(reason)` for local cleanup before returning.

For a strategy that requires one configured symbol:

```cpp
void onHostError(const Protocol::ErrorMessage& error) override
{
    if (error.has_subscription_error() &&
        error.subscription_error().terminal() &&
        error.subscription_error().symbol() == m_symbol)
    {
        m_runtime->fail("Required symbol " + m_symbol + ": " + error.message());
        return;
    }
    // Log transient or unrelated failures; the host handles transient retries.
}
```

Return promptly after calling `fail`; it does not forcibly unwind strategy
callback code. Use this policy only for feeds/symbols the strategy requires.
A multi-symbol scanner may choose to discard a rejected symbol instead.
The host preserves the reason in the Strategy Failed dialog and strategy log,
disconnects data callbacks, releases claims/references and pending confirmations,
and keeps the configuration loaded for correction and restart. Existing broker
orders/positions are not automatically cancelled or flattened.
Rebuild strategy executables against the updated SDK before using this API.
The new runtime state and generated protobuf layout require SDK ABI version 2
(`libOpenTraderPlatformStrategySDK.so.2`). The wire protocol remains compatible,
but old binaries must not load the new library under the previous `.so.1` name.
Existing SDK-1 binaries require their matching SDK-1 library until rebuilt.

#### `ExternalStrategyRuntime` helpers

- `log(...)`
- `claimSymbols(...)`
- `requestCurrentTimeUnixNanos()`
- `logToChart(...)`
- `setChartStatus(...)` / `clearChartStatus(...)`
- `requestHistoricalBars(...)`
- `placeOrder(...)`
- `cancelOrder(...)`
- `startTimer(...)` / `cancelTimer(...)`
- accessors for current configuration, cash balance, orders, and positions

`setChartStatus(...)` accepts message-only payloads; the GUI status panel supports rich text (HTML), so strategies can style subparts of the message (for example, coloring only pass/fail tokens). The status is rendered as a chart overlay in the top-left status zone.

### Host-side Runtime (internal)

These classes are useful when editing the platform, not when authoring external strategies:

- `StrategyManager`
- `ProcessStrategyRuntimeBackend`
- `StrategySDK` (host-side Qt object)
- `StrategyLogger`
- `ExternalStrategyDescription`

---

## Configuration

### Strategy Self-Description

Strategies are loaded by executable path. Before launch, the GUI runs:

```bash
<strategy-executable> --describe-strategy
```

The executable must print JSON like:

```json
{
  "name": "ExampleStrategyProcess",
  "version": "1.0.0",
  "parameterSchema": [
    {
      "key": "symbol",
      "type": "string",
      "label": "Symbol",
      "default": "AAPL"
    }
  ]
}
```

The host stores the chosen executable path plus user-provided field values keyed by schema field ID.

### Loading Strategies in the GUI

1. Open the **Trade** tab.
2. Use the **Load** button in `StrategyQuickView`.
3. Select a strategy executable.
4. Review/edit the schema-driven fields returned by `--describe-strategy`.
5. Start the strategy from the context menu.

---

## Best Practices

1. **Claim symbols before trading or chart logging/status/bracket overlays**.
   The host expects strategies to establish symbol ownership first.

2. **Read configuration from `onStart(...)` rather than hardcoding everything**.
   The protobuf `StrategyConfiguration` carries the configured field values in `custom_params`; market symbols should be claimed explicitly via `claimSymbols(...)`.

3. **Keep the machine protocol on the Unix socket**.
   `stdout`/`stderr` are captured as human-readable log lines, not as a transport.

4. **Use replay mode and the public examples for validation**.
  `HistoricalBarsStrategy` demonstrates host-provided historical data. The order-driving harness under `Strategies/Tests/` is for controlled test accounts only.

5. **Treat order/position updates as the source of truth**.
   A request ID confirms that an intent was sent; the actual state arrives later through update callbacks.

6. **Log deliberately**.
   Strategy logs are persisted and surfaced in the GUI, so concise structured messages are easier to debug than noisy per-tick spam.

---

## Known Limitations

- The **official SDK is C++ today**. The wire protocol is process-based and language-neutral, but non-C++ strategies currently need to implement the protocol themselves.
- Strategies must currently run as **local executables** supervised by the host.
- The removed shared-library runtime is still represented in some compatibility structures (`StrategyRuntimeType::PluginSharedLibrary`) only so old saved configs can fail clearly.
- Strategy lifecycle control already exists in the GUI/runtime, but not every lifecycle surface is exported over MCP yet.

---

## Reference Map

| Path | Why it matters |
|------|----------------|
| `Src/Strategy/StrategyManager.*` | Host-side lifecycle orchestration |
| `Src/Strategy/ProcessStrategyRuntimeBackend.*` | Process launch, socket handshake, protocol bridge |
| `Src/Strategy/StrategyConfig.*` | Stored runtime configuration |
| `Src/Strategy/ExternalStrategyDescription.*` | Executable introspection and schema validation |
| `Src/Strategy/StrategyLogger.*` | Per-strategy log persistence |
| `Src/StrategySDK/Public/OpenTraderPlatform/StrategySDK/ExternalStrategyRuntime.h` | Strategy-author-facing runtime API |
| `Strategies/README.md` | Sample build/install workflow |
| `Strategies/Examples/ExampleStrategyProcess/` | Minimal working sample |
| `Strategies/Examples/HistoricalBarsStrategy/` | Historical-bars API sample |
| `Strategies/Tests/OrderTestStrategy/` | Order-driving test harness; never use with a live account |
