# Algo/ Directory - Trading Algorithm Components - Agent Instructions

The Algo directory contains the trading algorithm coordination logic and various receivers for processing market data and trading events. TSClient is brokerage-only (orders, positions, accounts). Market data (Level 2, trades, bars) flows from DBClient in both live and replay modes through a unified pipeline.

## Overview

**Location**: `Src/Algo/`
**Purpose**: Trading algorithm coordination, data routing, and SymbolContext actor model
**Key Component**: MainAlgo singleton running in dedicated thread

**Directory Contents**:
- `MainAlgo.h/cpp` — Central coordinator singleton
- `BarReceiver/` — Bar reception (wired to `LiveBarAccumulator`)
- `Level2Receiver/` — Level 2 (10-level book) reception, replaces former `MarketDepthQuoteReceiver/`
- `Level1Receiver/` — Level 1 (BBO) reception for secondary stocks (new)
- `PositionsReceiver/` — Position tracking via StreamPositions
- `OrdersReceiver/` — Order tracking via StreamOrders
- `StreamReceiver/` — Base class for all receivers

## MainAlgo (MainAlgo.h/cpp)

**Role**: Central coordinator for all trading algorithm operations

### Singleton Pattern

```cpp
class MainAlgo final : public QObject {
public:
    static MainAlgo* getInstance();
    static void destroyInstance();

    Q_DISABLE_COPY(MainAlgo)

private:
    static MainAlgo* m_instance;
    explicit MainAlgo();
    ~MainAlgo();
};
```

### Threading

MainAlgo runs in its own dedicated QThread:
```cpp
QThread thread;  // Stack-allocated (preferred pattern)

MainAlgo::MainAlgo() {
    moveToThread(&thread);
    thread.start();
}

~MainAlgo() {
    thread.quit();
    if (!thread.wait(5000)) {
        thread.terminate();
        thread.wait();
    }
}
```

### Key Responsibilities

1. **Stock Instrument Management**
   - Create and manage SymbolContext instances (one per symbol)
   - Track currently displayed stock via `m_currentDisplayedSymbolContext`
   - Coordinate bar caching per symbol

2. **Account and Balance Management**
   - Track TradeStation accounts (brokerage-only via TSClient)
   - Poll balances periodically (configurable interval)
   - Emit balance updates to UI

3. **Position Tracking**
   - Manage PositionsReceiver (Qt parent-child ownership)
   - Track open positions across accounts
   - Forward position updates to UI

4. **Order Management**
   - Manage OrdersReceiver (Qt parent-child ownership)
   - Place, modify, cancel orders via TSClient
   - Track order status changes
   - Forward order updates to UI

5. **Risk Management**
   - Own and evaluate account risk state through `RiskManager` before allowing order placement
   - Track drawdown basis metrics from balance updates (equity/today PnL/realized PnL)
   - Enforce hard limits (drawdown, per-trade planned loss, position size/notional, daily entries, open positions)
   - Maintain lock/cooldown lifecycle and emit `riskStatusChanged(accountId)` for GUI refresh
   - Persist live/sim risk runtime state into the active ledger and use replay-session risk state during replay

6. **Market Data Forwarding**
   - Connect Level2Receiver signals to `displayedStockReceivedNewLevel2` for the displayed stock
   - Connect BarReceiver signals to `displayedStockReceivedNewBar` for the displayed stock
   - Write display data into `DisplaySnapshot` (inside SymbolContext) for 30 Hz GUI pull
   - Forward raw Level2 data (DWP/BAI computation removed — belongs in individual strategies)

7. **Replay Coordination**
   - Forward replay control signals (start, stop, pause, resume) from DBClient to UI
   - Manage replay order/position streams with simulated account

