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

MACD automatic Y-axis ranges are symmetric about zero. Both normal and
Shift-wheel vertical zoom keep zero at the subplot midpoint; horizontal
zoom remains unchanged. Manual zoom remains preserved across data updates.

### Source File Organization

Following the TSClient pattern, the implementation is divided into logical units:

**StockPriceChart.cpp** - Core functionality:
- Constructor and destructor
- `setSymbol()` - Switch displayed stock symbol
- `populateAvailableReplayDays()` - Scan cache for available replay data
- `onLevel2Update()`, `updateBboOverlay()` - Best-bid/ask overlay guides and labels

**StockPriceChartBars.cpp** - Bar management and drawing:
- `addLiveBar()` - Add new bar to chart
- `updateCandlestickData()`, `updateVolumeData()` - Update visual data
- `rescaleVolumeAxisToVisibleRange()` - Dynamic volume axis scaling
- `drawSessionBackgroundsForDate()` - Session background rendering from time anchor alone
- `drawBackgroundsForReceivedBars()` - Session background rendering
- `clearBackgroundRects()`, `drawFixedBackgroundRect()` - Background management
- `onRequestedMissingBarsReceived()`, `onRequestedMissingBarsFailed()` - Handle bar requests
- `redrawLastPriceLine()` - Current price indicator with sticky edge labels when off-screen
- `checkForMissingBars()` - Detect and request missing data
- `clearSymbol()`, `clearChart()` - Chart clearing operations

**StockPriceChartReplay.cpp** - Replay mode:
- `onReplayDayChanged()`, `onReplayTimeChanged()` - User replay controls
- `onReplayTimeRangeQueryFinished()` - Handle time range queries
- `onReplayDataLoadFailed()` - Error handling
- `queryStockTimeRangeForDate()` - Query available data
- `setReplayModeActive()` - Enable/disable replay mode
- `updateCurrentTimeLine()` - Draw current time indicator
- Replay speed options include `25x` (`Playback::Speed::Fast25x`)

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
- `onStrategyLogEmitted()`, `createLogMarker()` - Bottom chart bubble-log markers (hover tooltip)
- `onStrategyStatusEmitted()`, `updateStrategyStatusVisual()` - Per-symbol rich-text strategy status overlay (top-left)
- `applyBracketOverlayEvent()`, `updateBracketOverlayVisuals()` - Managed bracket stop/take overlay rendering
- Drag-adjust emits `adjustManagedBracketRequested(...)` and commits through `MainAlgo::processAdjustManagedBracketLevels(...)`
- Bracket drag hit-testing is label-first; line fallback requires proximity to the drawn line segment in both X and Y (not Y-only), so off-segment clicks keep normal pan behavior
- Middle-click cycles bracket wheel-adjust mode while an overlay is active: `NORMAL -> STOP -> TAKE -> NORMAL`
- `TAKE` mode wheel changes take-profit target while keeping stop fixed (internally stepped using the R-step setting)
- `STOP` mode wheel changes stop-loss percent-from-entry and auto-recomputes take to preserve current R
- Mode badge is shown only in `TAKE`/`STOP`; hidden in `NORMAL`
- Mode math uses `referenceEntryPrice` from overlay payload (manual arming and managed overlays)
- Wheel-step and clamp settings are user-configurable in `ConfigTab` (`Config/BracketWheel*` keys)

### Overlay Placement (Current UX)

- Symbol watermark: bottom-right corner
- `Muted` watermark: bottom-center
- Strategy status rich text: top-left overlay (no dedicated status subchart)

### Strategy User-Confirm Preview Brackets

- Pending strategy user-confirm orders can render a preview bracket on chart before `Y` acceptance.
- The user can drag the preview stop; the accepted order path forwards this stop override.
- The final accepted order still runs through `MainAlgo` risk evaluation (including per-trade planned-loss checks).
- In hard mute mode (`M`), confirmations are auto-rejected and preview/focus-flash flow is suppressed.

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
     └─ Play button pressed
        (back to Playing)
     │
     └─ Restart returns to fresh
        pre-play selection state

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
| **Paused** | ❌ | ❌ | ✅ | ✅ |

### Dynamic Day/Time Selection Feature

**Current behavior**: Users can change the replay day or start time only in the fresh pre-play `PreloadingPaused` state. Once replay has started, both controls stay locked even while paused; `Restart` is the supported way to return to a fresh pre-play state where day/time are editable again.

**Key Files**:
- `ChartToolbar.h/cpp`: UI controls and state management
- `StockPriceChart.h/cpp`: Signal handlers and chart updates
- `Src/Core/MainApp.cpp`: Preload orchestration
- `Src/Core/Replay/ReplayEngine.cpp`: Error signaling

