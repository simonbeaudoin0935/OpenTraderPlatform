# StockPriceChart/ - Real-Time Price Chart with Replay Mode - Agent Instructions

## Overview

**Location**: `Src/FrontEnd/GUI/StockPriceChart/`
**Purpose**: Display live candlestick charts with real-time data, historical data loading, and market replay functionality
**Main Class**: `StockPriceChart` (split across multiple source files)

## File Structure

The StockPriceChart class has been split into multiple source files for better organization and maintainability:

```
StockPriceChart/
├── AGENTS.md                        # This file
├── StockPriceChart.h                # Main class declaration
├── StockPriceChart.cpp              # Constructor, destructor, symbol management (377 lines)
├── StockPriceChartBars.cpp          # Bar management and drawing (820 lines)
├── StockPriceChartReplay.cpp        # Replay mode functionality (312 lines)
├── StockPriceChartOrderViz.cpp      # Order visualization (920 lines)
├── StockPriceChartUtils.cpp         # Time/index utilities (297 lines)
├── ChartToolbar.h/cpp               # Toolbar controls
├── IndexToTimeTicker.h              # Custom axis ticker
└── ZoomAndPanning.cpp               # Mouse interaction handling
```

### Source File Organization

Following the TSClient pattern, the implementation is divided into logical units:

**StockPriceChart.cpp** - Core functionality:
- Constructor and destructor
- `setSymbol()` - Switch displayed stock symbol
- `populateAvailableReplayDays()` - Scan cache for available replay data

**StockPriceChartBars.cpp** - Bar management and drawing:
- `addLiveBar()` - Add new bar to chart
- `updateCandlestickData()`, `updateVolumeData()` - Update visual data
- `rescaleVolumeAxisToVisibleRange()` - Dynamic volume axis scaling
- `drawBackgroundsForReceivedBars()` - Session background rendering
- `clearBackgroundRects()`, `drawFixedBackgroundRect()` - Background management
- `onRequestedMissingBarsReceived()`, `onRequestedMissingBarsFailed()` - Handle bar requests
- `redrawLastPriceLine()` - Current price indicator
- `checkForMissingBars()` - Detect and request missing data
- `clearSymbol()`, `clearChart()` - Chart clearing operations

**StockPriceChartReplay.cpp** - Replay mode:
- `onReplayDayChanged()`, `onReplayTimeChanged()` - User replay controls
- `onReplayTimeRangeQueryFinished()` - Handle time range queries
- `onReplayDataLoadFailed()` - Error handling
- `queryStockTimeRangeForDate()` - Query available data
- `setReplayModeActive()` - Enable/disable replay mode
- `updateCurrentTimeLine()` - Draw current time indicator

**StockPriceChartOrderViz.cpp** - Order visualization:
- `getExactIndexForTimestamp()` - Precise timestamp positioning
- `createOrderMarker()`, `createBuyMarker()`, `createSellMarker()`, `createCancelledMarker()` - Marker creation
- `updateMarkerState()`, `moveMarkerToPrice()`, `removeOrderMarker()` - Marker management
- `clampIndexToValidRange()` - Boundary checking
- `createPositionLine()` - Draw position connection lines
- `updateOpenPositionDynamicLine()` - Live position tracking
- `ensureOpenPositionPLBox()`, `updateOpenPositionPLBox()`, `hideOpenPositionPLBox()` - P&L display
- `createClosedPositionPLLabel()` - Realized P&L labels
- `calculateDCAPrice()` - Dollar-cost averaging calculation
- `finalizeClosedPosition()` - Complete position visualization
- `clearOrderVisualizations()` - Remove all visualizations
- `loadHistoricalOrders()`, `loadHistoricalPositions()` - Load from database
- `updateOrderVisualizationsVisibility()`, `cullOrderVisualizationsToVisibleRange()` - Performance optimization
- `onOrderPlaced()`, `onOrderFilled()`, `onOrderCancelled()`, `onOrderAmended()` - Order event handlers
- `onPositionOpened()`, `onPositionUpdated()`, `onPositionClosed()` - Position event handlers
- `setOrderVisualizationsVisible()` - Toggle visibility

**StockPriceChartUtils.cpp** - Time/index utilities:
- `getPreviousTradingMinute()` - Find previous valid trading time
- `adjustToValidTradingTime()` - Adjust timestamp to trading hours
- `getPreviousFriday()` - Calculate previous Friday for weekends
- `updateAxisLabelsDensity()` - Adjust axis label spacing
- `addHistoricalBarsToIndexMapping()` - Build bidirectional index
- `getTimestampForIndex()`, `getIndexForTimestamp()` - Index/time conversions
- `indexToTimeString()` - Format index as time string

