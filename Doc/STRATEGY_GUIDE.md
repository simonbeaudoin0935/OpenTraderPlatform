# OpenTraderPlatform — Strategy Developer Guide

**Who this document is for**: people who want to write a strategy for OpenTraderPlatform without having to learn the full host-side architecture first.

The most important change to know is this:

> OpenTraderPlatform strategies are no longer in-process `.so` plugins.
>
> A strategy is now a **separate executable** that the platform launches and supervises.

That means a crashing strategy usually takes down **only itself**, not the whole trading platform.

---

## Table of Contents

1. [What Is OpenTraderPlatform?](#what-is-opentraderplatform)
2. [What a Strategy Is](#what-a-strategy-is)
3. [How a Strategy Talks to the App](#how-a-strategy-talks-to-the-app)
4. [Writing Your First Strategy](#writing-your-first-strategy)
5. [Building and Loading It](#building-and-loading-it)
6. [Testing with Replay Mode](#testing-with-replay-mode)
7. [Frequently Asked Questions](#frequently-asked-questions)

---

## What Is OpenTraderPlatform?

OpenTraderPlatform is a desktop trading application that combines:

- **Databento** for market data (bars, trades, Level 2)
- **TradeStation** for brokerage (orders, positions, balances)
- **Replay mode** for testing with historical data and local order emulation

The host application owns the UI, the broker connections, replay control, persistence, and safety checks.

Your strategy only focuses on trading logic.

---

## What a Strategy Is

A strategy is a **child process** launched by OpenTraderPlatform.

The host sends it:

- lifecycle events (`onStart`, `onStop`, `onShutdown`)
- market data (`onBar`, `onLevel2Snapshot`, `onTrade`)
- brokerage state (`onOrderUpdate`, `onPositionUpdate`, `onBalanceUpdate`)

Your strategy sends back:

- log messages
- symbol-claim requests
- historical-bar requests
- order placement/cancellation requests
- chart log markers
- chart bracket-overlay intents (logical stop/take lines)

This split is the reason the platform is much safer than the old plugin model.

In replay, there is one extra lifecycle nuance to understand:

- a preloaded replay session is **visible but not tradable**
- before the first Play press, a strategy may be restored/primed in the UI without its child process starting yet
- after replay has started once, replay pause/resume maps to your `onPause(...)` / `onResume(...)` callbacks

---

## How a Strategy Talks to the App

You usually do **not** work with raw sockets or Protobuf directly.

Instead, you implement `ExternalStrategyHandler` and let `ExternalStrategyRuntime` do the connection work.

### The two main SDK classes

- `OpenTraderPlatform::StrategySDK::ExternalStrategyHandler`
  - you subclass this
  - you override methods like `onStart(...)`, `onBar(...)`, and `onStop(...)`
- `OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime`
  - owns the runtime loop
  - connects to the host
  - gives you helper methods like:
    - `log(...)`
    - `claimSymbols(...)`
    - `requestHistoricalBars(...)`
    - `placeOrder(...)`
    - `cancelOrder(...)`
    - `upsertBracketOverlay(...)` / `clearBracketOverlay(...)`
    - `requestCurrentTimeUnixNanos()`

### Important rule

Before you expect market data or trading authority for a symbol, call `claimSymbols(...)`.

---

## Writing Your First Strategy

The simplest good starting point is `Strategies/Examples/ExampleStrategyProcess/main.cpp`.

Here is the basic pattern:

```cpp
#include <string>

#include "OpenTraderPlatform/StrategySDK/ExternalStrategyRuntime.h"

namespace Protocol = opentraderplatform::strategy::v1;

class MyStrategyProcess final : public OpenTraderPlatform::StrategySDK::ExternalStrategyHandler
{
  public:
    std::string strategyName() const override { return "MyStrategyProcess"; }
    std::string strategyVersion() const override { return "1.0.0"; }

    void setRuntime(OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime* runtime)
    {
        m_runtime = runtime;
    }

    void onStart(const Protocol::StrategyConfiguration&) override
    {
        m_runtime->log("Strategy started");

        const auto claims = m_runtime->claimSymbols({"AAPL"});
        if (claims.grantedSymbols.empty())
        {
            m_runtime->log("Could not claim AAPL", Protocol::LOG_LEVEL_ERROR);
        }
    }

    void onBar(const Protocol::Bar& bar) override
    {
        m_runtime->log("close=" + std::to_string(bar.close()), Protocol::LOG_LEVEL_DEBUG);
    }

    void onStop(std::string_view reason) override
    {
        m_runtime->log("Stop requested: " + std::string(reason));
    }

  private:
    OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime* m_runtime = nullptr;
};

int main()
{
    MyStrategyProcess strategy;
    OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime runtime(strategy);
    strategy.setRuntime(&runtime);
    return runtime.run();
}
```

### What this example does

- announces a strategy name/version
- connects to the host runtime
- claims `AAPL` when it starts
- logs each bar close it receives
- logs the stop reason when the host shuts it down

### What callbacks are available?

You can override any of these as needed:

- `onStart(...)`
- `onPause(...)`
- `onResume(...)`
- `onStop(...)`
- `onShutdown(...)`
- `onHeartbeat(...)`
- `onBar(...)`
- `onLevel2Snapshot(...)`
- `onTrade(...)`
- `onOrderUpdate(...)`
- `onPositionUpdate(...)`
- `onBalanceUpdate(...)`
- `onHostError(...)`

### Requesting historical bars

`ExternalStrategyRuntime` can synchronously request historical bars from the host:

```cpp
const auto result = m_runtime->requestHistoricalBars(
    "AAPL",
    sessionDayUnixNanos,
    firstBarUnixNanos,
    lastBarUnixNanos,
    "1m");

if (!result.hasError())
{
    m_runtime->log("received " + std::to_string(result.bars.size()) + " bars");
}
```

If you want a real working example, use `Strategies/Examples/HistoricalBarsStrategy/`.

### Placing orders

Order placement is available through the external runtime.

```cpp
const std::string requestId = m_runtime->placeOrder(
    "AAPL",
    "SIM123456",
    Protocol::ORDER_SIDE_BUY,
    Protocol::ORDER_TYPE_MARKET,
    100);
```

Important idea: the returned string tells you the request was sent. The actual order state arrives later via `onOrderUpdate(...)`.

To cancel:

```cpp
m_runtime->cancelOrder(orderId);
```

The order-driving test harness is under `Strategies/Tests/OrderTestStrategy/`.
It submits orders using its configured account; use a test account only.

### Logging

Use:

```cpp
m_runtime->log("message");
m_runtime->log("warning message", Protocol::LOG_LEVEL_WARNING);
```

You can also emit chart markers:

```cpp
m_runtime->logToChart("AAPL", "entry setup detected");
```

And set/clear a per-symbol chart status message (bottom-right status zone):

```cpp
m_runtime->setChartStatus(
    "AAPL",
    "Pillars 3/5 | RVOL:<span style='color:#7dff9d'>Y</span> "
    "GAIN:<span style='color:#ff8a8a'>N</span>");
// ...
m_runtime->clearChartStatus("AAPL");
```

You can also publish logical bracket overlays to the chart:

```cpp
using OpenTraderPlatform::StrategySDK::BracketOverlaySide;

m_runtime->upsertBracketOverlay(
    "AAPL",
    BracketOverlaySide::Long,
    9.85,   // stop
    10.55); // take

// Later, when the logical bracket is no longer active:
m_runtime->clearBracketOverlay("AAPL");
```

---

## Building and Loading It

### CMake

Inside this repository, use the helper from `Strategies/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyStrategyProcess VERSION 1.0.0 LANGUAGES CXX)

add_opentraderplatform_process_strategy(MyStrategyProcess
    main.cpp
)
```

### Self-description

A strategy executable now describes itself with `--describe-strategy`:

```json
{
  "name": "MyStrategyProcess",
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

The GUI loads the executable directly, runs `--describe-strategy`, and builds the configuration form from the returned schema.

### Build commands

From the repository root:

```bash
cmake --build build --target MyStrategyProcess
cmake --build build --target stage-strategy-sdk
```

Outputs go to the active build tree, for example:

- `build/bin/MyStrategyProcess`
- `build/bin/opentraderplatform-mcp-server`
- `build/strategy-sdk/`

Runtime binaries are emitted under `build/bin/`.

### Loading in the app

1. Start `./build/Src/OpenTraderPlatform`
2. Stay on the **Trade** tab
3. Use the **Load** button in `StrategyQuickView`
4. Select your strategy executable
5. Start the strategy from the context menu
6. Use **Display Logs** to open the per-strategy log panel

---

## Testing with Replay Mode

Replay mode is the safest way to test a strategy end-to-end.

### Typical workflow

1. Build your strategy
2. Download replay data for a symbol/date from the GUI
3. Enter replay mode
4. Load your strategy
5. Start the strategy
6. Press Play on the replay toolbar
7. Watch:
   - strategy logs
   - chart markers
   - chart bracket overlays
   - orders
   - positions
   - simulated P&L

Important replay details:

- If replay is still in the preloaded paused state, **Start** primes the strategy for the first Play instead of launching it immediately.
- On the first Play press, the host starts all primed strategies, waits for `onStart(...)` to finish, receives the strategy-ready acknowledgement from the runtime, and only then lets replay advance.
- Keep `onStart(...)` focused on fast setup. If your strategy needs expensive warm-up (for example, historical requests across a very large symbol list), queue that work after startup and let symbols become tradable progressively instead of blocking replay start.
- If replay is paused later, your strategy should treat `onPause(...)` as a real trading stop signal and avoid timer-driven actions until `onResume(...)`.
- In replay mode, order placement is handled by the local order emulator instead of the live broker, but replay orders are now rejected unless playback is actually running.

---

## Frequently Asked Questions

### My strategy receives no data. What should I check?

First, make sure you successfully called `claimSymbols(...)`.

Second, check the strategy log for a host error or a rejected symbol claim.

Third, confirm the host actually has data for that symbol in the current mode (live or replay).

### Can I run multiple strategies at the same time?

Yes. Each loaded strategy gets its own child process and its own host-side runtime objects.

### What happens if my strategy crashes?

The host marks that strategy as failed, captures any final log output it can, and keeps the rest of OpenTraderPlatform running.

### How do I get the current market/replay time?

Use:

```cpp
const auto now = m_runtime->requestCurrentTimeUnixNanos();
```

That gives you the host's current time source, which is especially important in replay mode.

### Can I write a strategy in Python or another language?

The **official SDK today is C++**.

Because the transport is process-based, other languages are possible in principle, but they currently need to implement the protocol themselves.

### Where do my logs go?

Three places matter:

- `m_runtime->log(...)` messages go into the host's per-strategy logging pipeline
- child `stdout`
- child `stderr`

All of those are captured into per-strategy log files under:

`~/.local/state/OpenTraderPlatform/StrategiesLogs/`

### Do I still need to build a `.so` file?

No. The old shared-library plugin model is gone from the live strategy runtime.

Build an **executable** instead.

---

## Summary

What changed:

- strategies are now separate executables
- the host supervises them over a Unix-socket + Protobuf runtime
- crashes are isolated much better than before

What to use:

- `ExternalStrategyHandler`
- `ExternalStrategyRuntime`
- `StrategyDescription` + `runStrategyProcessMain(...)`
- replay mode for testing

Where to learn more:

- `Strategies/Examples/ExampleStrategyProcess/`
- `Strategies/README.md`
- [STRATEGY.md](STRATEGY.md)
- [ARCHITECTURE.md](ARCHITECTURE.md)