8. **Strategy Management**
   - Own and run StrategyManager for loaded external strategy processes
   - Persist strategy chart log markers via OrdersDatabase
   - Track per-symbol live strategy chart status (`m_strategyStatusesBySymbol`) for chart top-left rich-text overlay
   - Own platform-managed bracket lifecycle (`m_managedBrackets`) across virtual/native modes
   - In regular live/sim session, native managed-bracket failures hard-drop protection (no virtual fallback) and emit
     GUI warning signal
   - Track virtual exit order IDs to keep managed virtual exits single-flight (prevents duplicate flatten submissions)
   - Manage strategy manual order confirmations (queue + active request timer)
   - In replay mode, pause/resume active manual confirmation countdowns with replay playback so timeouts do not elapse
     while replay is paused
   - Support strategy user-confirm preview brackets with user stop override; final `Y` accept still passes through risk gate
   - Support global hard mute mode for strategy confirmations (`M`): reject active + queued confirmations and auto-reject
     future confirmations while muted
   - Respect per-symbol manual-confirmation blocklist (`m_blockedManualConfirmationSymbols`): when the user presses
     `Shift+N` on an active confirmation, reject it and auto-reject future confirmations for that symbol before they
     reach UI
   - Emit strategy chart artifact signals to GUI (`strategyLogEmitted`, `strategyStatusEmitted`,
     `managedBracketOverlayEmitted`, `managedBracketProtectionDropped`)

### Risk Management Integration (RiskManager)

Risk checks are centralized in `MainAlgo::processPlaceOrder(...)` and run for both manual GUI orders and strategy-driven
orders.

Evaluation pipeline (entry orders):

1. lock/cooldown checks
2. entry count / open-position count limits
3. size + notional checks
4. stop-aware planned-loss-per-trade check
5. drawdown budget check (can hard-lock trading)

Drawdown basis modes are implemented in `RiskManager`:

- trailing `EquityPeak`
- trailing `TodaysProfitLoss`
- trailing `RealizedProfitLoss`
- non-trailing `TodaysProfitLossFromBaseline`

For user-confirm strategy orders, GUI preview stop overrides are injected into the final `PlaceOrderRequest` so the
planned-loss risk check uses the same stop visible on chart.

### Key Members

```cpp
class MainAlgo {
    QThread thread;

    // Per-symbol actors (each with FIFO queue + drain loop)
    QMap<QString, QPointer<SymbolContext>> m_symbolContexts;
    QPointer<SymbolContext> m_currentDisplayedSymbolContext;

    // Receivers (Qt parent-child ownership, parent is 'this')
    PositionsReceiver* m_positionReceiver = nullptr;
    OrdersReceiver* m_orderReceiver = nullptr;
    bool positionStreamStarted = false;
    bool orderStreamStarted = false;

    // Account and balance
    Account m_activeAccount;
    Balance m_currentBalance;
    std::unique_ptr<QTimer> m_balancePollingTimer;

    // Strategy management
    StrategyManager m_strategyManager;

    // Crash monitoring
    std::unique_ptr<QSocketNotifier> m_crashNotifier;
};
```

### Signal Interface

**Signals emitted by MainAlgo**:
```cpp
signals:
    // Market data for displayed stock (internal use — NOT wired to GUI directly)
    // These drive BarAggregator and StrategyManager.  The GUI pulls display data
    // from DisplaySnapshot at 30 Hz instead of receiving these signals.
    void displayedStockReceivedNewBar(QString symbol, Bar bar);
    void displayedStockReceivedNewLevel2(QString symbol, Level2 level2);
    void displayedStockReceivedNewTrade(QString symbol, Trade trade);

    // Trading events
    void receivedNewPosition(QString account, Position position);
    void positionDeleted(QString account, QString positionID);
    void receivedNewOrder(QString account, Order order);
    void strategyLogEmitted(StrategyLogEntry entry);
    void strategyStatusEmitted(StrategyStatusEntry entry);
    void managedBracketOverlayEmitted(StrategyBracketOverlayEntry entry);

    // Account and balance
    void tradeStationAccountsReceived(QVector<Account> accounts);
    void balanceUpdated(Balance balance);

    // Replay control (forwarded from DBClient)
    void replayStarted();
    void replayStopped();
    void replayPaused();
    void replayResumed();
    void replayTimeUpdated(QDateTime currentTime);
    void replayEndReached();
```

> **Removed signals**: `displayedStockReceivedNewQuote` (Quote type removed; BBO now comes from `Level2.m_bids[0]`/`Level2.m_asks[0]`), `displayedStockReceivedNewMarketDepthQuote` (replaced by `displayedStockReceivedNewLevel2`).

