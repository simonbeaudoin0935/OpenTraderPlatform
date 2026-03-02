# GUI/ - Graphical User Interface - Agent Instructions

The GUI directory contains the full-featured Qt Widgets desktop interface for L2Trader.

## Overview

**Location**: `Src/FrontEnd/GUI/`
**Framework**: Qt Widgets 6.x
**Main Class**: GUIFrontend (implements FrontEnd abstract base class)
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
│   ├── CacheTab.cpp/h              # Bar cache management
│   ├── ConfigTab.cpp/h             # Application settings
│   ├── LoggingTab.cpp/h            # Live log display
│   ├── ShortcutsTab.cpp/h          # Keyboard shortcuts
│   └── StrategiesTab/              # Strategy plugin management
│       ├── AGENTS.md
│       └── ... (strategy UI components)
├── Widgets/                        # Reusable widget components
│   ├── AGENTS.md
│   ├── OrderEntry/                 # Order placement widget
│   │   ├── AGENTS.md
│   │   └── OrderEntryWidget.cpp/h
│   ├── MarketDepth/                # Level 2 market depth display
│   │   ├── AGENTS.md
│   │   ├── MarketDepthTable.cpp/h
│   │   └── MarketDepthTableView.cpp/h
│   └── Gauge/                      # Circular gauge for metrics
│       ├── AGENTS.md
│       └── Gauge.cpp/h
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
- `Tabs/AGENTS.md` - Tab components (cache, logging, strategies, etc.)
- `Tabs/StrategiesTab/AGENTS.md` - Strategy plugin management UI
- `Widgets/AGENTS.md` - Reusable widget components overview
- `Widgets/OrderEntry/AGENTS.md` - Order entry widget with sticky price
- `Widgets/MarketDepth/AGENTS.md` - Level 2 market depth display
- `Widgets/Gauge/AGENTS.md` - Circular gauge metrics display
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
│  │ • Settings Tab                     │  │
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

**Implementation**: TODO Phase 4 — will be driven by Databento `StatusMsg` via `DBClient::tradingStatusChanged` signal. Currently not connected (stub).

**BATS flag**: Intentionally not displayed (removed by design — the IsBats flag is not user-relevant).


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
- Last price line with dynamic label
- Missing bar detection and auto-request
- Mouse interactions (pan, zoom, reset)
- **Order visualization**: Buy/sell markers, position lines, P&L display
- **Replay mode**: Play back historical market data

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
```

### MarketDepthTable (Widgets/MarketDepth/)

**Level 2 market depth display**

See `Widgets/MarketDepth/AGENTS.md` for complete documentation.

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

**Custom View**: `MarketDepthTableView` handles formatting and color coding

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

**Signal Interface**:
```cpp
signals:
    void orderPlaced(const PlaceOrderRequest& request);

public slots:
    void setSymbol(const QString& symbol);
    void updateStickyPrice(const Level2& level2);
```

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
- Action (Buy, Sell, etc.)
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

The RecorderTab was removed in Phase 3 of the Databento migration. Recording functionality
will be replaced by Databento `.dbn` archive downloads in Phase 7.

### Gauge/ (Widgets/Gauge/)

**Circular gauge widgets for metrics**

See `Widgets/Gauge/AGENTS.md` for complete documentation.

Used for displaying:
- **BAI** (Bid-Ask Imbalance)
- **DWP** (Depth-Weighted Price)
- **OBLR** (Order Book Level Ratio)
- **QRR** (Quote Refresh Rate)

Custom QWidget-based circular gauges with:
- Needle indicator
- Min/max/current value display
- Color-coded ranges (green/yellow/red)
- Smooth animations

### StrategiesTab/ (Tabs/StrategiesTab/)

**Strategy plugin management**

See `Tabs/StrategiesTab/AGENTS.md` and `Doc/STRATEGY.md` for complete details.

Features:
- Load strategy plugins (.so files)
- Start/stop strategies
- View strategy status and logs
- Crash isolation per strategy
- Strategy cards with metrics

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

### From Backend to GUI

```cpp
// MainAlgo → GUIFrontend
connect(&MainAlgo::getInstance(), &MainAlgo::displayedStockReceivedNewBar,
        this, &GUIFrontend::onCurrentHighlightedStockBarReceived);

connect(&MainAlgo::getInstance(), &MainAlgo::receivedNewPosition,
        this, &GUIFrontend::onNewPositionReceived);

connect(&MainAlgo::getInstance(), &MainAlgo::receivedNewOrder,
        this, &GUIFrontend::onNewOrderReceived);

// Level 2 data → MarketDepthTable
connect(&MainAlgo::getInstance(), &MainAlgo::displayedStockReceivedNewLevel2,
        this, &GUIFrontend::onCurrentHighlightedReceivedNewLevel2);
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
        updateMarketDepth(m_pendingLevel2);
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
