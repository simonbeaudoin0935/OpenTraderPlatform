# Algo/ Directory - Trading Algorithm Components - Agent Instructions

The Algo directory contains the trading algorithm coordination logic and various receivers for processing market data and trading events. TSClient is brokerage-only (orders, positions, accounts). Market data (Level 2, trades, bars) flows from DBClient in live mode and from ReplayEngine in replay mode.

## Overview

**Location**: `Src/Algo/`
**Purpose**: Trading algorithm coordination, data reception, and processing
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
   - Create and manage StockInstruments instances (one per symbol)
   - Track currently displayed stock via `currentDisplayedStockInstrument`
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

5. **Market Data Forwarding**
   - Connect Level2Receiver signals to `displayedStockReceivedNewLevel2` for the displayed stock
   - Connect BarReceiver signals to `displayedStockReceivedNewBar` for the displayed stock
   - Forward raw Level2 data (DWP/BAI computation removed — belongs in individual strategies)

6. **Replay Coordination**
   - Create and manage ReplayEngine
   - Forward replay control signals (start, stop, pause, resume) to UI
   - Manage replay order/position streams with simulated account

7. **Strategy Management**
   - Own and run StrategyManager for loaded strategy plugins

### Key Members

```cpp
class MainAlgo {
    QThread thread;

    // Per-symbol instruments
    QMap<QString, QPointer<StockInstruments>> stockInstruments;
    QPointer<StockInstruments> currentDisplayedStockInstrument;

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

    // Replay engine (owned, runs in MainAlgo thread)
    ReplayEngine* m_replayEngine = nullptr;

    // Crash monitoring
    std::unique_ptr<QSocketNotifier> m_crashNotifier;
};
```

### Signal Interface

**Signals emitted by MainAlgo**:
```cpp
signals:
    // Market data for displayed stock
    void displayedStockReceivedNewBar(QString symbol, Bar bar);
    void displayedStockReceivedNewLevel2(QString symbol,
                                         Level2 level2,
);  // Simplified: raw Level2 only; strategies compute their own metrics

    // Trading events
    void receivedNewPosition(QString account, Position position);
    void positionDeleted(QString account, QString positionID);
    void receivedNewOrder(QString account, Order order);

    // Account and balance
    void tradeStationAccountsReceived(QVector<Account> accounts);
    void balanceUpdated(Balance balance);

    // Replay control (forwarded from ReplayEngine)
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

## StockInstruments

**Role**: Per-symbol data container and processor

**Composition Pattern** (preferred over pointers):
```cpp
class StockInstruments : public QObject {
public:
    explicit StockInstruments(const QString& p_symbol, QObject* p_parent = nullptr);
    ~StockInstruments();

    QString symbol;
    BarCache barCache;
    BarReceiver barReceiver;
    Level2Receiver m_level2Receiver;
};
```

**Lifecycle**:
- Created when symbol first selected via `onSelectDisplayedStock()`
- Previous instrument is cleaned up (`deleteLater()`) when a different symbol is selected
- Only one StockInstruments exists at a time (the displayed stock)
- Manages its own bar cache, bar receiver, and Level 2 receiver

**Responsibilities**:
- Manage BarCache for symbol
- Own Level2Receiver that computes bid-ask imbalance and depth-weighted prices
- Own BarReceiver that processes bars built by `LiveBarAccumulator`

## Receiver Components

### BarReceiver/

**Role**: Receive and process bar data

`BarReceiver` processes bars produced by `LiveBarAccumulator`. In live mode, bars come from `DBClient::newTrade` → `LiveBarAccumulator`. In replay mode, bars come from `ReplayEngine::replayTrade` → `LiveBarAccumulator`.

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

Historical bars are loaded into BarCache via REST API requests (TSClient historical endpoint).

### Level2Receiver/

**Role**: Process Level 2 market depth data (10-level book snapshots)

Replaces the former `MarketDepthQuoteReceiver/`. Uses the `Level2` model (from `Src/Core/Models/Level2.h`) which contains `std::array<Level2Row, 10>` for bids and asks. Connected to `DBClient::newLevel2` in live mode and `ReplayEngine::replayLevel2` in replay mode.

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

**Data Flow**:
```
Live mode:
DBClient::newLevel2 (Databento Schema::Mbp10)
    ↓
Level2Receiver::onReceivedNewLevel2()
    ↓ (raw forward, no computation)
    ↓ emit receivedNewLevel2(...)
MainAlgo (forwarded to displayedStockReceivedNewLevel2)
    ↓
GUIFrontend → Level2Table display

Replay mode:
ReplayEngine::replayLevel2
    ↓ (same slot and processing)
Level2Receiver::onReceivedNewLevel2()
    ...
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