**Slots for receiving events**:
```cpp
public slots:
    void onTradeStationAuthStateChanged(bool isAuthenticated,
                                        TSClient::AuthStateReason reason,
                                        const QString& message);
    void onSelectDisplayedStock(const QString& symbol);

private slots:
    void onReceivedNewPosition(const QString& account, Position position);
    void onPositionDeleted(const QString& account, const QString& positionID);
    void onLoadedPositionsFromDatabase(const QString& account, QMap<QString, Position> positions);
    void onReceivedNewOrder(const QString& account, Order order);

    void onReceivedAsyncGetAccounts(const QVector<Account>& results);
    void onBalanceReceived(const QVector<Balance>& results);
    void requestBalance();

    void onStrategyCrashNotified();
    void onReplayEndReached();
```

## SymbolContext

**Role**: Per-symbol passive actor — holds all data/receivers for one symbol and processes events via a thread-pool-backed work queue.

**Actor Model Pattern**:
```cpp
class SymbolContext : public QObject {
public:
    explicit SymbolContext(const QString& p_symbol, QObject* p_parent = nullptr);
    ~SymbolContext();

    void enqueueLevel2(const Level2& p_level2);  // Thread-safe enqueue
    void enqueueTrade(const Trade& p_trade);      // Thread-safe enqueue

    QString symbol;
    BarCache barCache;
    BarReceiver barReceiver;
    Level2Receiver m_level2Receiver;
    LiveBarAccumulator m_liveBarAccumulator;     // 1-minute bar accumulator
    BarAggregator m_barAggregator;

private:
    using WorkItem = std::variant<Level2, Trade>;
    void drain();
    void processLevel2(const Level2& l2);
    void processTrade(const Trade& trade);

    QMutex m_queueMutex;
    QQueue<WorkItem> m_queue;
    std::atomic<bool> m_draining{false};
    std::atomic<bool> m_destroying{false};
    QWaitCondition m_drainDone;
};
```

#### DisplaySnapshot (Pull-Based GUI Data)

Each SymbolContext holds a `DisplaySnapshot` struct that is the bridge between
the MainAlgo thread (writer) and the GUI thread (reader at 30 Hz):

```cpp
struct DisplaySnapshot {
    QReadWriteLock lock;

    // Data fields (written by MainAlgo, read by GUI)
    Bar       latestBar;
    Level2    latestLevel2;
    QVector<Trade> pendingTrades;  // Accumulated since last GUI read
    Bar       aggregatorBar;       // Current higher-TF bar
    QDateTime replayTime;

    // Dirty-flag bitmask (cleared by GUI after read)
    enum DirtyFlag : uint8_t {
        L2        = 1,
        Trade     = 2,
        BarFlag   = 4,
        Aggregator= 8,
        ReplayTime=16,
    };
    std::atomic<uint8_t> dirtyFlags{0};
};
```

MainAlgo writes under `QWriteLocker`; GUIFrontend reads and clears dirty flags
under `QWriteLocker` (needs write access to clear flags). The dirty-flag bitmask
allows the GUI to skip unchanged widgets each tick.

**Threading Guarantees**:
- `enqueueLevel2()` / `enqueueTrade()` are thread-safe (mutex-protected push)
- Only ONE pool thread drains a symbol at a time → sequential per symbol, parallel across symbols
- ABA-safe drain loop: re-checks queue after clearing `m_draining` flag
- Destructor sets `m_destroying` and waits for running drain to finish

**DirectConnection Rule**:
All internal signal-slot connections within SymbolContext use `Qt::DirectConnection` so they execute on the pool thread during `drain()`. External connections (to MainAlgo, GUI, StrategyManager) keep `Qt::AutoConnection` → become `QueuedConnection` when emitted from the pool thread.

**Centralized Routing**:
SymbolContext does NOT connect to DBClient directly. MainAlgo wires:
```
DBClient::newLevel2 → MainAlgo::onNewLevel2Received → symbolContext->enqueueLevel2()
DBClient::newTrade  → MainAlgo::onNewTradeReceived  → symbolContext->enqueueTrade()
```
This is the same handler for both live and replay modes — no mode-checking required.

