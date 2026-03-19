# L2Trader — Strategy Developer Guide

**Who this document is for**: Anyone who wants to write a trading strategy for L2Trader and doesn't need (or want) deep knowledge of C++ internals or system architecture. If you can follow code examples and understand trading concepts, this guide is for you.

> **Migration note**
>
> L2Trader strategies now run as external executables, not in-process `.so` plugins. Parts of this guide still use the older plugin wording while it is being rewritten; for current examples, start with `Strategies/ExampleStrategyProcess/`, `Strategies/README.md`, and the installed `L2TraderStrategySDK`.

---

## Table of Contents

1. [What Is L2Trader?](#what-is-l2trader)
2. [How the Application Works](#how-the-application-works)
3. [What Market Data Is Available](#what-market-data-is-available)
4. [What a Strategy Is](#what-a-strategy-is)
5. [The Strategy Lifecycle](#the-strategy-lifecycle)
6. [Writing Your First Strategy](#writing-your-first-strategy)
7. [Using the SDK: Requesting Historical Bars](#using-the-sdk-requesting-historical-bars)
8. [Reacting to Live Data](#reacting-to-live-data)
9. [Placing Orders](#placing-orders)
10. [Logging from Your Strategy](#logging-from-your-strategy)
11. [Loading and Running Your Strategy in the App](#loading-and-running-your-strategy-in-the-app)
12. [Testing with Replay Mode](#testing-with-replay-mode)
13. [Frequently Asked Questions](#frequently-asked-questions)

---

## What Is L2Trader?

L2Trader is a desktop trading application. Think of it as a command center for monitoring the stock market in real time and placing trades automatically (or manually).

At its core the app does three things:

1. **Receives live market data** — every time the price of a stock changes, every time a buy or sell order is placed on the exchange, L2Trader gets that information in milliseconds.
2. **Displays that data** — in charts, order-book tables, and status panels.
3. **Executes trades** — it can send real buy/sell orders to a TradeStation brokerage account, either by hand through the interface or automatically through a **strategy process** that you write.

---

## How the Application Works

You do not need to understand every technical detail, but a high-level picture helps you write better strategies.

### Two Data Sources

L2Trader connects to two services simultaneously:

| Service | What it provides |
|---------|-----------------|
| **Databento** | All market data — live price quotes, order book depth, trade prints, historical candlestick bars |
| **TradeStation** | All brokerage services — your account, placing orders, tracking positions and balances |

Think of Databento as the live TV feed showing the market, and TradeStation as the brokerage desk where your orders actually go.

### The Main Window

When you launch the application you see:

- **A candlestick chart** — shows how the stock price moved over time, minute by minute. Each candle represents one minute.
- **A Level 2 panel** — shows the 10 best buy (bid) prices and 10 best sell (ask) prices on the exchange right now, with quantities.
- **Order entry panel** — where you can manually enter orders.
- **Orders and positions dock** (bottom) — tables showing your open orders and current positions.
- **Strategies tab** — where you manage your strategy processes.

### What "One Minute Bar" Means

The chart shows **1-minute candlestick bars**. Each bar summarizes what happened in one minute:
- **Open** — price at the start of the minute
- **High** — highest price during that minute
- **Low** — lowest price during that minute
- **Close** — price at the end of the minute
- **Volume** — total shares traded during that minute

The trading day starts at **4:00 AM Eastern time** (pre-market) and ends at **7:00 PM Eastern time**, giving 900 bars per full trading day.

### Live vs. Replay Mode

The app has two modes:

- **Live mode** — connected to Databento; real-time data is flowing.
- **Replay mode** — playing back a recording of a past trading day from a saved file. No live connection needed; orders are simulated locally (no real money moves). This is ideal for testing strategies.

You switch between modes using the data source button in the toolbar.

---

## What Market Data Is Available

Your strategy receives three types of real-time callbacks:

### 1. New Bar (every minute)

```
onNewBar(symbol, bar)
```

Called every time a **completed 1-minute bar** arrives. The `bar` contains:
- `bar.open` — opening price
- `bar.high` — highest price
- `bar.low` — lowest price
- `bar.close` — closing price
- `bar.volume` — total volume
- `bar.timestamp` — the date and time of the bar's open (e.g., 9:31:00 AM)

This is the "slow path" — called once per minute. Good for most strategy logic like moving averages, breakout detection, and pattern recognition.

### 2. New Level 2 (every order book change)

```
onNewLevel2(symbol, level2)
```

Called every time the order book changes — which can happen hundreds of times per second. The `level2` contains:
- `level2.bids` — list of 10 best bid prices with size and count
- `level2.asks` — list of 10 best ask prices with size and count
- `level2.timestamp` — when the snapshot was taken

This is the "fast path" — ideal for measuring order book imbalance or detecting large orders.

### 3. New Trade (every trade print)

```
onNewTrade(symbol, trade)
```

Called every time a trade executes on the exchange. The `trade` contains:
- `trade.price` — the price at which shares traded
- `trade.size` — how many shares
- `trade.side` — whether the aggressor was a buyer or seller
- `trade.timestamp` — when the trade happened

This is the "very fast path" — useful for tracking momentum, aggressor-side analysis, or building your own custom bars at a different time interval.

### Summary

| Callback | Frequency | Use for |
|----------|-----------|---------|
| `onNewBar` | Once per minute | Moving averages, patterns, trend following |
| `onNewLevel2` | Hundreds/sec | Order book imbalance, spread, depth |
| `onNewTrade` | Varies (can be fast) | Momentum, tape reading, custom bars |

---

## What a Strategy Is

A strategy is an **external executable** that L2Trader launches and supervises. The application does not know in advance what your strategy does; it streams market/state updates to your process, and your process sends explicit intents such as symbol claims, logs, historical-data requests, and order placement/cancel requests back to the host.

This approach gives you total control over your trading logic while the application handles all the hard parts: connecting to exchanges, managing network streams, drawing charts, etc.

### The Strategy Contract

Your strategy must implement four things:

| Method | When it's called | What to do |
|--------|-----------------|-----------|
| `onStart()` | When the user clicks **Start** | Initialize state, request historical data if needed |
| `onStop()` | When the user clicks **Stop** | Clean up, save state if needed |
| `onNewBar(symbol, bar)` | Every completed 1-minute bar | Your main trading logic |
| `onNewLevel2(symbol, level2)` | Every order book change | Fast-path logic (optional) |

`onNewTrade` is also available but optional.

### What Your Strategy Can Do

- **Read historical data** — ask for past bars to initialize moving averages, etc.
- **Log messages** — send text to the Strategies tab so you can see what your strategy is thinking.
- **Place orders** — buy or sell the stock (in progress — see [Placing Orders](#placing-orders))
- **Claim a symbol** — tell the app which stock your strategy wants data for

### What Your Strategy Cannot Do Directly

- It cannot access the GUI or modify the chart — it communicates only through the defined callbacks and signals.
- It cannot make raw network requests to external services — all data flows through the L2Trader SDK.

---

## The Strategy Lifecycle

Here is what happens from the moment you load a plugin to when you stop it:

```
1. You click "Load Strategy" in the Strategies tab
         │
         ▼
2. The app opens your .so file and finds your factory functions
   (createStrategy / destroyStrategy)
         │
         ▼
3. The strategy is created and shown on a card in the Strategies tab
   — Status: LOADED
         │
4. You click "Start" on the card
         │
         ▼
5. A dedicated thread is created just for your strategy
   onStart() is called — your initialization code runs
   — Status: RUNNING
         │
6. Market data flows in:
   onNewBar() called on each completed bar
   onNewLevel2() called on each order book snapshot
   onNewTrade() called on each trade
         │
7. You click "Stop"
         │
         ▼
8. onStop() is called — your cleanup code runs
   The thread is shut down
   — Status: STOPPED
         │
9. You can Start again or unload the plugin
```

If your strategy crashes (a programming error causes a fatal signal), the app catches it, marks the strategy as **ERROR**, and continues running normally. Other strategies and the main app are unaffected.

---

## Writing Your First Strategy

Here is a complete, minimal strategy that logs a message every time a new bar arrives. Use it as your starting template.

### File Structure

```
Strategies/
└── MyFirstStrategy/
    ├── CMakeLists.txt
    ├── MyFirstStrategy.h
    └── MyFirstStrategy.cpp
```

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyFirstStrategy)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(MyFirstStrategy SHARED
    MyFirstStrategy.h
    MyFirstStrategy.cpp
)

target_link_libraries(MyFirstStrategy PRIVATE Qt6::Core)
```

### MyFirstStrategy.h

```cpp
#pragma once
#include "Src/Strategy/StrategyBase.h"
#include "Src/Strategy/StrategySDK.h"

class MyFirstStrategy : public StrategyBase {
    Q_OBJECT

public:
    explicit MyFirstStrategy(QObject* parent = nullptr);
    ~MyFirstStrategy() override;

    // Required lifecycle methods
    void onStart() override;
    void onStop() override;

    // Called on every completed 1-minute bar
    void onNewBar(const QString& symbol, const Bar& bar) override;

private:
    StrategySDK m_sdk;
    int m_barCount = 0;   // Just a counter to show we're receiving bars
};

// Required factory functions — do not rename these
extern "C" {
    StrategyBase* createStrategy();
    void destroyStrategy(StrategyBase* strategy);
}
```

### MyFirstStrategy.cpp

```cpp
#include "MyFirstStrategy.h"

MyFirstStrategy::MyFirstStrategy(QObject* parent)
    : StrategyBase(parent)
{
}

MyFirstStrategy::~MyFirstStrategy() = default;

void MyFirstStrategy::onStart()
{
    m_barCount = 0;
    emit logMessage("=== MyFirstStrategy started ===");

    // Tell the SDK which symbol we want data for
    m_sdk.claimSymbol("AAPL");
}

void MyFirstStrategy::onStop()
{
    emit logMessage("=== MyFirstStrategy stopped. Total bars seen: "
                    + QString::number(m_barCount) + " ===");
    m_sdk.releaseSymbol("AAPL");
}

void MyFirstStrategy::onNewBar(const QString& symbol, const Bar& bar)
{
    m_barCount++;

    // Log a summary of each bar
    emit logMessage(
        symbol + " bar #" + QString::number(m_barCount)
        + "  O=" + QString::number(bar.open,  'f', 2)
        + "  H=" + QString::number(bar.high,  'f', 2)
        + "  L=" + QString::number(bar.low,   'f', 2)
        + "  C=" + QString::number(bar.close, 'f', 2)
        + "  V=" + QString::number(bar.volume)
    );
}

// Factory functions
extern "C" {
    StrategyBase* createStrategy()  { return new MyFirstStrategy(); }
    void destroyStrategy(StrategyBase* s) { delete s; }
}
```

### Building

```bash
cd Strategies/MyFirstStrategy
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
# Output: libMyFirstStrategy.so
```

---

## Using the SDK: Requesting Historical Bars

Often you need historical data to initialize your strategy — for example, to compute a 20-bar moving average before you start trading.

### Example: Compute a Simple Moving Average on Start

```cpp
#include <numeric>  // for std::accumulate

void MySMAStrategy::onStart()
{
    emit logMessage("Fetching historical bars...");

    QDate today = QDate::currentDate();
    QDate fiveDaysAgo = today.addDays(-5);

    // Request the last 5 days of bars for AAPL
    // The callback is called once the data arrives (usually within 1-2 seconds)
    m_sdk.requestHistoricalBars("AAPL", fiveDaysAgo, today,
        [this](std::shared_ptr<QVector<Bar>> bars) {

            if (!bars || bars->isEmpty()) {
                emit logMessage("Warning: no historical bars received");
                return;
            }

            emit logMessage("Received " + QString::number(bars->size()) + " historical bars");

            // Keep the last 20 closes for our SMA calculation
            int count = qMin(20, static_cast<int>(bars->size()));
            m_recentCloses.clear();
            for (int i = bars->size() - count; i < bars->size(); ++i)
                m_recentCloses.push_back((*bars)[i].close);

            double sma = std::accumulate(m_recentCloses.begin(),
                                         m_recentCloses.end(), 0.0)
                         / m_recentCloses.size();

            emit logMessage("Initial 20-bar SMA: " + QString::number(sma, 'f', 2));
        }
    );
}
```

### Important Notes About Historical Data Requests

- The request is **asynchronous** — the callback runs a little later, not immediately. Do not try to use the data right after making the call; use it inside the callback.
- The bars are returned in **chronological order** (oldest first, newest last).
- Each bar's `timestamp` is the **open time** of that bar. For example, a bar that covers 9:31:00–9:31:59 AM has a timestamp of `9:31:00 AM`.

---

## Reacting to Live Data

### Updating a Moving Average on Each New Bar

```cpp
void MySMAStrategy::onNewBar(const QString& symbol, const Bar& bar)
{
    // Add new close to our rolling window
    m_recentCloses.push_back(bar.close);
    if (m_recentCloses.size() > 20)
        m_recentCloses.pop_front();   // Remove oldest

    if (m_recentCloses.size() < 20) {
        emit logMessage("Warming up... " + QString::number(m_recentCloses.size()) + "/20 bars");
        return;
    }

    double sma = std::accumulate(m_recentCloses.begin(),
                                 m_recentCloses.end(), 0.0)
                 / m_recentCloses.size();

    emit logMessage(symbol + " close=" + QString::number(bar.close, 'f', 2)
                    + "  SMA20=" + QString::number(sma, 'f', 2));

    // Simple crossover signal
    if (bar.close > sma)
        emit logMessage(">>> Price ABOVE SMA — potential long setup");
    else
        emit logMessage(">>> Price BELOW SMA — stay flat");
}
```

### Reading the Order Book (Level 2)

```cpp
void MyL2Strategy::onNewLevel2(const QString& symbol, const Level2& level2)
{
    // Get the best bid and ask
    if (level2.bids.isEmpty() || level2.asks.isEmpty())
        return;

    double bestBid = level2.bids[0].price;
    double bestAsk = level2.asks[0].price;
    double spread  = bestAsk - bestBid;

    // Calculate total bid volume vs ask volume (top 5 levels)
    double totalBidVol = 0;
    double totalAskVol = 0;
    for (int i = 0; i < qMin(5, level2.bids.size()); ++i)
        totalBidVol += level2.bids[i].size;
    for (int i = 0; i < qMin(5, level2.asks.size()); ++i)
        totalAskVol += level2.asks[i].size;

    double imbalance = (totalBidVol - totalAskVol)
                     / (totalBidVol + totalAskVol + 1e-9);

    // Only log if imbalance is significant (to avoid spam)
    if (qAbs(imbalance) > 0.3) {
        emit logMessage(symbol
            + " Spread=" + QString::number(spread, 'f', 3)
            + " Imbalance=" + QString::number(imbalance, 'f', 2)
            + (imbalance > 0 ? " (BID heavy)" : " (ASK heavy)"));
    }
}
```

> **Tip**: Be careful with `onNewLevel2` — it is called very frequently. Logging on every call will flood your log panel. Only log when something meaningful happens (a threshold crossed, a condition met).

---

## Placing Orders

> **Status**: Order placement via `StrategySDK` is currently in development. The API below shows the intended interface; availability may vary with your version of L2Trader.

### Market Order

```cpp
// Buy 100 shares of AAPL at market price
m_sdk.placeMarketOrder("AAPL", 100, OrderSide::Buy);
```

### Limit Order

```cpp
// Buy 100 shares of AAPL with a limit of $195.00
m_sdk.placeLimitOrder("AAPL", 100, OrderSide::Buy, 195.00);
```

### Cancel Order

```cpp
m_sdk.cancelOrder(orderId);
```

### In Replay Mode

When you run the application in **Replay mode**, all orders are **simulated locally** — no real money, no real brokerage. The simulator:
- Introduces a realistic reception delay (100–500 ms)
- Introduces a realistic execution delay (10–50 ms)
- Fills limit orders when the market depth reaches your price
- Tracks a virtual account with a starting balance of $100,000

This means you can test your full strategy including order placement and position management without any risk.

---

## Logging from Your Strategy

The `logMessage` signal sends text to the **Logs panel** on your strategy's card in the Strategies tab. This is your primary window into what your strategy is doing.

```cpp
emit logMessage("Simple text message");
emit logMessage("Value of X: " + QString::number(x));
emit logMessage("Bar arrived: close=" + QString::number(bar.close, 'f', 2));
```

**Tips**:
- Keep messages concise — they appear in a scrolling list.
- Prefix important events to make them easy to spot: `">>> SIGNAL: buy condition met"`
- Do **not** log on every single Level 2 update — it will be too fast to read and will slow down the log panel.

---

## Loading and Running Your Strategy in the App

### Step 1 — Build your strategy

```bash
cd Strategies/MyFirstStrategy/build
cmake --build .
```

This produces `libMyFirstStrategy.so` (on Linux).

### Step 2 — Open the Strategies tab

Click the **Strategies** tab in the main window.

### Step 3 — Load the plugin

Click **Load Strategy**. A file dialog opens. Navigate to your `.so` file and select it.

Your strategy appears as a card:

```
┌──────────────────────────────────────────────┐
│  MyFirstStrategy                  ● LOADED   │
│                         [Start]  [Stop]       │
├──────────────────────────────────────────────┤
│  Logs  │  Info  │  Orders  │  Positions      │
│  (empty — strategy not started yet)          │
└──────────────────────────────────────────────┘
```

### Step 4 — Select a stock symbol

In the toolbar at the top of the main window, type the ticker symbol you want to monitor (e.g., `AAPL`) and press Enter. The chart updates and the Databento connection subscribes to that symbol.

### Step 5 — Start the strategy

Click **Start** on the strategy card. `onStart()` is called. You should see log messages appear in the Logs panel.

### Step 6 — Monitor

Watch the Logs panel in your strategy card. You will also see your strategy listed in the **Strategy Quick View** in the Trade tab (the compact tree widget on the right side).

### Step 7 — Stop the strategy

Click **Stop** on the card. `onStop()` is called and the strategy goes to STOPPED state. You can start it again at any time.

---

## Testing with Replay Mode

Replay mode lets you test your strategy against historical data without any live connection or real money.

### How to Download Replay Data

1. Open the **Records Info** tab in the app (part of the Config tab area).
2. Select a symbol and date.
3. Click **Download**. The app downloads the historical order book and trade data from Databento and saves it locally.
4. Downloading takes a few seconds to a minute depending on your internet connection.

### How to Enter Replay Mode

1. Click the **data source label** in the toolbar (it shows "Live" when in live mode).
2. Select a date and click OK.
3. The app switches to replay mode. The chart clears and will start showing historical data as you play back.

### Playing Back

- Click the **Play button** (▶) in the toolbar to start playback.
- Use the **speed selector** to control how fast time moves:
  - `0.01×` — very slow (good for watching individual order book changes)
  - `1×` — real-time
  - `10×` — 10× faster
  - `Max` — as fast as possible (good for rapid backtesting)
- Press **Pause** (⏸) to freeze playback.
- Press Space bar as a shortcut for play/pause.

### What Happens to Your Strategy During Replay

Your strategy works **identically** in replay mode:
- `onStart()` / `onStop()` are called at the same times
- `onNewBar()`, `onNewLevel2()`, `onNewTrade()` fire with historical data
- Orders you place go to the local order emulator — you see them in the Orders dock, positions update, P&L is calculated

The only difference: the "market" data comes from a file instead of a live feed, and orders don't go to a real broker.

### A Typical Testing Workflow

1. **Write strategy** — code your logic
2. **Build** — `cmake --build .`
3. **Download replay data** — pick a symbol and an interesting date
4. **Load strategy** in the app
5. **Enter Replay mode** with that date
6. **Click Start** on the strategy, then **Play** on the toolbar
7. **Watch the Logs panel** — see what decisions your strategy makes
8. **Check the Orders and Positions** docks to see simulated trades
9. **Iterate** — modify strategy code, rebuild, reload, test again

---

## Frequently Asked Questions

### My strategy is not receiving any bars. What is wrong?

Make sure you called `m_sdk.claimSymbol("YOURSTOCK")` in `onStart()`. Without claiming a symbol, the app does not know which stock to route data to your strategy.

Also make sure the same symbol is selected in the toolbar at the top of the main window — the Databento connection only streams data for the currently subscribed symbol.

### Can I run multiple strategies at the same time?

Yes. Each strategy gets its own card and its own thread. You can start several strategies simultaneously. They do not interfere with each other.

### What happens if my code crashes?

The app detects the crash, marks your strategy as **ERROR** (the card turns red), and keeps running normally. Other strategies and the main application are completely unaffected. Fix the bug, rebuild, reload, and start again.

### How do I access the current time?

Inside any callback, use Qt's `QDateTime::currentDateTime()`. In replay mode, this returns the current replay time (the simulated market time), not your computer's real clock.

### Can I write my strategy logic in Python?

Not currently. Strategies must be written in C++ as they are loaded as native shared libraries. The system is designed for maximum performance — trading logic needs to run in microseconds.

### How do I store state between bars?

Use member variables in your strategy class:

```cpp
class MyStrategy : public StrategyBase {
private:
    StrategySDK m_sdk;
    double m_previousClose = 0.0;   // Persists between onNewBar() calls
    int m_positionSize = 0;
    QVector<double> m_recentCloses;
};
```

Anything you put in a member variable persists for the entire lifetime of your strategy (from `onStart()` to `onStop()`).

### What is the difference between `onNewBar` and `onNewTrade`?

- `onNewBar` fires **once per minute** with a completed, summarized candlestick bar. Use this for most strategy logic.
- `onNewTrade` fires for **every individual trade** on the exchange — this can be hundreds or thousands of times per minute. Use it only when you need tick-by-tick granularity.

### Can I access multiple symbols?

You can call `m_sdk.claimSymbol()` for multiple symbols. The `symbol` argument in `onNewBar(symbol, bar)` tells you which stock the data is for. Note: the app streams data for the symbol currently shown in the toolbar; additional symbols beyond the main displayed one may require a separate live subscription.

### Where are my strategy log messages saved?

Currently, strategy log messages are displayed in the **Logs panel** on your strategy card. They are not written to disk. If you need persistent logging, use Qt's `qCDebug()` / `qCInfo()` functions with a custom logging category — those messages go to the application's log files.

---

## Summary

| Concept | What it means for you |
|---------|----------------------|
| **Bar** | 1-minute candlestick. Your main data source in `onNewBar()`. |
| **Level 2** | Full 10-level order book snapshot. Available in `onNewLevel2()` for fast-path logic. |
| **Trade** | Individual trade print. Available in `onNewTrade()` for tick-level analysis. |
| **Claim symbol** | Call `m_sdk.claimSymbol("AAPL")` so the app routes that stock's data to you. |
| **logMessage** | `emit logMessage("text")` — shows up in the Logs panel on your card. |
| **Historical bars** | `m_sdk.requestHistoricalBars(...)` — async request; use the result inside the callback. |
| **Replay mode** | Test with real historical data, no real money, simulated order fills. |
| **Crash isolation** | If your code crashes, only your strategy is affected — the app keeps running. |

---

*For technical details about the plugin system, threading model, and full API reference, see [STRATEGY.md](STRATEGY.md).*  
*For the overall application architecture, see [ARCHITECTURE.md](ARCHITECTURE.md).*
