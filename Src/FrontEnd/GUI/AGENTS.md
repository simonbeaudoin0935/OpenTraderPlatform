# GUI/ - Graphical User Interface - Agent Instructions

The GUI directory contains the full-featured Qt Widgets desktop interface for OpenTraderPlatform.

## Overview

**Location**: `Src/FrontEnd/GUI/`
**Framework**: Qt Widgets 6.x
**Main Class**: GUIFrontend (QObject-based GUI frontend)
**Key Features**: Real-time charts, market depth, order entry, position tracking

For complete architectural details, see `Doc/FRONTEND.md`.

## Directory Organization

The GUI folder is organized into logical subfolders:

```
Src/FrontEnd/GUI/
├── AGENTS.md (this file)
├── GUIFrontend.cpp/h/ui (main window)
├── Tabs/                           # Tab components
│   ├── AGENTS.md
│   ├── RiskTab.cpp/h               # Risk limits configuration + runtime controls
│   ├── CacheTab.cpp/h              # Bar cache management
│   ├── ConfigTab.cpp/h             # Application settings
│   ├── LoggingTab.cpp/h            # Live log display
│   └── ShortcutsTab.cpp/h          # Keyboard shortcuts
├── Widgets/                        # Reusable widget components
│   ├── AGENTS.md
│   ├── OrderEntry/                 # Order placement widget
│   │   ├── AGENTS.md
│   │   └── OrderEntryWidget.cpp/h
│   ├── Level2/                     # Level 2 market depth display (was MarketDepth/)
│   │   ├── AGENTS.md
│   │   ├── Level2Widget.cpp/h      # Main widget (was MarketDepthTable)
│   │   └── Level2TableView.cpp/h   # Custom table view (was MarketDepthTableView)
│   ├── StrategyQuickView/          # Live strategy + claimed symbol tree (in Trade tab)
│   │   └── StrategyQuickView.cpp/h
│   ├── RiskStatus/                 # Compact risk drawdown headroom widget
│   │   └── RiskStatusWidget.cpp/h
│   └── StrategyLogWidget/          # Per-strategy log display (bottom splitter panel)
│       └── StrategyLogWidget.cpp/h
├── Widgets/                        # Window-style displays
│   ├── AGENTS.md
│   ├── OrderWidget.cpp/h           # Orders table
│   ├── PositionWidget.cpp/h        # Positions table
│   └── BalanceWidget.cpp/h         # Account balance
└── StockPriceChart/                # Real-time price chart
    ├── AGENTS.md
    ├── StockPriceChart.cpp/h
    ├── ChartToolbar.cpp/h
    └── ... (chart components)
```

**Navigation**: Each subfolder contains its own AGENTS.md with detailed documentation. See:
- `Tabs/AGENTS.md` - tab components including `RiskTab` and tab order
- `Widgets/AGENTS.md` - Reusable widget components overview
- `Widgets/OrderEntry/AGENTS.md` - Order entry widget with sticky price
- `Widgets/Level2/AGENTS.md` - Level 2 market depth display (Level2Widget)
- `Widgets/AGENTS.md` - Order/Position/Balance window displays
- `StockPriceChart/AGENTS.md` - Real-time chart with replay mode

## Main Window Structure

### GUIFrontend (GUIFrontend.ui/h/cpp)

