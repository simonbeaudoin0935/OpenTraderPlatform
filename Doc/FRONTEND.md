# OpenTraderPlatform Frontend Architecture

## Table of Contents

1. [Overview](#overview)
2. [GUI Implementation](#gui-implementation)
3. [Data Flow](#data-flow)

## Overview

OpenTraderPlatform now uses a single frontend implementation:

- **GUIFrontend**: Qt Widgets desktop application — charts, Level 2 order book, order entry, and replay controls.

`GUIFrontend` runs on the main thread.

Startup session restoration is enabled by `MainApp::start()` only after credential
unlocking completes (including cancellation or failure) and all worker threads
have been started. Authentication then triggers a one-time restoration, with a
500 ms fallback for sessions without authentication. Secondary chart windows are
restored after 600 ms. These timers are not armed during frontend construction:
the YubiKey prompt runs a nested event loop, where restoring replay prematurely
would block the GUI waiting for a worker thread that has not started.

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

### Visual Theme

Order-entry settings offer an **Enable Success Popup** toggle, persisted under
`OrderEntry/ResultPopupEnabled`. It controls successful placement notifications
only; order failures, including risk rejections, always display an error dialog.

Live TradeStation stream creation executes directly on the client thread and
blocks only callers on other threads. Market-depth capacity checks and queued
requests are serialized on that thread. Per-symbol bars, quotes, and depth
subscriptions retain at most one pending retry or queued depth request per kind.
Transient failures retry with exponential delays from 1 to 30 seconds, reset by
valid market data. Bad-request/invalid-symbol and forbidden failures disable
automatic retries for that subscription until its symbol context is recreated.
Final stream error bodies are parsed before closure, including responses without
a trailing newline; repeated completion callbacks emit only one error closure.

The Qt Widgets interface uses a VS Code-inspired dark palette while retaining the
existing trading workspace and panel arrangement. Shared surface, text, border,
selection, and accent colors are defined in `GUIThemeConstants` in
`Src/Misc/CONSTANTS.h` and applied by `GUIFrontend::setupDarkTheme`.
The interface prefers Segoe UI with Lato/Noto Sans fallbacks; log panes prefer
Cascadia Code and Consolas with a platform monospace fallback.

Market and trading cues (such as bid/ask, buy/sell, risk state, and session
backgrounds) remain semantically color-coded; the shared theme styles the
surrounding workspace, controls, tables, and focus/selection states.

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
- Last-price line with sticky border label when price moves off-screen
- **Order visualization**: buy markers (green ▲), sell markers (red ▼), position lines, unrealized P&L box, closed-position P&L labels, and strategy bracket overlays (STOP/TAKE)
- **BBO overlay guides**: bid/ask lines originate at the replay/live time line and terminate at the bid/ask label anchors

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

MACD and RSI subpanes retain their manually adjusted vertical range when new
bars arrive or the forming bar changes. MACD continues automatic scaling until
its range is adjusted; RSI initially uses 0–100. Hiding and showing an indicator
preserves the adjustment. Clearing the chart for a new symbol or replay session
restores automatic range initialization.

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

There is no 10-second timeframe keyboard shortcut. Saved unmodified legacy
number-key layouts are migrated to the mapping above; customized bindings are
preserved. The same default number keys apply to detached charts.

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

Tree widget showing all loaded strategies and the symbol each has claimed. Provides quick visibility and control directly from the Trade tab.

#### Tabs and Embedded Strategy Tools

| Tab | Class | Purpose |
|-----|-------|---------|
| **Trade (strategy tools)** | `StrategyQuickView` + `StrategyLoadDialog` | Load/start/stop external strategy processes and open per-strategy logs |
| **Config** | `ConfigTab` | Databento API key entry, dataset selection, connection management |
| **Cache** | `CacheTab` | Bar cache inspection, clear, preload controls |
| **Logging** | `LoggingTab` | Live log display with per-category filter controls |

#### Dock Widgets

| Widget | Class | Description |
|--------|-------|-------------|
| Orders | `OrderWidget` | All orders with status; right-click to cancel |
| Positions | `PositionWidget` | Open positions with unrealized P&L |
| Balance | `BalanceWidget` | Account cash and buying power |

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
- [STRATEGY.md](STRATEGY.md) — Strategy runtime UI (`StrategyQuickView`, `StrategyLoadDialog`, `StrategyLogWidget`)