**Lifecycle**:
- Created when symbol first selected via `onSelectDisplayedStock()`
- Previous instrument is cleaned up (`deleteLater()`) when a different symbol is selected
- Only one SymbolContext exists at a time (the displayed stock)
- MainAlgo subscribes to DBClient live data on creation (if not in replay mode)

## BarAggregator

**Role**: Real-time aggregation of closed 1-minute bars into higher-timescale bars

**Location**: `Src/Algo/BarAggregator/BarAggregator.h/.cpp`

**Purpose**: Derives live 5m, 15m, 30m, 1h, 4h, 1d, 1w, and 1M bars from the stream of closed 1m bars. Runs on the MainAlgo thread.

```cpp
class BarAggregator : public QObject {
    Q_OBJECT
public:
    explicit BarAggregator(QObject* parent = nullptr);

public slots:
    void onNewBar(const QString& symbol, const Bar& bar);  // feed closed 1m bars here

signals:
    // Thread context: emitted from MainAlgo thread
    void barUpdated(TimeFrame tf, const QString& symbol, const Bar& bar);  // in-progress bar
    void barClosed(TimeFrame tf, const QString& symbol, const Bar& bar);   // completed bar
};
```

**Accumulation logic**:
- Maintains one `Accumulator` struct per target TimeFrame per symbol
- Intraday close detection: `(minutesFromOpen + 1) % tfMinutes == 0`
- Daily bar closes at `TIME_LAST_CANDLE_AFTER_MARKET_SESSION`
- Weekly bar closes on Friday at `TIME_LAST_CANDLE_AFTER_MARKET_SESSION`
- Monthly bar closes when the next trading day falls in a different calendar month

**Wiring** (in `SymbolContext` constructor):
```cpp
connect(&barReceiver.m_liveBarAccumulator, &LiveBarAccumulator::barClosed,
        &m_barAggregator, &BarAggregator::onNewBar);
connect(&m_barAggregator, &BarAggregator::barClosed,
        &barCache, [&barCache](TimeFrame tf, const QString& sym, const Bar& bar) {
            barCache.storeBar(tf, sym, bar);
        });
```

## Receiver Components

### BarReceiver/

**Role**: Receive and process bar data

`BarReceiver` publishes exactly one source of 1m bars per mode:
- **Replay**: `DBClient::newTrade` → `MainAlgo::routeTrade` → `SymbolContext::enqueueTrade()` → `LiveBarAccumulator::onNewTrade()` (drain loop) → `BarReceiver`.
- **Live/Sim**: TradeStation bar stream → `MainAlgo::routeBar` → `SymbolContext::enqueueBar()` → `processBar()` (drain loop) → `BarReceiver` and `BarAggregator` (Closed → `onNewBar`, Open → `onBarUpdated`). Live trades are **not** fed into the 1m accumulator (they are derived from this same bar stream, so they would build a duplicate candle).

### Live/Sim Time & Sales Reconstruction (`TapeReconstructor/BarTapeReconstructor`)

TradeStation exposes no trade tape, so live/sim Time & Sales is rebuilt from the bar stream:

- `SymbolContext::processBar` feeds each 1m update to `BarTapeReconstructor::onBar`. The `TotalVolume` delta versus the previous update becomes one print (size = delta, price = bar close, timestamp = receive time); a new minute prints that bar's whole volume. The print then goes through `processTrade` (Time & Sales snapshot, `receivedNewTrade` for strategies).
- The first update, and the first update after the bar stream reopens (`enqueueTapeReset`), is only a baseline. Updates for an older minute or with non-increasing volume are ignored (TradeStation re-sends stale bars around rollover).
- Side: close ≥ ask → `TradeSide::Bid` (buy aggressor), close ≤ bid → `TradeSide::Ask`, else `None` (same convention as Databento). The BBO comes from the latest Level 2 top of book while it is fresher than `TapeReconstructionConstants::LEVEL2_BBO_MAX_AGE_MS`, otherwise from the quote stream (`MainAlgo::routeQuote` → `enqueueQuoteBbo`). The quote stream no longer produces trades.
- Limitations: each print batches every trade between two bar updates (~0.25–3 s), intermediate prices are lost, and BBO timing skew can mislabel some prints.
- Replay is unchanged: real Databento trades drive Time & Sales.