## Architecture

### Component Hierarchy

```
StockPriceChart (QWidget)
    │
    ├── ChartToolbar (Toolbar controls)
    │       ├── Timeframe selector
    │       ├── Auto-timeframe checkbox
    │       ├── Volume controls
    │       ├── Replay controls (day, time, speed, play/pause)
    │       └── Wheel sensitivity settings
    │
    └── QCustomPlot (Chart rendering)
            ├── m_candlesticks (QCPFinancial - main candlestick chart)
            ├── m_positiveBars, m_negativeBars (volume bars)
            ├── m_currentPriceLine (white vertical line)
            ├── Axes with TimeTickerEngine
            └── Session background rects (pre-market, regular, after-hours)
```

## Replay Mode Feature (NEW)

### State Machine

The replay controls use a 4-state machine managed by `ChartToolbar`:

```
┌──────────┐
│ Inactive │ ◄─────┐
└────┬─────┘       │
     │             │
     │ enterReplayMode()
     ▼
┌──────────────────┐
│ PreloadingPaused │ (First data loaded, chart populated, waiting for play)
└────┬─────────────┘
     │
     │ Play button pressed
     ▼
┌────────┐
│Playing │ (Replay running)
└────┬───┘
     │
     │ Pause button pressed or pause signal
     ▼
┌───────┐
│Paused │ (Paused during playback)
└────┬──┘
     │
     └─ Day/Time change triggers re-preload
     │  (stays in Paused state)
     │
     └─ Play button pressed
        (back to Playing)

     │
     └─ exitReplayMode()
        (back to Inactive)
```

### UI Control Enable/Disable Matrix

| State | Day Combo | Time Edit | Speed Combo | Play/Pause Button |
|-------|-----------|-----------|-------------|-------------------|
| **Inactive** | ❌ | ❌ | ❌ | ❌ |
| **PreloadingPaused** | ✅ | ✅ | ✅ | ✅ |
| **Playing** | ❌ | ❌ | ✅ | ✅ |
| **Paused** | ✅ | ✅ | ✅ | ✅ |

### Dynamic Day/Time Selection Feature

**Problem Solved**: Users can now change the replay day or start time BEFORE or DURING a pause, and the chart automatically reloads with the new day/time data.

**Key Files**:
- `ChartToolbar.h/cpp`: UI controls and state management
- `StockPriceChart.h/cpp`: Signal handlers and chart updates
- `Src/Core/MainApp.cpp`: Preload orchestration
- `Src/Core/Replay/ReplayEngine.cpp`: Error signaling

**Flow**:

```
User selects different day (in paused state)
    │
    ▼
ChartToolbar::onReplayDayChanged()
    │
    ├─ Guard: If Playing state, return (ignore change)
    │
    └─ If PreloadingPaused or Paused: emit replayDayChanged(date)
            │
            ▼
    StockPriceChart::onReplayDayChanged(date)
            │
            ├─ Check: MainApp::isInReplayMode()?
            │
            └─ If yes: MainApp::preloadChartForReplay(date, currentTime, speed)
                    │
                    ▼
            MainApp::preloadChartForReplay()
                    │
                    └─ MainAlgo::enterReplayModePaused(newDate, time, speed)
                            │
                            ▼
            ReplayEngine::startReplayPaused()
                    │
                    ├─ initLoaders(newDate, time)
                    │
                    ├─ If error: emit replayDataLoadFailed(message)
                    │       │
                    │       ▼
                    │   StockPriceChart::onReplayDataLoadFailed()
                    │       └─ Clear toolbar info, log error
                    │
                    └─ If success: emit first bar, pause
                            │
                            ▼
                    Chart updates with new day's data
```

**Time Change Flow** (similar to day change):
```
User changes start time (in paused state)
    │
    ▼
ChartToolbar::onReplayTimeChanged()
    │
    ├─ Guard: If Playing state, return (ignore change)
    │
    └─ If paused: emit replayStartTimeChanged(time)
            │
            ▼
    StockPriceChart::onReplayTimeChanged(time)
            │
            └─ MainApp::preloadChartForReplay(currentDate, newTime, speed)
                    │
                    ▼
            Chart updates with new start time
```