**Flow**:

```
User selects different day (in pre-play state)
    │
    ▼
ChartToolbar::onReplayDayChanged()
    │
    ├─ Guard: If Playing state, return (ignore change)
    │
    └─ If PreloadingPaused: emit replayDayChanged(date)
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
                    └─ If success: preload chart at configured replay start, pause
                            │
                            ▼
                    Chart updates with new day's data
```

Notes:
- Quiet-symbol replay starts can legitimately preload into a stretch with no real bars yet.
- In that case the chart remains anchored at the configured replay start time and waits for replay time to advance linearly toward the first real event, rather than snapping directly to the first trade/quote timestamp.

**Time Change Flow** (similar to day change):
```
User changes start time (in pre-play state)
    │
    ▼
ChartToolbar::onReplayTimeChanged()
    │
    ├─ Guard: If Playing state, return (ignore change)
    │
    └─ If PreloadingPaused: emit replayStartTimeChanged(time)
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
4. **On Pause Press**: Transition to `Paused` (day/time remain locked)
5. **On Restart**: Return to fresh pre-play state where day/time are editable
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
    setReplayState(ReplayState::Paused);   // Keep day/time locked after first play
}
```

### StockPriceChart Integration

**New Public Slots**:

```cpp
// Replay data loading error handler
void onReplayDataLoadFailed(const QString& errorMessage);

// Triggered when user changes day/time selection in fresh pre-play state
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
- `RequestMissingBars` emitted → `GUIFrontend::requestMissingBarsFromCache` → `MainAlgo::requestMissingBarsDisplayedStock`
- Received bars added to chart via `onRequestedMissingBarsReceived()`

**Token-based in-flight guard**: `m_currentMissingBarsRequestToken` (atomic uint64) prevents duplicate requests.
A non-zero token means a request is in flight; `checkForMissingBars` returns early if token ≠ 0.
`clearSymbol()` / timescale switch reset the token to 0, causing any later response to be ignored as stale.

**Automatic chain-fill**: When `onRequestedMissingBarsReceived` or `onRequestedMissingBarsFailed` completes,
it clears the token and calls `onAxisRangeChanged()` to re-trigger `checkForMissingBars`. This drives
day-by-day backfill automatically without user interaction.

**One-day-at-a-time fill**: `checkForMissingBars` always requests the day immediately before `firstBarTime`,
not the view start. This prevents skipping intermediate days when the user pans far left in one move.

### Known-Empty Date Tracking

`m_knownEmptyDates: QSet<QDate>` accumulates dates that produced zero bars (non-trading days, unknown
market closures). `checkForMissingBars` skips these in the `while` loop so the same date is never
retried after a failure.

**Holiday pre-check**: The `while` loop also calls `MarketCalendar::getHolidayName(date)` for every
candidate date. If the name is non-empty, the date is added to `m_knownEmptyDates` and
`drawHolidayDayMarker()` is called immediately — **no BarCache or API call is made**. This means
known NYSE holidays (Presidents' Day, MLK, etc.) render their grey watermark instantly when the
user first pans into them.

### Holiday Visual Markers

`drawHolidayDayMarker(date, name)` draws:
- A dark-grey `QCPItemRect` (`QColor(50,50,55)`) on the `"background"` layer spanning the full trading-hours day
- A rotated 90° `QCPItemText` centered on the day with the holiday name

Holiday names come from `MarketCalendar::getHolidayName()` in `Src/Misc/CONSTANTS.h`.
Cleanup is handled by `clearBackgroundRects()` (holiday rect in `m_holidayDayRects`, label in `m_holidayLabels`).
`clearBackgroundRects()` must dedupe pointers and only call `removeItem` for items still owned by
`QCustomPlot` to avoid aborts when symbol switches/replay resets happen back-to-back.

### Loading Spinner

A `QCPItemText` on the `"overlay"` layer positioned at `(0.01, 0.5)` in axis-rect-ratio coordinates
(always visible at the left edge, independent of pan position) provides an animated loading indicator.

- **Frames**: 10 braille-dot characters `⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏` cycled at 80ms (~12fps)
- **Start**: `startLoadingSpinner()` called in `checkForMissingBars` after token is set
- **Stop**: `stopLoadingSpinner()` called in `onRequestedMissingBarsReceived`, `onRequestedMissingBarsFailed`, `clearSymbol`, `clearChart`
- **Driver**: `m_loadingSpinnerTimer (QTimer, 80ms)` calls `rpQueuedReplot` on each tick

### Symbol Switch: Range Preservation and Stale-Data Prevention

`setSymbol(symbol)` is the single entry point for changing the displayed stock. It:
1. Captures `m_preservedXRange = xAxis->range()` if `m_index0Timestamp.isValid()` (i.e., chart has content)
2. Calls `clearSymbol()` — wipes all bar data, index maps, token, known-empty dates, holiday labels
3. Calls `clearOrderVisualizations()`
4. Sets `m_symbol`, updates watermark
5. Calls `initializeTimeAnchor()` — sets new index-0, applies `m_preservedXRange`, defers initial bar request

`initializeTimeAnchor()` also calls `drawSessionBackgroundsForDate(m_index0Timestamp.date())` immediately, so replay/live
symbol switches still show the session shading even when the selected symbol has not received its first bar for that
window yet (for example, a pre-market chart that stays empty until a 7:05 breakout).

**Y range is NOT preserved** on symbol switch — the new symbol trades at a different price level.
**X range IS preserved** — the user stays at the same calendar time window they were viewing.

`GUIFrontend::displayStock()` is the caller; it must NOT call `clearSymbol()` separately (it's done inside `setSymbol()`).

### Symbol Switch: Race Condition Guard

`setSymbol()` fires synchronously on the GUI thread. The `onSelectDisplayedStock(symbol)` signal to
`MainAlgo` is a **queued cross-thread connection** — `currentDisplayedStockInstrument` in MainAlgo
may still point to the old symbol when the first bar request fires.

**Guard in `requestMissingBarsFromCache`**:
```cpp
if (MainAlgo::getInstance()->getDisplayedSymbol() != ui->priceChart->getCurrentSymbol())
{
    QTimer::singleShot(50, this, [this, from, to]() { requestMissingBarsFromCache(from, to); });
    return;  // Token stays set; no date recorded
}
```
The 50ms retry keeps the in-flight token locked (preventing duplicate requests) and does not call
`onRequestedMissingBarsFailed()` (avoiding spurious known-empty date recording). By 50ms MainAlgo
has processed the queued event and routes subsequent requests to the correct BarCache.

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
4. Check replay files exist: `~/.local/share/OpenTraderPlatform/ReplayData/{YYYY-MM-DD}/{SYMBOL}_mbp10.dbn.zst` and the matching `_trades.dbn.zst` file
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
// Marker creation/updates
OrderMarker* createBuyMarker(...);
OrderMarker* createSellMarker(...);
OrderMarker* createCancelledMarker(...);
void updateMarkerState(OrderMarker* marker, OrderMarker::State newState);
void moveMarkerToPrice(OrderMarker* marker, double newPrice);

// Position line management
QCPItemLine* createPositionLine(...);
void updateOpenPositionDynamicLine(double currentPrice, int currentBarIndex);
void updateOpenPositionPLBox(double currentPrice); // sticky Y-border behavior with up/down cue
void createClosedPositionPLLabel(PositionVisualization* pos);

// Strategy bracket overlay
void applyBracketOverlayEvent(const StrategyBracketOverlayEntry& entry, bool replot);
void updateBracketOverlayVisuals();
// Active state is live-emitted by MainAlgo (managed bracket engine), not DB replay.

// Coordinate conversion
double getExactIndexForTimestamp(const QDateTime& timestamp);

// Cleanup
void clearOrderVisualizations();
void updateOrderVisualizationsVisibility();
```