```cpp
class BarReceiver : public StreamReceiver {
signals:
    void receivedNewBar(QString symbol, Bar newBar);

protected:
    QPointer<Stream> getStreamBase() const override {
        return nullptr;  // No TradeStation stream — bars come via LiveBarAccumulator
    }

private:
    const QString m_symbol;
};
```

Historical bars are loaded into BarCache via Databento historical API requests.

### Level2Receiver/

**Role**: Process Level 2 market depth data (10-level book snapshots)

Replaces the former `MarketDepthQuoteReceiver/`. Uses the `Level2` model (from `Src/Core/Models/Level2.h`) which contains `std::array<Level2Row, 10>` for bids and asks. Connected via `SymbolContext::enqueueLevel2()` → `Level2Receiver::onReceivedNewLevel2()` (DirectConnection in drain loop). Same path for live and replay.

**Key Features**:
- Computes metrics from 10-level book snapshots:
  - **Bid-Ask Imbalance**: Ratio of bid vs ask volume across configurable levels
  - Raw Level2 data forwarded to strategies and GUI; metrics computed per-strategy
- No stream queuing logic (Databento has no concurrent stream limits)

**Signal**:
```cpp
void receivedNewLevel2(QString symbol, Level2 level2);  // 2 params only
```

**Slot** (invoked when Level2 data arrives):
```cpp
void onReceivedNewLevel2(Level2 level2);
```

**Data Flow** (identical for live and replay):
```
DBClient::newLevel2 (Databento Schema::Mbp10 or replay .dbn.zst)
    ↓ [signal → MainAlgo thread]
MainAlgo::onNewLevel2Received()
    ↓ m_symbolContexts[sym]->enqueueLevel2()
SymbolContext FIFO queue
    ↓ [QThreadPool drain()]
Level2Receiver::onReceivedNewLevel2() [DirectConnection]
    ↓ emit receivedNewLevel2(...)
    ↓ [QueuedConnection → MainAlgo thread]
MainAlgo → displayedStockReceivedNewLevel2
    ↓
GUIFrontend → Level2Table display
```

**BBO Access**: Best bid/ask is `Level2.m_bids[0]` and `Level2.m_asks[0]`. There is no separate Quote type.

### Level1Receiver/

**Role**: Receive Level 1 BBO updates for secondary (non-displayed) stocks

New receiver added as part of the Databento migration. Subscribes to `DBClient::newLevel1` (Databento `Schema::Mbp1`). Lightweight alternative to Level2Receiver for stocks that only need top-of-book data.

**Signal**:
```cpp
void receivedNewLevel1(QString symbol, Level1 level1);
```

**Slot**:
```cpp
void onReceivedNewLevel1(Level1 level1);
```

The `Level1` struct (defined in `Src/Core/Models/Level2.h`) contains a single `Level2Row m_bid` and `Level2Row m_ask`.

### PositionsReceiver/

**Role**: Track and manage trading positions

**Key Responsibilities**:
- Receive position updates from StreamPositions (TSClient brokerage stream)
- Maintain position map by account and position ID
- Calculate P/L (realized and unrealized)
- Detect position opens/closes
- Load historical positions from PositionsDatabase
- Emit events for position changes

**Signals**:
```cpp
void receivedNewPosition(QString account, Position position);
void positionDeleted(QString account, QString positionID);
void loadedPositionsFromDatabase(QString account, QMap<QString, Position> positions);
```

**Position Lifecycle**:
```
New Position → Open → Updated (price changes) → Closed
                  ↓
               Position object maintained in receiver
```

### OrdersReceiver/

**Role**: Track order status and lifecycle

**Key Responsibilities**:
- Receive order updates from StreamOrders (TSClient brokerage stream)
- Maintain order map by account and order ID
- Track order state transitions
- Store order history to OrdersDatabase
- Emit events for order changes

**Signal**:
```cpp
void receivedNewOrder(QString account, Order order);
```

**Order State Machine**:
```
Submitted → Acknowledged → Open → PartiallyFilled → Filled
                         ↓           ↓
                       Canceled   Canceled
                         ↓
                      Rejected
```

