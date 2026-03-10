# L2Trader Frontend Architecture

## Table of Contents

1. [Overview](#overview)
2. [Frontend Abstraction](#frontend-abstraction)
3. [GUI Implementation](#gui-implementation)
4. [TUI Implementation](#tui-implementation)
5. [Data Flow](#data-flow)

## Overview

L2Trader provides two frontend implementations:

- **GUI**: Full-featured Qt Widgets desktop application — charts, Level 2 order book, order entry, replay controls
- **TUI**: Lightweight ncurses terminal application for headless servers — order/position monitoring, no order entry

Both conform to a common `FrontEnd` abstract base class and run on the **main thread**.

## Frontend Abstraction

### FrontEnd Base Class

```cpp
class FrontEnd : public QObject {
    Q_OBJECT

signals:
    void selectedDisplayedStock(const QString& symbol);

public slots:
    virtual void onTSClientDataUsageUpdate(qsizetype bytes) = 0;
    virtual void onTradeStationAccountsReceived(const QVector<Account>& accounts) = 0;
    virtual void onMemoryUsageUpdate(qsizetype bytes) = 0;
    virtual void onCurrentHighlightedStockBarReceived(const QString& symbol, const Bar& bar) = 0;
    virtual void onCurrentHighlightedReceivedNewLevel2(const QString& symbol, const Level2& level2,
                                                       double dwp, double bidTotalVol, double askTotalVol) = 0;
    virtual void onCurrentHighlightedReceivedNewTrade(const QString& symbol, const Trade& trade) = 0;
    virtual void onNewPositionReceived(const QString& accountId, const Position& position) = 0;
    virtual void onPositionDeleted(const QString& accountId, const QString& positionId) = 0;
    virtual void onNewOrderReceived(const QString& accountId, const Order& order) = 0;
    virtual void onBalanceUpdated(const Balance& balance) = 0;
};
```

### Build-Time Selection

```cmake
option(ENABLE_GUI "Enable GUI frontend (Qt Widgets)" ON)
if(ENABLE_GUI)
    target_compile_definitions(L2Trader PRIVATE GUI_ENABLED)
endif()
```

```cpp
#ifdef GUI_ENABLED
    FrontEnd* frontend = new GUIFrontend(&mainAlgo);
#else
    FrontEnd* frontend = new TUIFrontend(&mainAlgo);
    static_cast<TUIFrontend*>(frontend)->initialize();
#endif
```

---

## GUI Implementation

**Location**: `Src/FrontEnd/GUI/`  
**Main class**: `GUIFrontend` (implements `FrontEnd`)  
**Framework**: Qt Widgets 6.x + QCustomPlot

### Main Window Layout

```
┌──────────────────────────────────────────────────────────────┐
│ Menu Bar: File | View | Tools | Help                         │
├──────────────────────────────────────────────────────────────┤
│ Toolbar: [Symbol Input] [Account] [Databento btn] [Data src] │
├──────────────────────────────────────────────────────────────┤
│           Tab Widget                                          │
│  ┌────────────────────────────────────────────────────────┐  │
│  │ Trade | Config | Cache | Logging | Strategies           │  │
│  │                                                         │  │
│  │  Trade Tab:                                             │  │
│  │  ┌──────────────────────┐  ┌───────────────────────┐  │  │
│  │  │   StockPriceChart    │  │  Level2Widget (10-lvl) │  │  │
│  │  │   (candlestick +     │  │  bid/ask order book   │  │  │
│  │  │    volume + overlays)│  └───────────────────────┘  │  │
│  │  │                      │  ┌───────────────────────┐  │  │
│  │  │  [ChartToolbar]      │  │  OrderEntryWidget     │  │  │
│  │  │  [replay controls]   │  │  StrategyQuickView    │  │  │
│  │  └──────────────────────┘  └───────────────────────┘  │  │
│  └────────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────┤
│ Status Bar: TS Data: X MB | Memory: Y MB                     │
├──────────────────────────────────────────────────────────────┤
│ Dock Widgets (bottom):                                        │
│  [Orders Table]  [Positions Table]  [Balance Display]        │
└──────────────────────────────────────────────────────────────┘
```

### MarketFlags Labels

Three always-visible status indicators in the top controls area:

| Label | Member | Active Color | Condition |
|-------|--------|-------------|-----------|
| **HALTED** | `m_haltedLabel` | Red `#CC0000` | `isHalted = true` in `newStatus` signal |
| **DELAYED** | `m_delayedLabel` | Yellow `#CCAA00` | Never active (Databento live = no delayed feed) |
| **HTB** | `m_hardToBorrowLabel` | Orange `#CC6600` | `isSsr = true` in `newStatus` signal |

Labels are always visible; when inactive they display in grey. They reset on symbol change.

**Gateway errors**: `DBClient::liveGatewayError(errorText, isFatal)` → `GUIFrontend::onDatabentoGatewayError()`. On a fatal error, a `QMessageBox` is shown and the Databento button turns amber.

### Key Components

#### StockPriceChart (`StockPriceChart/`)

Real-time candlestick chart using **QCustomPlot**.

**Features**:
- Live forming bar updated on every `Trade` record (via `LiveBarAccumulator::barUpdated`)
- Completed bars added on `LiveBarAccumulator::barClosed`
- Historical bar loading triggered by pan/zoom (gap detection → `requestMissingBars` signal)
- **Session backgrounds**: pre-market (orange), regular hours, after-hours (violet)
- Volume bars on a separate axis
- Last-price line with dynamic label
- **Order visualization**: buy markers (green ▲), sell markers (red ▼), position lines, unrealized P&L box

**Bidirectional Index System**:
```
Historical ← | Origin (index 0) | → Live
      -3  -2  -1   0   1   2   3   4
```
Benefits: O(m) historical insertion, no full rebuild, stable existing indices.

**Interaction**:
- `Ctrl+Shift+Scroll` — Vertical pan
- `Alt+Scroll` — Horizontal pan
- `Ctrl+Scroll` — Horizontal zoom
- `Shift+Scroll` — Vertical zoom
- `Scroll` — Both axes zoom
- `Right Click` — Reset to last 30 bars

**ChartToolbar**: Contains symbol display, timeframe selector, auto-TF checkbox, replay play/pause button, speed selector, and order-visualization toggle.

#### Dynamic Timescale

The chart supports seamless switching between multiple timeframes:

**Available Timeframes** (keyboard shortcuts):
- `1` = 1-minute (default)
- `2` = 5-minute
- `3` = 15-minute
- `4` = 30-minute
- `5` = 1-hour
- `6` = 4-hour
- `7` = 1-day (future)
- `8` = 1-week (future)
- `9` = 1-month (future)

**Auto-Timeframe Switching**: When the "Auto" checkbox is enabled, the chart automatically switches timeframes based on the visible time range:
- Zooming in (less time visible) → smaller timeframes
- Zooming out (more time visible) → larger timeframes
- Thresholds configurable in Config tab

**Range Preservation**: Both X and Y axis ranges are preserved when switching timeframes manually or automatically, preventing jarring view changes.

**Candle Alignment**: Candle left edges align with bar open time (a 13:00 bar spans 13:00-13:05 for 5m TF).

**Replay Start Time Granularity**: When switching timeframes in replay mode, the start time selector steps align with the active timeframe (5m TF → steps by 5 minutes, 1h TF → steps by 1 hour).

#### Level2Widget (`Widgets/Level2/`)

10-level bid/ask order book display, updated on every `DBClient::newLevel2` signal.

```
Bid Price | Bid Size | Count  ||  Ask Price | Ask Size | Count
-------------------------------------------------------------------
 $199.95  |   1,500  |   5   ||   $200.05  |   2,000  |   8
 $199.90  |   2,200  |   9   ||   $200.10  |   1,800  |   6
 ...      |   ...    |  ...  ||   ...      |   ...    |  ...
```

#### OrderEntryWidget (`Widgets/OrderEntry/`)

Order placement panel.

**Fields**: Symbol (auto-filled from chart), Buy/Sell toggle, order type (Market/Limit/Stop/Stop-Limit), quantity, limit price, time-in-force.

**Sticky Price Feature**: The limit price field can "stick" to the last-traded price, automatically updating as trades arrive, until the user manually edits it.

**Signal**: Emits `orderPlaced(PlaceOrderRequest)` on submit. In replay mode, the order goes to `OrderEmulator` instead of the real API.

#### StrategyQuickView (`Widgets/StrategyQuickView/`)

Tree widget showing all loaded strategies and the symbol each has claimed. Provides quick visibility into strategy activity without opening the Strategies tab.

#### Tabs

| Tab | Class | Purpose |
|-----|-------|---------|
| **Config** | `ConfigTab` | Databento API key entry, dataset selection, connection management |
| **Cache** | `CacheTab` | Bar cache inspection, clear, preload controls |
| **Logging** | `LoggingTab` | Live log display with per-category filter controls |
| **Strategies** | `StrategiesTab` | Load/start/stop strategy plugins; `StrategyCard` per plugin |

#### Dock Widgets

| Widget | Class | Description |
|--------|-------|-------------|
| Orders | `OrderWidget` | All orders with status; right-click to cancel |
| Positions | `PositionWidget` | Open positions with unrealized P&L |
| Balance | `BalanceWidget` | Account cash and buying power |

---

## TUI Implementation

**Location**: `Src/FrontEnd/TUI/`  
**Framework**: ncurses + Qt Core  
**Use case**: Headless servers, minimal resource footprint

### Features

- Window-based layout: orders pane, positions pane, status bar
- Keyboard navigation (arrow keys, Enter, `q` to quit)
- Color-coded P&L values
- Real-time order and position updates
- Memory and network usage display in status bar
- Logs written to `stderr` (keeps ncurses display clean)

### Limitations

- No order entry — monitoring only
- No chart or Level 2 display
- No replay controls
- No strategy management UI

### Running TUI Mode

```bash
./build/TUI/Src/L2Trader 2>app.log   # ncurses on stdout, logs on stderr
```

---

## Data Flow

### Backend → Frontend (Live Mode)

```
DBClient (Databento callback thread)
    │
    │  newLevel2(symbol, level2)
    ├──────────────────────────────► MainAlgo (queued)
    │                                    │
    │                                    │  displayedStockReceivedNewLevel2
    │                                    └────────────────────────────────► GUIFrontend (main thread)
    │                                                                            │
    │                                                                            └► Level2Widget.update()
    │
    │  newTrade(symbol, trade)
    ├──────────────────────────────► MainAlgo → LiveBarAccumulator
    │                                    │
    │                                    │  barUpdated / barClosed
    │                                    └────────────────────────────────► GUIFrontend
    │                                                                            │
    │                                                                            └► StockPriceChart.addLiveBar()
    │
    │  newStatus(symbol, isHalted, haltReason, isSsr)
    └──────────────────────────────► GUIFrontend.onDatabentoStatusUpdate()
                                         │
                                         └► MarketFlags labels (HALTED / HTB)
```

### Frontend → Backend (User Actions)

```
User changes symbol
    └► GUIFrontend::onNewDisplayedStockSelection()
           └► emit selectedDisplayedStock(symbol)
                  └► MainAlgo::onSelectDisplayedStock() [queued]
                         └► DBClient::subscribeLive(symbol)

User places order
    └► OrderEntryWidget::onSubmitClicked()
           └► emit orderPlaced(request)
                  └► GUIFrontend::onOrderPlaced()
                         └► MainAlgo::placeOrder(request) [queued]
                                └► TSClient::placeOrder() [or OrderEmulator in replay]

User clicks Play in replay
    └► ChartToolbar::replayPlayPauseToggled(true)
           └► MainApp::resumeReplayPlayback()
                  └► MainAlgo::resumeReplay() [BlockingQueuedConnection]
                         └► ReplayEngine::resumeReplay()
```

---

## Related Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) — System architecture, threading, component roles
- [DEVELOPMENT.md](DEVELOPMENT.md) — Build setup and coding guidelines
- [STRATEGY.md](STRATEGY.md) — Strategy plugin UI (StrategiesTab, StrategyCard)