### Toolbar Toggle

The ChartToolbar includes an "Orders" checkbox to show/hide all order visualizations,
including strategy bracket overlays:

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

Managed bracket overlays are synchronized from MainAlgo live state when symbol contexts are acquired/selected.

### Strategy Chart Artifacts

- **Bubble log markers**: `onStrategyLogEmitted()` creates fixed-size bottom dots (`QCPItemLogDot`) with hover tooltips.
- **Status panel**: `onStrategyStatusEmitted()` updates a dedicated lower status pane for the current symbol. Visibility is controlled by the toolbar `Status` checkbox.
- Status updates are live-session state (in-memory) and are not loaded from `OrdersDatabase`.

### BBO Overlay Notes

- Bid/ask horizontal guide lines originate from the replay/live time line (`m_currentTimeLine`).
- Lines terminate at the bid/ask label anchor, not at the full chart edge.
- Bid/ask prices are clamped to visible Y-range for stable rendering while panning/zooming.

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

## Dynamic Timescale Feature

### Overview

The chart supports multiple timeframes (1m, 5m, 15m, 30m, 1h, 4h, 1d, 1w, 1M) with seamless switching and automatic timeframe selection based on zoom level.

### Timeframe Selection

**Manual Selection** (Toolbar dropdown or keyboard shortcuts):
- `1` = 1-minute
- `2` = 5-minute
- `3` = 15-minute
- `4` = 30-minute
- `5` = 1-hour
- `6` = 4-hour
- `7` = 1-day
- `8` = 1-week
- `9` = 1-month