### StreamReceiver/

**Role**: Base class for all stream-based receivers

**Provides common functionality**:
- Stream heartbeat management (pause/resume)
- Active stream detection via `hasActiveStream()`
- Abstract `getStreamBase()` for derived classes to implement

```cpp
class StreamReceiver : public QObject {
public:
    void pauseHeartbeat();
    void resumeHeartbeat();
    [[nodiscard]] bool hasActiveStream() const;

protected:
    [[nodiscard]] virtual QPointer<Stream> getStreamBase() const = 0;
};
```

**Derived Classes**:
- `BarReceiver` — `getStreamBase()` returns `nullptr` (bars come via `LiveBarAccumulator`, not a Stream)
- `Level2Receiver` — `getStreamBase()` returns `nullptr` (data comes via `DBClient::newLevel2` signal)
- `Level1Receiver` — `getStreamBase()` returns `nullptr` (data comes via `DBClient::newLevel1` signal)
- `PositionsReceiver` — active, wraps `StreamPositions` from TSClient
- `OrdersReceiver` — active, wraps `StreamOrders` from TSClient

## Data Flow Patterns

### Level 2 Data Flow

```
DBClient thread → emit newLevel2(symbol, level2)
                    ↓ QueuedConnection
MainAlgo thread → onNewLevel2Received(symbol, level2)
                    → m_symbolContexts[symbol]->enqueueLevel2(level2)
                    ↓ QThreadPool::globalInstance()
Pool thread     → drain() → processLevel2()
                    → m_level2Receiver.onReceivedNewLevel2() [DirectConnection]
                    → emit receivedNewLevel2(...)
                    ↓ QueuedConnection (pool → MainAlgo thread)
MainAlgo thread → onDisplayedLevel2Received()
                    → emit displayedStockReceivedNewLevel2()
                    ↓ QueuedConnection (MainAlgo → GUI thread)
GUI thread      → update Level2Table
```

### Position Update Flow

```mermaid
sequenceDiagram
    participant Stream as StreamPositions (TSClient)
    participant Receiver as PositionsReceiver
    participant MainAlgo
    participant Frontend as GUIFrontend

    Stream->>Receiver: positionUpdate(Position)
    Receiver->>Receiver: Update position map, calculate P/L
    Receiver-->>MainAlgo: receivedNewPosition(account, position)
    MainAlgo-->>Frontend: receivedNewPosition(account, position)
    Frontend->>Frontend: Update PositionWidget
```

### Stock Selection Flow

```mermaid
sequenceDiagram
    participant Frontend as GUIFrontend
    participant MainAlgo
    participant Old as Old SymbolContext
    participant New as New SymbolContext

    Frontend->>MainAlgo: onSelectDisplayedStock(symbol)
    MainAlgo->>Old: disconnect bar/level2 signals
    MainAlgo->>Old: deleteLater()
    MainAlgo->>New: create SymbolContext(symbol)
    MainAlgo->>New: connect barReceiver.receivedNewBar → displayedStockReceivedNewBar
    MainAlgo->>New: connect level2Receiver.receivedNewLevel2 → displayedStockReceivedNewLevel2
```

## Threading Model

### Thread Boundaries

**MainAlgo thread** (routing + lightweight coordination):
- MainAlgo singleton
- StrategyManager
- PositionsReceiver, OrdersReceiver
- GUI throttle timer

**QThreadPool** (heavy per-symbol processing):
- SymbolContext::drain() runs on pool threads
- Level2Receiver, BarReceiver, LiveBarAccumulator, BarAggregator execute on pool thread via DirectConnection
- One pool thread per symbol at a time (sequential per symbol, parallel across symbols)

**Cross-thread communication** via signals:
```
TSClient thread  ─[QueuedConnection]→  MainAlgo thread  (orders, positions, accounts)
DBClient thread  ─[QueuedConnection]→  MainAlgo thread  (market data routing)
MainAlgo thread  ─[enqueue+pool]→      QThreadPool      (per-symbol processing)
Pool thread      ─[QueuedConnection]→  MainAlgo thread   (processed results)
MainAlgo thread  ─[QueuedConnection]→  Main/GUI thread   (UI updates)
```