```mermaid
sequenceDiagram
    participant DBClient as DBClient (live) / ReplayEngine (replay)
    participant L2R as Level2Receiver
    participant MainAlgo
    participant Frontend as GUIFrontend

    DBClient->>L2R: onReceivedNewLevel2(Level2)
    L2R->>L2R: Calculate bidAskImbalance, bidDWP, askDWP
    L2R-->>MainAlgo: receivedNewLevel2(symbol, level2, imbalance, bidDWP, askDWP)
    MainAlgo-->>Frontend: displayedStockReceivedNewLevel2(...)
    Frontend->>Frontend: Update Level2Table
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
    participant Old as Old StockInstruments
    participant New as New StockInstruments

    Frontend->>MainAlgo: onSelectDisplayedStock(symbol)
    MainAlgo->>Old: disconnect bar/level2 signals
    MainAlgo->>Old: deleteLater()
    MainAlgo->>New: create StockInstruments(symbol)
    MainAlgo->>New: connect barReceiver.receivedNewBar → displayedStockReceivedNewBar
    MainAlgo->>New: connect level2Receiver.receivedNewLevel2 → displayedStockReceivedNewLevel2
```

## Threading Model

### Thread Boundaries

All Algo components run on MainAlgo thread:
- MainAlgo
- StockInstruments instances
- All receivers (Level2Receiver, Level1Receiver, BarReceiver, PositionsReceiver, OrdersReceiver)
- ReplayEngine
- StrategyManager

**Cross-thread communication** via signals:
```
TSClient thread ─[signal]→ MainAlgo thread   (orders, positions, accounts)
DBClient thread ─[signal]→ MainAlgo thread   (market data: Level2, Level1, Trade)
MainAlgo thread ─[signal]→ Main/GUI thread   (UI updates)
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

StockInstruments uses composition for its receivers and cache:
```cpp
// ✓ CORRECT - Direct member objects
class StockInstruments {
    BarCache barCache;
    BarReceiver barReceiver;
    Level2Receiver m_level2Receiver;
};

// ✗ WRONG - Unnecessary pointers
class StockInstruments {
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

### StockInstruments Management

```cpp
// Stored by symbol in map with QPointer for safe access
QMap<QString, QPointer<StockInstruments>> stockInstruments;

// Created on demand in onSelectDisplayedStock()
currentDisplayedStockInstrument = new StockInstruments(symbol, this);
stockInstruments.insert(symbol, currentDisplayedStockInstrument);

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

MainAlgo coordinates replay mode for strategy testing using Databento `.dbn.zst` files via `ReplayEngine`.

### Entering Replay Mode

```cpp
void MainAlgo::enterReplayMode(QDate p_date, QTime p_startTime, ReplayEngine::PlaybackSpeed p_speed) {
    // 1. Create ReplayEngine (lazy init, parent=this for thread affinity)
    m_replayEngine = new ReplayEngine(this, TSClient::getInstance());

    // 2. Forward replay control signals to MainAlgo signals for UI
    connect(m_replayEngine, &ReplayEngine::replayStarted, this, &MainAlgo::replayStarted);
    connect(m_replayEngine, &ReplayEngine::replayStopped, this, &MainAlgo::replayStopped);
    // ... (paused, resumed, timeUpdated, endReached)

    // 3. Wire market data signals to receivers (connectReplaySignals)
    connect(m_replayEngine, &ReplayEngine::replayLevel2, level2Receiver, &Level2Receiver::onReceivedNewLevel2);
    connect(m_replayEngine, &ReplayEngine::replayTrade, m_liveBarAccumulator, &LiveBarAccumulator::onNewTrade);

    // 4. Start replay order/position streams with simulated account
    startReplayOrderStreams();

    // 5. Start replay
    m_replayEngine->startReplay(p_date, p_startTime, p_speed);
}
```

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
| Market data | `DBClient::newLevel2` / `newTrade` (live) | Recorded `.dbn.zst` files (ReplayEngine) |
| Fills | Real market | Based on recorded depth |
| Latency | Real network | Simulated (100-500ms) |
| Balance | Real | $100,000 simulated |

See `Src/Core/Replay/OrderEmulator/AGENTS.md` for order emulation details.

## Related Agent Instructions

- `../Core/AGENTS.md`: MainApp orchestration
- `../Core/Cache/BarCache/AGENTS.md`: Bar caching details
- `../Clients/TSClient/AGENTS.md`: TSClient (brokerage-only: orders, positions, accounts)
- `../FrontEnd/AGENTS.md`: UI integration
- `../Strategy/AGENTS.md`: Strategy plugin system

## Related Documentation

- `Doc/ARCHITECTURE.md`: System architecture and data flows
- `Doc/DEVELOPMENT.md`: Development guidelines