**Auto-Timeframe** (when "Auto" checkbox enabled):
- Automatically switches timeframes based on visible time range
- Each timeframe has configurable lower/upper thresholds (in minutes)
- Switching happens on zoom in/out

### Auto-Timeframe Thresholds

Configured in Config tab (`Src/FrontEnd/GUI/Tabs/ConfigTab.cpp`):

| Timeframe | Default Lower | Default Upper | Description |
|-----------|---------------|---------------|-------------|
| 1m | 30 min | 150 min | Switch up when > 2.5h visible |
| 5m | 120 min | 480 min | 2-8h comfortable range |
| 15m | 240 min | 960 min | 4-16h comfortable range |
| 30m | 480 min | 1440 min | 8-24h comfortable range |
| 1h | 720 min | 2880 min | 12h-2 days comfortable range |
| 4h | 1440 min | 10080 min | 1 day-1 week comfortable range |

Settings stored in `AppState.ini` under `Config/AutoTF/<timeframe>/Lower|Upper`.

### Timescale-Aware Replay Start Time

When switching timeframes in replay mode, the start time selector adjusts its step granularity:

| Timeframe | Display Format | Step Boundaries |
|-----------|----------------|-----------------|
| 1m | hh:mm | Every minute (no snapping) |
| 5m | hh:mm | 0, 5, 10, 15... minutes |
| 15m | hh:mm | 0, 15, 30, 45 minutes |
| 30m | hh:mm | 0, 30 minutes |
| 1h | hh:00 | Every hour |
| 4h | hh:00 | 0, 4, 8, 12, 16, 20 hours |

Implementation in `ChartToolbar::calculateSteppedTime()` and `snapTimeToStep()`.

### Candle Alignment

QCustomPlot centers candlesticks on their `key` value. To align the left edge of each candle with its bar's open time, we offset the key by `minutesPerBar / 2.0`:

```cpp
// In addLiveBar(), updateCandlestickData(), updateVolumeData():
const double keyOffset = BarUtils::minutesPerBar(m_displayTimeFrame) / 2.0;
const double displayKey = index + keyOffset;
```

This ensures:
- A 13:00 bar's left edge is at the 13:00 mark
- A 13:00 bar's right edge is at 13:05 (for 5m bars) or 13:01 (for 1m bars)

### Range Preservation

`m_preservedXRange: std::optional<QCPRange>` and `m_preservedYRange: std::optional<QCPRange>` hold
ranges across clear/reinit operations.

| Trigger | X preserved | Y preserved | Notes |
|---------|-------------|-------------|-------|
| Timescale switch | ✅ | ✅ | `preserveCurrentRanges()` before `clearChart()` |
| Symbol switch | ✅ | ❌ | Captured in `setSymbol()` when `m_index0Timestamp.isValid()` |
| First load | ❌ | ❌ | Default `(-60, 30)` for X; auto-fit from first bar batch for Y |

`initializeTimeAnchor()` consumes `m_preservedXRange` (resets it to `nullopt` after applying).
`onRequestedMissingBarsReceived()` consumes `m_preservedYRange` on the first bar batch.

**Important**: `setSymbol()` must capture the range BEFORE calling `clearSymbol()`, which resets
xAxis to `(0,30)`. Capturing after clear always yields the wrong `(0,30)` default.

### Key Files for Dynamic Timescale

| File | Responsibility |
|------|----------------|
| `ChartToolbar.h/cpp` | Timeframe selector, auto checkbox, step granularity |
| `ZoomAndPanning.cpp` | `checkAutoTimeFrame()` - auto-switching logic |
| `StockPriceChartBars.cpp` | Candle/volume positioning with offset |
| `StockPriceChart.cpp` | `setDisplayTimeFrame()`, range preservation |
| `ConfigTab.cpp` | Configurable auto-TF thresholds |
| `GUIFrontend.cpp` | `onTimeFrameChanged()` handler |

### Implementation Notes

1. **Stale Request Handling**: Uses atomic token (`m_currentMissingBarsRequestToken`) instead of semaphore to handle rapid timescale switching without crashes.

2. **No Oscillation**: Auto-TF uses minute-based thresholds (not bar counts) to prevent oscillation when switching changes the visible bar count.

3. **Order Marker Alignment**: `getExactIndexForTimestamp()` no longer subtracts 0.5 since candles are now aligned with their open time.

4. **Signal Blocking**: When auto-TF switches, `QSignalBlocker` is scoped to only block during `setCurrentTimeFrame()`, then the signal is emitted after to ensure GUIFrontend receives it.