### Signal Flow

```
ChartToolbar signals:
    replayDayChanged(QDate) ──► StockPriceChart::onReplayDayChanged()
    replayStartTimeChanged(QTime) ──► StockPriceChart::onReplayTimeChanged()
    replayPlayPauseToggled(bool playing) ──► [to MainApp via StockPriceChart lambda]
    replaySpeedChanged(PlaybackSpeed) ──► [to MainApp via lambda]

ReplayEngine signals:
    replayDataLoadFailed(QString error) ──► StockPriceChart::onReplayDataLoadFailed()
    [Connected in GUIFrontend::onReplayModeEntered()]

StockPriceChart internal:
    requestMissingBars(QDateTime, QDateTime) ──► [to MainApp for historical fetch]
```

### ChartToolbar Class

**Key Methods**:

```cpp
class ChartToolbar : public QWidget {
public:
    enum class ReplayState {
        Inactive,         // No replay active
        PreloadingPaused, // First data loaded, waiting for play
        Playing,          // Actively replaying
        Paused            // Paused during playback
    };

    // State management
    ReplayState getReplayState() const;
    void setReplayState(ReplayState state);
    void updateUIControlStates();  // Apply enable/disable based on state

    // Replay controls
    QDate getSelectedReplayDay() const;
    void setSelectedReplayDay(const QDate& date);
    QTime getReplayStartTime() const;
    void setReplayStartTime(const QTime& time);
    ReplayEngine::PlaybackSpeed getReplaySpeed() const;
    bool isReplayPlaying() const;
    void setReplayPlaying(bool playing);
    void togglePlayPause();
    void updateReplayInfo(const QTime& startTime, const QTime& endTime, int barCount);

    // Scan cache for available days
    void scanAndPopulateReplayDays();
    void setAvailableReplayDays(const QList<QDate>& days);

private slots:
    void onPlayPauseClicked();      // Manages state transitions
    void onReplayDayChanged(int index);   // Emits signal + guard
    void onReplayTimeChanged(const QTime& time);  // Emits signal + guard
    void onWheelRatioChanged(int index);

private:
    void updatePlayPauseButton();   // Updates button text/color
    void updateUIControlStates();   // Disable/enable controls per state

    ReplayState m_replayState = ReplayState::Inactive;
};
```

**State Transitions**:

1. **Initialization**: `Inactive` (all controls disabled)
2. **On Replay Mode Enter**: Transition to `PreloadingPaused` (controls enabled)
3. **On Play Press**: Transition to `Playing` (day/time disabled)
4. **On Pause Press**: Transition to `Paused` (day/time re-enabled)
5. **On Day/Time Change (while Paused)**: Repreload, stay in `Paused`
6. **On Replay Mode Exit**: Transition to `Inactive` (all controls disabled)

**Guard Logic**:

```cpp
// In onReplayDayChanged()
if (m_replayState == ReplayState::Playing) {
    return;  // Silently ignore changes during playback
}

// In onReplayTimeChanged()
if (m_replayState == ReplayState::Playing) {
    return;  // Silently ignore changes during playback
}

// In onPlayPauseClicked()
if (playing) {
    setReplayState(ReplayState::Playing);  // Disable day/time
} else if (m_replayState == ReplayState::Playing) {
    setReplayState(ReplayState::Paused);   // Re-enable day/time
}
```

### StockPriceChart Integration

**New Public Slots**:

```cpp
// Replay data loading error handler
void onReplayDataLoadFailed(const QString& errorMessage);

// Triggered when user changes day/time selection during paused replay
void onReplayDayChanged(const QDate& date);
void onReplayTimeChanged(const QTime& time);
```

**Signal Connections** (in constructor):

```cpp
connect(chartToolbar, &ChartToolbar::replayDayChanged,
        this, &StockPriceChart::onReplayDayChanged);

connect(chartToolbar, &ChartToolbar::replayStartTimeChanged,
        this, &StockPriceChart::onReplayTimeChanged);

// Error handling (connected in GUIFrontend::onReplayModeEntered())
ReplayEngine* engine = MainApp::getInstance()->getReplayEngine();
connect(engine, &ReplayEngine::replayDataLoadFailed,
        this, &StockPriceChart::onReplayDataLoadFailed, Qt::UniqueConnection);
```

**Preload Trigger Logic**:

```cpp
void StockPriceChart::onReplayDayChanged(const QDate& date) {
    if (MainApp::isInReplayMode()) {
        QTime currentTime = chartToolbar->getReplayStartTime();
        ReplayEngine::PlaybackSpeed speed = chartToolbar->getReplaySpeed();

        // Trigger preload with new day
        MainApp::getInstance()->preloadChartForReplay(date, currentTime, speed);
    }
}
```

### Error Handling

**Database File Not Found/Corrupted**:

1. ReplayEngine detects failure in `initLoaders()`
2. Emits `replayDataLoadFailed(errorMessage)`
3. Signal connects to `StockPriceChart::onReplayDataLoadFailed()`
4. Handler clears toolbar info label to indicate failure
5. User can retry with different day or debug

**Example**:
```cpp
void StockPriceChart::onReplayDataLoadFailed(const QString& errorMessage) {
    qCWarning(ChartLog) << "Replay data load failed:" << errorMessage;

    // Clear info to show failure
    chartToolbar->updateReplayInfo(QTime(), QTime(), 0);

    qCCritical(ChartLog) << "Failed to preload replay data:" << errorMessage;
}
```

## Chart Display

### Index-Based Positioning System

The chart uses a bidirectional index system for efficient bar placement:

```
Historical ◄─────────┤ Origin ├─────────► Live/Future
       -3  -2  -1    0    1   2   3   4
```

**Benefits**:
- O(m) insertion of m historical bars (no full rebuild)
- Preserves existing indices
- Efficient pan/zoom operations

### Time Mapping

Two-way mapping between bar indices and timestamps:

```cpp
QMap<int, QDateTime> m_indexToTimeMap;        // index → timestamp
QMap<QDateTime, int> m_timeToIndexMap;        // timestamp → index
```

## Live Bar Updates

### Signal Flow

```
MainAlgo (MainAlgoThread)
    │
    └─ displayedStockReceivedNewBar(symbol, bar)
            │
            ▼ (cross-thread signal)

GUIFrontend (Main Thread)
    │
    └─ onCurrentHighlightedStockBarReceived(symbol, bar)
            │
            ▼

StockPriceChart::addLiveBar(symbol, bar)
    │
    ├─ Update m_latestBar and m_latestBarIndex
    ├─ Update candlestick data
    ├─ Update volume bars
    ├─ Redraw chart
    └─ Emit requestMissingBars if gap detected
```

## Interaction Controls

**Mouse/Keyboard**:
- `Ctrl+Shift+Scroll`: Vertical pan
- `Alt+Scroll`: Horizontal pan
- `Ctrl+Scroll`: Horizontal zoom
- `Shift+Scroll`: Vertical zoom
- `Scroll`: Both axes zoom
- `Right Click`: Reset to last 30 bars

## Threading

- **StockPriceChart**: Runs in Main (GUI) thread
- **ChartToolbar**: Runs in Main (GUI) thread
- **Signals from MainAlgo**: Cross-thread via `Qt::QueuedConnection`
- **Signals to MainApp**: Cross-thread via lambda/connection
- **ReplayEngine**: Runs in MainAlgoThread, signals cross-thread

## Performance Optimizations

### Update Throttling

High-frequency market depth updates throttled:
```cpp
QTimer m_updateThrottle;  // 100ms interval max 10 updates/sec
```

### Lazy Historical Data Loading

Historical bars loaded on-demand:
- User pans left → request earlier bars
- RequestMissingBars emitted → MainApp fetches from cache
- Received bars added to chart via `onRequestedMissingBarsReceived()`

### Bidirectional Index Efficiency

Adding m historical bars: O(m) instead of O(n) rebuild

## Related Components

- `ChartToolbar.h/cpp`: Replay controls and state management
- `Src/Core/MainApp.cpp`: Preload orchestration
- `Src/Core/Replay/ReplayEngine.cpp`: Replay engine
- `Src/FrontEnd/GUI/GUIFrontend.cpp`: Frontend integration
- `qcustomplot/`: Chart rendering library (external)

## Common Tasks

### Adding New Replay Feature

1. Add new control to ChartToolbar UI
2. Add corresponding getter/setter in ChartToolbar
3. Add state-aware enable/disable in `updateUIControlStates()`
4. Add signal in ChartToolbar header
5. Connect signal in StockPriceChart constructor
6. Implement handler slot in StockPriceChart
7. If needs preload: call `MainApp::preloadChartForReplay()`

### Debugging Replay Issues