**Qt Designer UI**: `GUIFrontend.ui` defines the layout
**Generated**: `ui_GUIFrontend.h` (auto-generated, don't edit)
**Implementation**: `GUIFrontend.h/cpp`

**Window Layout**:
```
┌──────────────────────────────────────────┐
│ Menu Bar: File | View | Tools | Help     │
├──────────────────────────────────────────┤
│ Toolbar: [Symbol Input] [Account Select] │
├──────────────────────────────────────────┤
│                                          │
│         Tab Widget (main area)           │
│  ┌────────────────────────────────────┐  │
│  │ • Trade Tab                        │  │
│  │ • Risk Management Tab              │  │
│  │ • Other tabs (Records/Logging/...) │  │
│  └────────────────────────────────────┘  │
│                                          │
├──────────────────────────────────────────┤
│ Status Bar: Data: X MB | Memory: Y MB    │
├──────────────────────────────────────────┤
│ Dock Widgets (bottom):                   │
│ • Order Window                           │
│ • Position Window                        │
│ • Balance Window                         │
└──────────────────────────────────────────┘
```

### MarketFlags Labels (top controls area)

Three always-visible QLabel indicators for Level 1 market status flags sit in the `topControlsLayout` at indices 5-7:

| Label | Member | Active Color | Active Text |
|-------|--------|-------------|------------|
| Halted | `m_haltedLabel` | Red (`#CC0000`) | `HALTED` |
| Delayed | `m_delayedLabel` | Yellow (`#CCAA00`) | `DELAYED` |
| Hard to Borrow | `m_hardToBorrowLabel` | Orange (`#CC6600`) | `HTB` |

**Behavior**: Labels are **always visible** (never hidden). When the flag is inactive, they display in grey with a dimmed style. When active, they switch to their colored active style.

**Implementation**: Connected to `DBClient::newStatus(symbol, isHalted, haltReason, isSsr)` signal. `GUIFrontend::onDatabentoStatusUpdate()` handles updates: HALTED sets the halted label red, HTB (isSsr) sets the HTB label orange. Labels reset on symbol change.

**DELAYED label**: Intentionally always inactive — Databento has no "delayed feed" concept for live subscriptions, so this label is kept in the layout but never driven.

**Gateway errors**: `DBClient::liveGatewayError(errorText, isFatal)` → `GUIFrontend::onDatabentoGatewayError()`. On a fatal error, a `QMessageBox` is shown to the user and the Databento connection button changes to amber with text "Databento: Subscription Error". Fatal error codes: AuthFailed=1, ApiKeyDeactivated=2, ConnectionLimitExceeded=3, InvalidSubscription=5.

**BATS flag**: Intentionally not displayed (removed by design — the IsBats flag is not user-relevant).

### Risk Management Surfaces

- **Tab**: `Risk Management` (second tab, immediately after Trade)
  - backed by `Tabs/RiskTab.*`
  - account-scoped risk config editing (drawdown basis, per-trade loss, position and trade-count limits, cooldown)
  - runtime actions: day reset and unlock trading
- **Status widget**: `RiskStatusWidget` (`Widgets/RiskStatus/`)
  - compact drawdown headroom/lock state summary
  - placed above Time & Sales in the right-side panel stack
  - receives updates from `MainAlgo::riskStatusChanged(accountId)`

### Strategy Confirmation Mute UX

Keyboard shortcut `M` toggles hard mute mode for strategy user-confirm orders:

- when enabled, active and queued confirmations are rejected and future confirmations are auto-rejected
- chart focus steal and flashing alert cues are suppressed
- chart watermark `Muted` is displayed while active
- pressing `M` again un-mutes and resumes normal confirmation flow


## Key Components

### StockPriceChart/ (Subdirectory)

**Real-time candlestick chart using QCustomPlot**

Key files:
- `StockPriceChart.h/cpp`: Main chart widget
- `StockPriceChart.ui`: Qt Designer layout
- `ChartToolbar.h/cpp`: Toolbar with controls
- `AGENTS.md`: Detailed component documentation

**Features**:
- Live candlestick updates
- Historical bar loading with bidirectional indexing
- Session backgrounds (pre-market orange, after-hours violet)
- Volume bars on separate axis
- Last price line with sticky border label behavior
- Missing bar detection and auto-request
- Mouse interactions (pan, zoom, reset)
- **Order visualization**: Buy/sell markers, position lines, P&L display, strategy STOP/TAKE bracket overlays
- **BBO overlay guides**: Bid/ask lines from timeline to bid/ask label anchors
- **Replay mode**: Play back historical market data
- **Replay speeds**: includes `1x`, `2x`, `5x`, `10x`, `25x`, `50x`, `100x`
- **Replay controls contract**: Day/time selection are session-global pre-play settings; once replay has started they stay locked until `Restart`

**Order Visualization Feature**:
- Buy markers: Green upward triangles at fill price/time
- Sell markers: Red downward triangles at fill price/time
- Position lines: Dotted green/red lines connecting entries to exits
- P&L box: Real-time unrealized P&L for open positions
- P&L labels: Realized P&L for closed positions
- Toolbar toggle to show/hide visualizations

See `StockPriceChart/AGENTS.md` for detailed order visualization documentation.

**Bidirectional Index System**:
```
Historical ← | Origin (index 0) | → Live/Future
       -3  -2  -1    0    1   2   3   4
```

Benefits:
- O(m) insertion for m historical bars
- No full rebuild on history load
- Preserves existing indices

**Interaction Controls**:
- Ctrl+Shift+Scroll: Vertical pan
- Alt+Scroll: Horizontal pan
- Ctrl+Scroll: Horizontal zoom
- Shift+Scroll: Vertical zoom
- Scroll: Both axes zoom
- Right Click: Reset to last 30 bars

**Signal/Slot Interface**:
```cpp
signals:
    void requestMissingBars(const QDateTime& start, const QDateTime& end);

public slots:
    void addLiveBar(const QString& symbol, const Bar& bar);
    void onRequestedMissingBarsReceived(const QVector<Bar>& bars);
    void setSymbol(const QString& symbol);
    // Order visualization slots
    void onOrderPlaced(const Order& order);
    void onOrderFilled(const Order& order);
    void onOrderCancelled(const Order& order);
    void onPositionUpdated(const Position& position);
    void onPositionClosed(const Position& position);
    void onStrategyBracketOverlayEmitted(const StrategyBracketOverlayEntry& entry);
```

### Level2Widget (Widgets/Level2/)

**Level 2 market depth display**

See `Widgets/Level2/AGENTS.md` for complete documentation.

Layout:
```
┌─────────────────────────────────────────────────┐
│ Bid Price | Bid Size | Bid Count || Ask Price | Ask Size | Ask Count │
├─────────────────────────────────────────────────┤
│  $99.95   |  1,500   |    5      ||  $100.05  |  2,000   |    8      │
│  $99.90   |  2,200   |    9      ||  $100.10  |  1,800   |    6      │
│  $99.85   |  1,000   |    3      ||  $100.15  |  3,500   |   12      │
└─────────────────────────────────────────────────┘
```

Features:
- Color-coded bid (green) and ask (red) sides
- Bold best bid/ask
- DWP (Depth-Weighted Price) indicator
- Spread calculation
- Real-time updates (throttled to 100ms)

**Custom View**: `Level2TableView` handles formatting and color coding

### OrderEntryWidget (Widgets/OrderEntry/)

**Order placement interface**

See `Widgets/OrderEntry/AGENTS.md` for complete documentation including sticky price feature.

Fields:
- Account selection (dropdown)
- Symbol input (auto-filled from chart)
- Trade action (Buy/Sell/BuyToCover/SellShort radio buttons)
- Order type (Market/Limit/Stop/StopLimit dropdown)
- Quantity (spinner)
- Limit price (visible for Limit/StopLimit orders)
- Time in force (Day/GTC dropdown)

**Sticky Price Feature**:
- Checkbox to enable/disable
- Aggressive/Passive mode radio buttons
- Price offset (0.00 to 10.00)
- Auto-updates limit price based on market depth
- Visual green flash on update

**Sticky Modes**:
- **Aggressive** (crosses spread for guaranteed fill):
  - Buy: limit = best ask + offset
  - Sell: limit = best bid - offset
- **Passive** (enters on bid/ask, better price but may not fill):
  - Buy: limit = best bid + offset
  - Sell: limit = best ask - offset

**Validation**:
- All required fields must be filled
- Limit price required for Limit/StopLimit
- Quantity must be > 0
- Prevents negative prices (min $0.01)
- In replay mode, order entry is blocked unless replay playback state is `Playing`
- Arm stop/take **percentage** controls were removed from the visible layout to reduce vertical footprint

**Signal Interface**:
```cpp
signals:
    void orderPlaced(const PlaceOrderRequest& request);

public slots:
    void setSymbol(const QString& symbol);
    void updateStickyPrice(const Level2& level2);
```

### StrategyQuickView replay states

`StrategyQuickView` now needs to distinguish more than just "running vs not running" for replay:

- **Grey** — loaded/stopped (not armed for replay start)
- **Yellow** — replay strategy is primed for the first Play, or already started but currently paused with replay
- **Green** — strategy is actively executing while replay/live data is advancing

Important replay nuance:

- Manual **Start** during replay preload should arm/prime the strategy instead of launching it immediately.
- On the first Play press, the platform starts all primed strategies first, then replay begins only after they report ready.
- Replay Restart returns previously active replay strategies to the yellow primed state for the fresh session.

### PositionWidget (Widgets/)

**Real-time position tracking**

See `Widgets/AGENTS.md` for complete documentation.

Columns:
- Symbol (clickable to load chart)
- Quantity
- Average Price
- Last Price
- P/L (dollars, color-coded)
- P/L % (color-coded)
- Market Value

**Color Coding**:
- Green: Profitable (P/L > 0)
- Red: Loss (P/L < 0)
- White: Break-even (P/L = 0)

**Features**:
- Click symbol to auto-load in chart
- Right-click context menu (future: quick close)
- Sort by any column
- Real-time price and P/L updates

**Data Model**: QStandardItemModel with custom delegates

**Tracking**:
```cpp
QMap<QString, int> m_positionRowMap;  // positionId → row index
```

### OrderWidget (Widgets/)

**Order management and status tracking**

See `Widgets/AGENTS.md` for complete documentation.

Columns:
- Order ID
- Symbol (clickable to load chart)
- Action (BUY, SELL, SHORT, COVER) — note: "SELLSHORT" displayed as "SHORT", "BUYTOCOVER" as "COVER"
- Quantity
- Type (Market, Limit, etc.)
- Price
- DateTime
- Status (ACK, Open, Filled, Canceled, Rejected)

**Features**:
- Click symbol to auto-load in chart
- Right-click to cancel open orders
- Color-coded status
- Sort and filter by any column

**Order Tracking**:
```cpp
QMap<QString, int> m_orderRowMap;  // orderId → row index
```

### BalanceWidget (Widgets/)

**Account balance display**

See `Widgets/AGENTS.md` for complete documentation.

Shows:
- Cash Available
- Buying Power
- Equity
- Margin Used
- Account Value
- Day P/L

**Update Frequency**: Configurable (default 5 seconds)

### Tabs/ (Subdirectory)

See `Tabs/AGENTS.md` for complete documentation of all tab components.

#### CacheTab (CacheTab.h/cpp)

**Bar cache management interface**

Features:
- View cache status per symbol
- Display cache size and bar count
- Clear cache (selected symbol or all)
- View disk usage
- Database path display

#### LoggingTab (LoggingTab.h/cpp)

**Live log display and controls**

Features:
- Real-time log message display (QTextEdit)
- Category filter checkboxes
- Log level dropdown (Debug, Info, Warning, Critical)
- Auto-scroll toggle
- Clear logs button
- Save logs to file

**Log Categories** (dynamically generated from code):
- TSClient.*
- MainAlgo.*
- BarCache.*
- GUI.*
- ... and more

#### RecorderTab — REMOVED

The RecorderTab was removed in Phase 3 of the Databento migration. Recording is done via Databento `.dbn` archive downloads.

### StrategyQuickView (Widgets/StrategyQuickView/)

**Live collapsible tree of active strategies and their claimed symbols — embedded in the Trade tab.**

**Columns** (5):
| Col | Header | Content |
|-----|--------|---------|
| 0 | Strategy / Symbol | Strategy name (root) or symbol (child); elastic stretch |
| 1 | Qty | Strategy-owned open quantity for symbol rows |
| 2 | Avg Price | Strategy-owned average fill price for the currently open shares |
| 3 | U/P&L | Platform-managed unrealized P&L for the strategy's currently open shares — fixed 65px |
| 4 | R/P&L | Platform-managed cumulative realized P&L for the strategy+symbol during the current strategy session — fixed 65px |

**Header toolbar** (above tree):
- "Load ⊕" button (top-right corner) — opens `StrategyLoadDialog`

**Right-click context menu** (strategy-level rows):
- **Start** — `StrategyManager::startStrategy(id)` (visible when stopped/loaded)
- **Stop / Unload** — `StrategyManager::unloadStrategy(id)` (visible when running/error)
- **Display Logs** — shows `StrategyLogWidget` in the bottom horizontal splitter

**Symbol selection**: Clicking a symbol row emits `symbolSelected(symbol)` → `GUIFrontend::displayStock()`
(warm switch — no cold load since strategy already subscribed to that symbol)

**Row ordering**:
- Symbol rows are ordered **within each strategy group only**
- Row order follows stable claim order and does not change from activity/position/P&L updates
- Unclaimed symbols are removed from the list
- Re-claimed symbols are treated as new claims and appended at the end

**Connections**: `StrategyManager` signals: `strategyLoaded`, `strategyUnloaded`,
`strategyStatusChanged`, `symbolsClaimed`

**Placed in**: `rightPanel` above `Level2Widget` in `GUIFrontend.ui`

### StrategyLogWidget (Widgets/StrategyLogWidget/)

**Per-strategy log viewer shown at the bottom of the main window alongside the platform logger.**

- Appears in the **right panel** of a `QSplitter(Horizontal)` that wraps the bottom logger area
- Left panel = existing `liveLogDisplay` (platform logs); right panel = `StrategyLogWidget`
- Starts hidden (zero-width); expands when "Display Logs" is triggered from StrategyQuickView
- Title bar: "Strategy Log: \<name\>" + level filter combo + Close (×) button
- QTimer (1000 ms) appends only new messages (tracks `m_lastLogIndex` into `StrategyLogger` buffer)
- `setStrategy(id, name)` / `clearStrategy()` public API

### StrategiesTab/ — REMOVED

The StrategiesTab (`Tabs/StrategiesTab/`) was removed. All strategy management is now handled via:
- **StrategiesQuickView** widget (Trade tab) — load, start, stop, display logs
- **StrategyLogWidget** (bottom splitter) — per-strategy log display

`StrategyLoadDialog` was moved to `Src/FrontEnd/GUI/Dialogs/` when the tab was removed.

## Dark Theme

Custom dark color scheme applied on startup:

```cpp
void setupDarkTheme(QMainWindow* window) {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(53, 53, 53));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(25, 25, 25));
    palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Button, QColor(53, 53, 53));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::Highlight, QColor(142, 45, 197));
    window->setPalette(palette);
}
```

## Signal/Slot Connections

### Display Refresh (Pull-Based, 30 Hz)

Market data is NOT pushed via signals. GUIFrontend runs a 33 ms timer that
reads `DisplaySnapshot` from the current `SymbolContext`:

```cpp
// Timer setup (in constructor)
m_displayRefreshTimer.start(33);  // ~30 Hz
connect(&m_displayRefreshTimer, &QTimer::timeout, this, &GUIFrontend::onDisplayRefreshTick);

// Each tick: read dirty flags, copy data, clear flags, update widgets
void GUIFrontend::onDisplayRefreshTick() {
    auto* sc = MainAlgo::getInstance()->getDisplayedSymbolContext();
    if (!sc) return;

    QWriteLocker lock(&sc->m_displaySnapshot.lock);
    // Check dirty flags, copy data, clear flags
    // Then update chart / Level2 / trades widgets outside lock
}
```

### From Backend to GUI (Push — Non-Display Events)

```cpp
// MainAlgo → GUIFrontend (positions, orders, accounts — still pushed)
connect(&MainAlgo::getInstance(), &MainAlgo::receivedNewPosition,
        this, &GUIFrontend::onNewPositionReceived);

connect(&MainAlgo::getInstance(), &MainAlgo::receivedNewOrder,
        this, &GUIFrontend::onNewOrderReceived);
```

**GUIFrontend order/position forwarding to chart**:

The `onNewOrderReceived()` and `onNewPositionReceived()` handlers forward events to both the respective windows AND the StockPriceChart for visualization:

```cpp
void GUIFrontend::onNewOrderReceived(QString account, Order order) {
    ui->orderWidget->updateOrder(account, order);

    // Forward to chart for visualization (if symbol matches)
    if (order.getSymbol() == ui->priceChart->getCurrentSymbol()) {
        // Route based on order status
        if (status == Order::Status::FLL) {
            ui->priceChart->onOrderFilled(order);
        } else if (status == Order::Status::OPN) {
            ui->priceChart->onOrderPlaced(order);
        }
        // ... etc
    }
}
```

### From GUI to Backend

```cpp
// Symbol selection
connect(ui->symbolInput, &QLineEdit::returnPressed,
        this, &GUIFrontend::onSymbolEntered);

void GUIFrontend::onSymbolEntered() {
    QString symbol = ui->symbolInput->text().toUpper();
    emit selectedDisplayedStock(symbol);  // → MainAlgo
}

// Order placement
connect(ui->orderEntryWidget, &OrderEntryWidget::orderPlaced,
        this, [this](const PlaceOrderRequest& request) {
            emit orderPlaced(request);  // → MainAlgo
        });
```

### Internal GUI Connections

```cpp
// Chart requests missing bars
connect(ui->stockPriceChart, &StockPriceChart::requestMissingBars,
        this, [this](const QDateTime& start, const QDateTime& end) {
            emit requestMissingBars(m_currentSymbol, start, end);
        });

// Position window symbol clicked
connect(m_positionWidget, &PositionWidget::symbolClicked,
        this, [this](const QString& symbol) {
            ui->symbolInput->setText(symbol);
            onSymbolEntered();
        });
```

## State Management

### Session Persistence

Save/restore on close/startup:
```cpp
void GUIFrontend::saveState() {
    Settings::setValue("GUI/Geometry", saveGeometry());
    Settings::setValue("GUI/WindowState", saveState());
    Settings::setValue("GUI/LastSymbol", m_currentSymbol);
    Settings::setValue("GUI/SelectedAccount", getSelectedAccountId());
    Settings::setValue("GUI/StickyPriceEnabled", ui->orderEntryWidget->isStickyEnabled());
}

void GUIFrontend::restoreState() {
    restoreGeometry(Settings::getValue("GUI/Geometry"));
    restoreState(Settings::getValue("GUI/WindowState"));
    // ... restore other state
}
```

## Performance Optimizations

### Update Throttling

High-frequency updates throttled to avoid UI lag:
```cpp
QTimer* m_updateThrottle = new QTimer(this);
m_updateThrottle->setSingleShot(true);
m_updateThrottle->setInterval(100);  // Max 10 updates/sec

void onCurrentHighlightedReceivedNewLevel2(...) {
    m_pendingLevel2 = level2;
    if (!m_updateThrottle->isActive()) {
        updateLevel2Display(m_pendingLevel2);
        m_updateThrottle->start();
    }
}
```

### Lazy Loading

- Load historical bars on demand (pan/zoom)
- Don't load all positions upfront
- Paginate large result sets

### Model Updates

- Use setData() instead of full model reset
- Update only changed cells
- Batch updates when possible

## Keyboard Shortcuts

Managed by ShortcutSettings singleton:
```cpp
QAction* quitAction = new QAction(tr("&Quit"), this);
quitAction->setShortcut(ShortcutSettings::getShortcut("quit", QKeySequence::Quit));
connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
```

Default shortcuts:
- Ctrl+Q: Quit
- F5: Refresh
- Ctrl+W: Close window
- Ctrl+N: New order
- Esc: Cancel/close dialog

Strategy manual-confirmation prompt keys (only while prompt is active):
- `Y`: Accept
- `N`: Reject
- `Shift+N`: Reject and block future manual confirmations for that symbol
- `W`: Authorize symbol focus switch when the prompt requires explicit switch authorization

## Error Handling

### Display Errors

```cpp
void showError(const QString& title, const QString& message) {
    QMessageBox::critical(this, title, message);
}

void showWarning(const QString& title, const QString& message) {
    QMessageBox::warning(this, title, message);
}

void showInfo(const QString& title, const QString& message) {
    QMessageBox::information(this, title, message);
}
```

### Validation Errors

```cpp
bool OrderEntryWidget::validate() {
    if (m_symbolInput->text().isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"),
                           tr("Symbol is required"));
        m_symbolInput->setFocus();
        return false;
    }
    // ... more validation
    return true;
}
```

## Testing GUI Components

### Unit Tests

Test individual widgets:
```cpp
TEST(OrderEntryWidget, ValidatesRequiredFields) {
    OrderEntryWidget widget;
    widget.setSymbol("");
    EXPECT_FALSE(widget.validate());

    widget.setSymbol("AAPL");
    EXPECT_TRUE(widget.validate());
}
```

### Integration Tests

Test with mock backend:
```cpp
MockMainAlgo mockAlgo;
GUIFrontend frontend(&mockAlgo);

// Simulate bar received
mockAlgo.emitTestBar("AAPL", generateBar());

// Verify chart updated
EXPECT_EQ(frontend.getChart()->getBarCount(), 1);
```

## Related Agent Instructions

- `../AGENTS.md`: FrontEnd abstract interface
- `../../Algo/AGENTS.md`: Backend coordination
- `../../Core/AGENTS.md`: Core components

## Related Documentation

- `Doc/FRONTEND.md`: Complete frontend documentation with diagrams
- `Doc/ARCHITECTURE.md`: System architecture
- `Doc/DEVELOPMENT.md`: Development guidelines
