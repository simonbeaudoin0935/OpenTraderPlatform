# Strategy/ Directory - External Strategy Runtime - Agent Instructions

The `Src/Strategy/` directory contains the **host-side external strategy runtime**. Strategies are no longer loaded as in-process shared libraries; each strategy is a supervised child process that speaks the framed-Protobuf strategy protocol over a Unix domain socket.

## Overview

**Location**: `Src/Strategy/`

**Purpose**:
- load strategy executables
- supervise one child process per strategy
- bridge host market/brokerage state into the child process
- route strategy intents (logs, managed brackets, symbol claims, historical requests, orders) back into `MainAlgo`

**Primary thread context**:
- `StrategyManager` lives on the **MainAlgo thread**
- each strategy runs as its own **external process**
- `QProcess` supervision and protocol handling happen on the host side through `ProcessStrategyRuntimeBackend`

## Key Files

| File | Role |
|------|------|
| `StrategyManager.h/.cpp` | Main orchestration class for load/start/stop/unload, symbol claim routing, per-strategy state, and UI-facing signals |
| `ProcessStrategyRuntimeBackend.h/.cpp` | Launches the child process, owns the Unix socket handshake, translates between protobuf envelopes and Qt host objects |
| `StrategySDK.h` | Host-side per-strategy SDK used internally by the runtime backend and `MainAlgo` integration |
| `StrategyConfig.h/.cpp` | Serializable runtime configuration (`runtimeType`, executable path, introspected name/version, opaque `fieldValues`) |
| `ExternalStrategyDescription.h/.cpp` | Runs `<executable> --describe-strategy`, validates returned schema, and validates stored field values |
| `StrategyLogger.h/.cpp` | Per-strategy disk + in-memory logging |
| `StrategyOrderValidator.h/.cpp` | Order-side validation shared by strategy order entry paths |
| `StrategyRuntimeBackend.h` | Runtime backend interface used by `StrategyManager` |

## Strategy Runtime Model

```text
StrategyQuickView / StrategyLoadDialog
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
```

Important details:

- **One process per strategy**. A strategy failure should not corrupt host memory.
- **Machine protocol is socket-based**. Do not reintroduce `stdin`/`stdout` as the primary transport.
- **Child `stdout`/`stderr` are still useful**. The backend captures both streams and appends them into `StrategyLogger` with `[stdout]` / `[stderr]` prefixes.
- **The old plugin runtime is removed from live use**. `StrategyRuntimeType::PluginSharedLibrary` still exists only as a compatibility enum value for old saved configs.

## StrategyManager Responsibilities

`StrategyManager` is the control point for strategy lifecycle.

Core responsibilities:

- accept a `StrategyConfig` and create a strategy instance ID
- create `StrategySDK`, `StrategyLogger`, and runtime backend objects
- start and stop strategies
- release symbol claims on stop/unload
- route order, position, and balance updates back to the owning strategy
- surface status/logging signals to the GUI (`StrategyQuickView`, `StrategyLogWidget`)

Replay-specific lifecycle responsibilities:

- keep replay-preloaded strategies **primed** without launching them yet
- gate the **first Play** on strategy startup readiness
- propagate replay `pause` / `resume` into already-started strategies
- fail clearly if a strategy binary does not support the current ready-ack protocol

Useful public entry points:

- `loadStrategy(...)`
- `startStrategy(...)`
- `unloadStrategy(...)`
- `stopAllStrategies()`
- `getActiveStrategies()`
- `getStrategyConfig(...)`
- `getStrategyRecentOrders(...)`
- `getStrategyOpenPositions(...)`

## ProcessStrategyRuntimeBackend Responsibilities

This class owns the child-process bridge.

It is responsible for:

- opening the listening Unix socket
- starting the `QProcess`
- accepting the runtime connection and completing the handshake
- sending the initial `StrategyConfiguration`
- publishing bars, Level 2, trades, orders, positions, and balances to the child
- consuming child requests such as claim-symbols, historical bars, chart logs, chart status updates/clears
  (message-only, rich-text capable, rendered in chart top-left overlay),
  managed-bracket upsert/cancel, place-order, cancel-order
- capturing `stdout` / `stderr`
- turning process/socket failures into `StrategyManager::markStrategyFailed(...)`. `reportFailure(...)` appends `buildFailureDiagnostics()`: the last strategy ERROR log, or else the recent `stderr` tail, if received within 10 s. The Strategy Failed dialog then shows the real cause instead of only "socket disconnected" / "exit code N".

When editing runtime behavior, start here:

- `ensureProcessStarted()`
- `acceptClientConnection()`
- `completeHandshake()`
- `handleInboundPayload(...)`
- `sendHostEnvelope(...)`
- `captureProcessStream(...)`

### Replay startup contract

The current replay contract is intentionally stricter than live/sim:

- replay preload is **not tradable**
- before the first Play press, a replay strategy can be **loaded/primed** but not yet launched
- the host starts primed strategies on first Play, waits for each child to finish `onStart()` and send the explicit **strategy-ready** acknowledgement, and only then lets replay advance
- later replay pauses/resumes use the existing pause/resume lifecycle messages to suspend/resume strategy logic
- strategies should treat **ready** as **lifecycle-safe to run**, not **every optional warm-up task is finished**; expensive work such as bulk historical seeding for hundreds of symbols should continue after startup instead of blocking the ready ack path

### Strategy user-confirm orders and mute interaction

Strategy order intents can target manual confirmation (`UserConfirm`) mode. Current host behavior:

- UI can present a preview bracket before user confirmation when a stop is supplied.
- User-edited preview stop is forwarded on acceptance (`Y`) and revalidated by the risk gate.
- `N` rejects the current confirmation; `Shift+N` also blocklists the symbol.
- Global mute shortcut `M` enables hard mute:
  - active confirmation is rejected immediately
  - queued confirmations are drained as rejected
  - future strategy confirmations are auto-rejected until unmuted

## Host-side StrategySDK

`Src/Strategy/StrategySDK.h` is the **host-side** per-strategy facade.

It is not the same thing as the installable strategy-author SDK in:

- `Src/StrategySDK/Public/OpenTraderPlatform/StrategySDK/ExternalStrategyRuntime.h`

The host-side `StrategySDK` is responsible for:

- order placement/cancellation via `MainAlgo`
- claimed-symbol tracking
- historical-bar access
- current-time access
- chart logging
- chart status updates (live-session state; message-only and rich-text capable)
- managed-bracket upsert/cancel forwarding
- per-strategy cached orders / positions / balance

When documenting or editing strategy author workflows, prefer the public SDK in `Src/StrategySDK/Public/...`.

## Executable Description and Configuration Rules

Strategies are now loaded by executable path only.

Current expectations:

- `runtimeType` should be `external-process`
- `executablePath` points at the child executable
- the executable must implement a fast, side-effect-free `--describe-strategy` mode
- `StrategyLoadDialog` runs `--describe-strategy` and renders the returned `parameterSchema`
- `StrategyConfig` stores opaque `fieldValues` keyed by schema field ID, plus the last introspected `name` / `version`

Compatibility note:

- `StrategyConfig` can still deserialize old manifest-era fields into `fieldValues`
- `StrategyConfig` can still deserialize the removed shared-library runtime enum value so old saved configs fail clearly instead of crashing

## GUI Surfaces Connected to This Directory

There is no `StrategiesTab/` anymore.

The current GUI surfaces are:

- `Src/FrontEnd/GUI/Widgets/StrategyQuickView/` — load/start/stop/display-logs actions and claimed-symbol view
- `Src/FrontEnd/GUI/Dialogs/StrategyLoadDialog.*` — executable selection and schema-driven parameter editing
- `StrategyLogWidget` — per-strategy log viewer in the bottom splitter area

## Common Editing Workflows

### Adding a new host-to-strategy capability

Touch all relevant layers together:

1. strategy protocol (`Src/StrategyProtocol/...` or generated protobufs)
2. strategy-side SDK/runtime if external strategies should consume it
3. `ProcessStrategyRuntimeBackend` envelope send/receive logic
4. `StrategyManager` / `MainAlgo` integration
5. docs (`Doc/STRATEGY.md`, `Doc/STRATEGY_GUIDE.md`, `Strategies/README.md` if user-visible)

### Adding a new strategy configuration field

Update:

1. `StrategyConfig`
2. the strategy's `StrategyDescription`
3. `StrategyLoadDialog`
4. any persistence/restore path in `StrategyManager`
5. docs

### Debugging a failing strategy

Check, in order:

1. strategy log file in `~/.local/state/OpenTraderPlatform/StrategiesLogs/`
2. host app log in `~/.local/state/OpenTraderPlatform/AppLogs/`
3. `stdout`/`stderr` lines captured by `captureProcessStream(...)`
4. handshake/runtime failure paths in `ProcessStrategyRuntimeBackend`

## Common Pitfalls

- **Do not reintroduce `QLibrary`/factory-function plugin loading.** That model is obsolete for the live runtime.
- **Do not document `Src/Strategy/StrategySDK.h` as the strategy-author API.** It is host-side.
- **Do not assume symbol data arrives before a claim.** Claim approval is the authority boundary.
- **Do not treat request submission as final state.** Order status is confirmed later through order-update messages.
- Runtime binaries for bundled strategy executables land in `build/bin/`.

## Related Documentation

- `Doc/STRATEGY.md` — current technical overview of the strategy runtime
- `Doc/STRATEGY_GUIDE.md` — beginner-oriented strategy author guide
- `Strategies/README.md` — bundled strategy samples and build workflow
- `Src/Algo/AGENTS.md` — `MainAlgo` integration and symbol routing
- `Src/FrontEnd/GUI/AGENTS.md` — GUI surfaces used for strategy management