1. Enable `Chart`, `ReplayEngine` logging categories
2. Check ChartToolbar state machine state via `getReplayState()`
3. Verify ReplayEngine emitting `replayDataLoadFailed` on errors
4. Check database files exist: `~/.cache/L2Trader/RecordedLiveData/Bars/`
5. Verify MainApp::isInReplayMode() returns true

### Handling New Edge Cases

1. Add validation/guard in appropriate slot
2. Emit error signal from ReplayEngine if needed
3. Connect error signal in GUIFrontend::onReplayModeEntered()
4. Handle error in `StockPriceChart::onReplayDataLoadFailed()`
5. Provide user feedback (update toolbar info, log error)

## Future Enhancements

**Possible improvements** (not implemented):
1. Cancel pending preloads on replay mode exit
2. Validate time is within available trading range
3. Check symbol has data on selected day before preload
4. Handle rapid day/time changes (race condition)
5. Auto-adjust invalid times to nearest trading time
6. Better error messages for edge cases

These are tracked in the implementation plan but deemed optional for core functionality.

## Order Visualization Feature

### Overview

The chart displays order executions and position P&L directly on the candlestick chart, allowing traders to:
- See exact entry and exit points with precise time/price coordinates
- Track positions dynamically with real-time P&L
- Visualize historical trades for analysis
- Connect related orders forming positions with visual lines

### Position Lifecycle Model

A **position** is defined as a complete round-trip from 0 shares to some quantity and back to 0 shares:

```
Position 1: Buy 10 → Sell 10 (closed, quantity=0)
Position 2: Buy 10 → Sell 10 (new position, separate ID)
Position 3: Buy 10 → Buy 10 → Sell 20 (DCA closed, quantity=0)
Position 4: Buy 10 → Sell 5 → ... (still open, 5 shares remaining)
```

Each closed position appears as separate entry in PositionWidget with quantity=0 and realized P&L.

### Data Structures

```cpp
// Order marker for visual representation
struct OrderMarker {
    QString orderID;
    QString symbol;
    QDateTime timestamp;      // Fill time for filled, placement time for pending
    double price;             // Fill price for filled, order price for pending
    int quantity;
    bool isBuy;               // true = buy, false = sell
    QString accountID;

    enum class State {
        Pending,     // Hollow triangle
        Filled,      // Solid triangle
        Cancelled    // Gray X
    };
    State state;

    // QCustomPlot visual elements (3 lines form triangle, 2 lines form X)
    QCPItemLine* markerLine1 = nullptr;
    QCPItemLine* markerLine2 = nullptr;
    QCPItemLine* markerLine3 = nullptr;
};

// Position visualization for connecting orders
struct PositionVisualization {
    QString positionID;
    QString symbol;
    QVector<OrderMarker*> entryMarkers;     // All buy orders in this position
    QVector<OrderMarker*> exitMarkers;      // All sell orders in this position
    QVector<QCPItemLine*> entryLines;       // Lines connecting sequential entries
    QCPItemLine* dynamicLine = nullptr;     // For open positions (to current price)
    QVector<QCPItemLine*> exitLines;        // Lines to each exit
    QCPItemText* plLabel = nullptr;         // P&L text label
    QCPItemRect* plBackground = nullptr;    // P&L box background
    double realizedPL = 0.0;                // Final P&L when closed
    bool isClosed = false;                  // True when shares reach 0
    bool isShort = false;                   // True for short positions
    double avgEntryPrice = 0.0;             // DCA average entry price
    int currentQuantity = 0;                // Current position size
    int totalBought = 0;                    // Total shares bought
    int totalSold = 0;                      // Total shares sold
};
```

### Visual Specifications

| Element | Color | Style | Size |
|---------|-------|-------|------|
| Buy marker (filled) | Green #00C800 | Solid upward triangle | Scales with zoom |
| Sell marker (filled) | Red #C80000 | Solid downward triangle | Scales with zoom |
| Buy marker (pending) | Green #00C800 | Dashed upward triangle | Scales with zoom |
| Sell marker (pending) | Red #C80000 | Dashed downward triangle | Scales with zoom |
| Cancelled marker | Gray #808080 | X shape | Scales with zoom |
| Position line (profit) | Green #00C800 | Dotted | 1-2px |
| Position line (loss) | Red #C80000 | Dotted | 1-2px |
| P&L text | Green/Red | Bold | 8-10pt |
| P&L box background | Dark gray | 70-80% opacity | Auto-size |

### Signal/Slot Connections

