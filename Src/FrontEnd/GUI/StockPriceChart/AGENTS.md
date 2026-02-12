# StockPriceChart/ - Real-Time Price Chart with Replay Mode - Agent Instructions

## Overview

**Location**: `Src/FrontEnd/GUI/StockPriceChart/`
**Purpose**: Display live candlestick charts with real-time data, historical data loading, and market replay functionality
**Main Classes**: `StockPriceChart`, `ChartToolbar`, `IndexToTimeTicker`

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