### Thread Safety Assertions

Using ASSUME macros (not Q_ASSERT):
```cpp
void MainAlgo::onSelectDisplayedStock(const QString& symbol) {
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    // Method logic...
}
```

## Memory Management

### Composition Over Pointers

SymbolContext uses composition for its receivers and cache:
```cpp
// ✓ CORRECT - Direct member objects
class SymbolContext {
    BarCache barCache;
    BarReceiver barReceiver;
    Level2Receiver m_level2Receiver;
};

// ✗ WRONG - Unnecessary pointers
class SymbolContext {
    BarCache* barCache;
    Level2Receiver* m_level2Receiver;
};
```

### Qt Parent-Child Ownership

For receivers that are recreated (e.g., during replay transitions):
```cpp
PositionsReceiver* m_positionReceiver = nullptr;  // Qt parent-child (parent is 'this')
OrdersReceiver* m_orderReceiver = nullptr;        // Qt parent-child (parent is 'this')
```

### SymbolContext Management

```cpp
// Stored by symbol in map with QPointer for safe access
QMap<QString, QPointer<SymbolContext>> m_symbolContexts;

// Created on demand in onSelectDisplayedStock()
m_currentDisplayedSymbolContext = new SymbolContext(symbol, this);
m_symbolContexts.insert(symbol, m_currentDisplayedSymbolContext);

// Old instrument cleaned up via deleteLater() when switching symbols
oldInstrument->deleteLater();
```

## Balance Polling

Periodic balance updates via TSClient (brokerage API):
```cpp
void MainAlgo::startBalancePolling() {
    m_balancePollingTimer = std::make_unique<QTimer>(this);
    m_balancePollingTimer->setInterval(PollingConstants::BALANCE_POLLING_INTERVAL_MS);

    connect(m_balancePollingTimer.get(), &QTimer::timeout,
            this, &MainAlgo::requestBalance);

    m_balancePollingTimer->start();
}
```

**Polling interval**: Centralized in `Src/Misc/CONSTANTS.h` as `PollingConstants::BALANCE_POLLING_INTERVAL_MS`.

## Configuration

### Settings

Via QSettings:
```cpp
// Balance polling interval
Settings::getValue("MainAlgo/BalancePollingInterval", 5000);

// Default timeframe
Settings::getValue("MainAlgo/DefaultTimeframe", "1Min");

// Position tracking
Settings::getValue("MainAlgo/EnablePositionTracking", true);
```

## Error Handling

### API Request Failures

Handle async request failures (TSClient brokerage operations):
```cpp
QFuture<std::expected<QVector<Account>, TSClient::Error>> future =
    TSClient::getInstance()->getAccounts();

future.then(this, [this](std::expected<QVector<Account>, TSClient::Error> results) {
    if (!results.has_value()) {
        CRITICAL << "Failed to get accounts";
        return;
    }
    // Process accounts...
});
```

### Position Tracking Errors

Handle position stream issues:
```cpp
if (!m_positionReceiver->hasActiveStream()) {
    WARNING << "Position stream disconnected";
    // Reconnection logic...
}
```

## Replay Mode

MainAlgo coordinates replay mode for strategy testing using Databento `.dbn.zst` files. Replay playback logic lives in DBClient (not a separate ReplayEngine class). DBClient emits `newLevel2`/`newTrade` in replay mode — the same signals as live mode — enabling a unified routing pipeline.

### Entering Replay Mode

```cpp
void MainAlgo::enterReplayMode(QDate p_date, QTime p_startTime, Playback::Speed p_speed) {
    // 1. Forward DBClient replay control signals to MainAlgo signals for UI
    connect(DBClient::getInstance(), &DBClient::replayStarted, this, &MainAlgo::replayStarted);
    connect(DBClient::getInstance(), &DBClient::replayStopped, this, &MainAlgo::replayStopped);
    // ... (paused, resumed, timeUpdated, endReached)

    // 2. Wire OrderEmulator market depth feed
    connect(DBClient::getInstance(), &DBClient::newLevel2,
            orderEmulator, &OrderEmulator::updateMarketDepth);

    // 3. Start replay order/position streams with simulated account
    startReplayOrderStreams();

    // 4. Start replay (DBClient opens .dbn.zst files and begins timer-based emission)
    DBClient::getInstance()->startReplayPaused(symbol, p_date, p_startTime, p_speed);
}
```

