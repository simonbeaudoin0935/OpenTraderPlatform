# L2Trader — Strategy Developer Guide

**Who this document is for**: people who want to write a strategy for L2Trader without having to learn the full host-side architecture first.

The most important change to know is this:

> L2Trader strategies are no longer in-process `.so` plugins.
>
> A strategy is now a **separate executable** that the platform launches and supervises.

That means a crashing strategy usually takes down **only itself**, not the whole trading platform.

---

## Table of Contents

1. [What Is L2Trader?](#what-is-l2trader)
2. [What a Strategy Is](#what-a-strategy-is)
3. [How a Strategy Talks to the App](#how-a-strategy-talks-to-the-app)
4. [Writing Your First Strategy](#writing-your-first-strategy)
5. [Building and Loading It](#building-and-loading-it)
6. [Testing with Replay Mode](#testing-with-replay-mode)
7. [Frequently Asked Questions](#frequently-asked-questions)

---

## What Is L2Trader?

L2Trader is a desktop trading application that combines:

- **Databento** for market data (bars, trades, Level 2)
- **TradeStation** for brokerage (orders, positions, balances)
- **Replay mode** for testing with historical data and local order emulation

The host application owns the UI, the broker connections, replay control, persistence, and safety checks.

Your strategy only focuses on trading logic.

---

## What a Strategy Is

A strategy is a **child process** launched by L2Trader.

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

This split is the reason the platform is much safer than the old plugin model.

---

## How a Strategy Talks to the App

You usually do **not** work with raw sockets or Protobuf directly.

Instead, you implement `ExternalStrategyHandler` and let `ExternalStrategyRuntime` do the connection work.

### The two main SDK classes

- `L2Trader::StrategySDK::ExternalStrategyHandler`
  - you subclass this
  - you override methods like `onStart(...)`, `onBar(...)`, and `onStop(...)`
- `L2Trader::StrategySDK::ExternalStrategyRuntime`
  - owns the runtime loop
  - connects to the host
  - gives you helper methods like:
    - `log(...)`
    - `claimSymbols(...)`
    - `requestHistoricalBars(...)`
    - `placeOrder(...)`
    - `cancelOrder(...)`
    - `requestCurrentTimeUnixNanos()`

### Important rule

Before you expect market data or trading authority for a symbol, call `claimSymbols(...)`.

---

## Writing Your First Strategy

The simplest good starting point is `Strategies/ExampleStrategyProcess/main.cpp`.

Here is the basic pattern:

```cpp
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

If you want a real working example, use `Strategies/HistoricalBarsStrategy/`.

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

For a complete working sample, see `Strategies/OrderTestStrategy/`.

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

---

## Building and Loading It

### CMake

Inside this repository, use the helper from `Strategies/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyStrategyProcess VERSION 1.0.0 LANGUAGES CXX)

add_l2trader_process_strategy(MyStrategyProcess
    main.cpp
)
```

### Manifest

A strategy is usually loaded through a JSON manifest:

```json
{
  "name": "My Strategy Process",
  "runtimeType": "external-process",
  "executablePath": "~/.local/share/L2Trader/Strategies/MyStrategyProcess",
  "symbols": ["AAPL"],
  "positionSize": 1,
  "riskLimit": 100.0,
  "customParams": {
    "accountID": "SIM123456"
  }
}
```

The GUI can load either:

- the manifest, or
- the executable directly

If a matching manifest exists, the load dialog uses it to prefill values and any declared custom parameter schema.

### Build commands

From the repository root:

```bash
cmake --build build/GUI --target MyStrategyProcess
cmake --build build/GUI --target stage-strategy-sdk
```

Outputs go to the active build tree, for example:

- `build/GUI/bin/MyStrategyProcess`
- `build/GUI/bin/l2trader-mcp-server`
- `build/GUI/strategy-sdk/`

There is **no top-level `build/bin/`**.

### Loading in the app

1. Start `./build/GUI/Src/L2Trader`
2. Stay on the **Trade** tab
3. Use the **Load** button in `StrategyQuickView`
4. Select your manifest or executable
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
   - orders
   - positions
   - simulated P&L

In replay mode, order placement is handled by the local order emulator instead of the live broker.

---

## Frequently Asked Questions

### My strategy receives no data. What should I check?

First, make sure you successfully called `claimSymbols(...)`.

Second, check the strategy log for a host error or a rejected symbol claim.

Third, confirm the host actually has data for that symbol in the current mode (live or replay).

### Can I run multiple strategies at the same time?

Yes. Each loaded strategy gets its own child process and its own host-side runtime objects.

### What happens if my strategy crashes?

The host marks that strategy as failed, captures any final log output it can, and keeps the rest of L2Trader running.

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

`~/.local/state/L2Trader/StrategiesLogs/`

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
- a JSON manifest
- replay mode for testing

Where to learn more:

- `Strategies/ExampleStrategyProcess/`
- `Strategies/README.md`
- [STRATEGY.md](STRATEGY.md)
- [ARCHITECTURE.md](ARCHITECTURE.md)