```
GUIFrontend::onNewOrderReceived()
    │
    ├─ Order::Status::OPN/ACK ──► StockPriceChart::onOrderPlaced()
    ├─ Order::Status::FLL/FLP/FPR ──► StockPriceChart::onOrderFilled()
    ├─ Order::Status::CAN/UCN/TSC ──► StockPriceChart::onOrderCancelled()
    └─ Order::Status::UCH/RSN ──► StockPriceChart::onOrderAmended()

GUIFrontend::onNewPositionReceived()
    │
    ├─ quantity == 0 ──► StockPriceChart::onPositionClosed()
    └─ quantity > 0 ──► StockPriceChart::onPositionUpdated()
```

### Public Slots

```cpp
// Order event handlers
void onOrderPlaced(const Order& order);    // Create pending marker
void onOrderFilled(const Order& order);     // Convert to filled, update position
void onOrderCancelled(const Order& order);  // Convert to cancelled X
void onOrderAmended(const Order& order);    // Move marker to new price

// Position event handlers
void onPositionOpened(const Position& position);   // Create new position
void onPositionUpdated(const Position& position);  // Update dynamic line/P&L
void onPositionClosed(const Position& position);   // Finalize, show realized P&L
```

### Helper Methods

```cpp
// Marker creation
OrderMarker* createBuyMarker(const Order& order, OrderMarker::State state);
OrderMarker* createSellMarker(const Order& order, OrderMarker::State state);
void createCancelledMarker(OrderMarker* marker);
void updateMarkerState(OrderMarker* marker, OrderMarker::State newState);
void moveMarkerToPrice(OrderMarker* marker, double newPrice);

// Position line management
void createPositionLine(PositionVisualization* pos, OrderMarker* from, OrderMarker* to);
void updateOpenPositionDynamicLine(double currentPrice, int currentBarIndex);
void updateOpenPositionPLBox(double currentPrice);
void createRealizedPLLabel(PositionVisualization* pos);

// Coordinate conversion
double getExactIndexForTimestamp(const QDateTime& timestamp);

// Cleanup
void clearOrderVisualizations();
void updateOrderVisualizationsVisibility();
```

### Toolbar Toggle

The ChartToolbar includes an "Orders" checkbox to show/hide all order visualizations:

```cpp
// ChartToolbar signals
void orderVisualizationsVisibilityChanged(bool visible);

// StockPriceChart slot
void onOrderVisualizationsVisibilityChanged(bool visible);
```

### Historical Data Loading

On symbol change, historical orders and positions are loaded:

```cpp
void loadHistoricalOrders();   // Query OrdersDatabase
void loadHistoricalPositions(); // Query PositionsDatabase
```

### P&L Calculations

**Long positions**:
```cpp
unrealizedPL = (currentPrice - avgEntryPrice) * currentQuantity;
```

**Short positions**:
```cpp
unrealizedPL = (avgEntryPrice - currentPrice) * currentQuantity;
```

**DCA average entry price**:
```cpp
avgEntryPrice = totalCostBasis / totalSharesBought;
```

### Performance Optimizations

1. **Culling**: Markers and lines outside visible X-axis range are hidden
2. **Z-ordering**: Recent orders render on top of older ones
3. **Update frequency**: P&L box updates with every bar (no throttling)
4. **Immediate clear**: Visualizations cleared instantly on symbol change

### Testing Scenarios

| Scenario | Expected Result |
|----------|-----------------|
| Buy 10 → Sell 10 | Closed position with 2 markers, line, realized P&L |
| Buy 10 → Sell 10 → Buy 10 → Sell 10 | Two separate closed positions |
| Buy 10 → Buy 10 → Sell 20 | DCA position with 3 markers, connecting lines |
| Buy 20 → Sell 10 → Sell 10 | Partial close, then full close |
| Order placed (pending) | Hollow triangle marker |
| Order cancelled | Gray X marker |
| Order amended | Marker moves to new price |

### Troubleshooting

**Markers not appearing**:
1. Check order symbol matches chart symbol
2. Verify OrdersDatabase is initialized
3. Check timestamp is within visible range
4. Verify m_orderVisualizationsVisible is true

**P&L miscalculated**:
1. Verify avgEntryPrice calculation includes all entries
2. Check long vs short position detection
3. Verify currentQuantity updates on partial closes

**Lines not connecting**:
1. Check position has both entry and exit markers
2. Verify getExactIndexForTimestamp() returns valid index
3. Check bars exist for the timestamps