Note: Level2/Trade routing to SymbolContext is handled automatically by the centralized routing established in `start()`. No separate replay wiring needed for data flow.

Replay risk-session nuance:

- replay risk runtime is tied to replay session ledger files
- on replay restart, `MainApp` rotates replay session storage and MainAlgo reloads a fresh replay risk state

### Exiting Replay Mode

When exiting replay mode back to live/simulation mode, `resumeLiveStreams()` performs critical cleanup:

```cpp
void MainAlgo::resumeLiveStreams() {
    // 1. Refetch real TradeStation accounts from API
    //    (m_activeAccount may still be "SIM123456" from replay)
    QFuture<std::expected<QVector<Account>, TSClient::Error>> future =
        TSClient::getInstance()->getAccounts();

    future.then(this, [this](auto results) {
        if (!results.has_value()) return;

        QVector<Account> accounts = results.value();
        m_activeAccount = accounts.first();
        emit tradeStationAccountsReceived(accounts);

        // 2. Delete and recreate receivers with real account
        delete m_positionReceiver;
        m_positionReceiver = new PositionsReceiver(m_activeAccount.getAccountId(), this);
        // Connect signals...

        delete m_orderReceiver;
        m_orderReceiver = new OrdersReceiver(m_activeAccount.getAccountId(), this);
        // Connect signals...
    });
}
```

**Critical**: Must refetch accounts before recreating receivers, otherwise API calls fail with ContentAccessDenied errors due to invalid account ID.

### startReplayOrderStreams()

Creates receivers for order/position tracking with simulated account:

```cpp
void MainAlgo::startReplayOrderStreams() {
    QString simAccountID = OrderEmulator::getSimulatedAccountID();  // "SIM123456"

    m_positionReceiver = new PositionsReceiver(simAccountID, this);
    // Connect signals...

    m_orderReceiver = new OrdersReceiver(simAccountID, this);
    // Connect signals...

    // Emit simulated account to update GUI account selector
    emit tradeStationAccountsReceived({simAccount});
}
```

### Order/Position Flow in Replay Mode

```
GUI/Strategy → placeOrder() → TSClient
    ↓ (Mode::Replay)
MockNetworkAccessManager
    ↓
OrderEmulator
    ↓ (reception delay 100-500ms)
emit OPN status
    ↓ (execution delay 10-50ms)
emit FLL status + position update
    ↓
MockNetworkReply::injectData()
    ↓
StreamOrders/StreamPositions
    ↓
OrdersReceiver/PositionsReceiver
    ↓
MainAlgo::onReceivedNewOrder/Position()
    ↓
GUI (OrderWidget, PositionWidget)
```

### Key Differences from Live Mode

| Aspect | Live Mode | Replay Mode |
|--------|-----------|-------------|
| Account | Real TradeStation account | `SIM123456` simulated |
| Orders | Real API (TSClient) | OrderEmulator |
| Positions | Real API (TSClient) | OrderEmulator |
| Market data | `DBClient::newLevel2` / `newTrade` (Databento live) | `DBClient::newLevel2` / `newTrade` (from `.dbn.zst` files) |
| Data pipeline | DBClient → MainAlgo → SymbolContext | Same — unified pipeline |
| Fills | Real market | Based on recorded depth |
| Latency | Real network | Simulated (100-500ms) |
| Balance | Real | $100,000 simulated |

See `Src/Core/Replay/OrderEmulator/AGENTS.md` for order emulation details.

## Related Agent Instructions

- `../Core/AGENTS.md`: MainApp orchestration
- `../Core/Cache/BarCache/AGENTS.md`: Bar caching details
- `../Clients/TSClient/AGENTS.md`: TSClient (brokerage-only: orders, positions, accounts)
- `../FrontEnd/AGENTS.md`: UI integration
- `../Strategy/AGENTS.md`: External strategy runtime

## Related Documentation

- `Doc/ARCHITECTURE.md`: System architecture and data flows
- `Doc/DEVELOPMENT.md`: Development guidelines
