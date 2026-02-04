# Session Context: Replay Feature Implementation
**Session ID**: c63f40b9-bca2-46f1-a151-2a695a5944f7

## Overview
This session focuses on completing the replay functionality for L2Trader, a real-time algorithmic trading application. The replay feature allows users to replay recorded historical market data at various speeds, supporting strategy backtesting and debugging.

## Current Status (Phase Completion)
- **Phases 1-3**: ✅ COMPLETE (Core infrastructure, state machine, UI controls)
- **Testing Phase**: 🔄 IN PROGRESS (debugging timing issues)
- **Phase 4**: ⏳ PENDING (Simulated order execution - deferred)

## What Exists Already
1. **Recorder Application**: Functional, records live bars and market depth quotes to SQLite
   - Stores JSON data with monotonic sequence indices and timestamps
   - Separate database files per trading day
   - Located at: `~/.cache/L2Trader/RecordedLiveData/`

2. **UI Replay Controls** (in ChartToolbar):
   - Day selector dropdown
   - Time selector input
   - Play/Pause button
   - Info label showing available date range and bar count

3. **App Time Abstraction**: `MainApp::getCurrentAppTime()`
   - Static function used throughout app for time queries
   - Supports both live and replay modes
   - Timezone-aware (Eastern/NYSE timezone)

## Architecture Design Decisions

### 1. Data Source Mode vs Playback State (COMPLETED)
**Decision**: Separate `DataSourceMode` (Live/Replay) from `PlaybackState` (Stopped/Playing/Paused)

**Rationale**: 
- UI has two independent buttons: LIVE/REPLAY label and Play/Pause button
- Each controls different aspects of replay system
- Prevents double-enter bugs and improves clarity

**Implementation**:
- `MainApp::DataSourceMode` enum with Live/Replay values
- `isInReplayMode()` function returns `m_dataSourceMode == DataSourceMode::Replay`
- `ReplayEngine` handles PlaybackState independently
- New functions: `enterReplayMode()`, `exitReplayMode()`, `startReplayPlayback()`, `pauseReplayPlayback()`, `resumeReplayPlayback()`

### 2. Database Path Organization (COMPLETED)
**Issue**: Path mismatch between Recorder and ReplayDataLoader
- Recorder stored at: `RecordedLiveData/Bars/` and `RecordedLiveData/MarketDepthQuotes/`
- ReplayDataLoader originally looked for: `RecordedData/Bars/` and `RecordedData/MarketDepthQuotes/`

**Fix**: Updated ReplayDataLoader paths in commit 8419d13

### 3. Replay Data Injection Strategy (COMPLETED)
**Design**: Mock QNetworkReply to inject replay data with minimal changes to TSClient

**Components**:
- `MockNetworkReply`: Derived from QNetworkReply, simulates API responses
- `ReplayDataLoader`: Reads SQLite and feeds data through MockNetworkReply
- `TSClient`: Receives mocked replies, treats them identically to live API responses
- Streams (StreamBars, StreamMarketDepthQuote): Receive data same way whether from live API or replay

**Benefit**: Strategies and UI work identically in live and replay modes

### 4. Timing Model (COMPLETED)
- **Bar timestamps**: Represent bar closing time
- **Replay timing**: 
  - Load first bar, record its timestamp as earliest replay start time
  - Read subsequent bars, calculate delay between timestamps
  - Use QTimer to emit bars at proper intervals based on configured speed
  - Speed multiplier: 1x = real-time, 10x = 10x faster, etc.

### 5. Data Loading Strategy (COMPLETED)
**Approach**:
- Load entire day's recorded data for selected stocks into SQLite
- Sequential playback using monotonic indices
- No backward seeking (initially)

**Files Involved**:
- `Src/Core/Replay/ReplayEngine.cpp`: Time and speed management
- `Src/Core/Replay/ReplayDataLoader.cpp`: SQLite access, data feeding
- `Src/Clients/MockNetworkReply.h/cpp`: Simulated network responses

## Current Implementation Status

### Completed Files
1. **MainApp** (Src/Core/MainApp.h/cpp)
   - `DataSourceMode` enum (Live, Replay)
   - `m_dataSourceMode` static member variable
   - State machine functions with ASSUME assertions for logic bugs
   - Integration with `getCurrentAppTime()` to use replay time when active

2. **ReplayEngine** (Src/Core/Replay/ReplayEngine.h/cpp)
   - PlaybackState management (Stopped/Playing/Paused)
   - Speed control (with before-play locking)
   - Timer-based data delivery coordination

3. **ReplayDataLoader** (Src/Core/Replay/ReplayDataLoader.h/cpp)
   - SQLite database access with correct paths
   - Data feeding mechanism via callbacks
   - Multi-stream support (bars + market depth)

4. **MockNetworkReply** (Src/Clients/MockNetworkReply.h/cpp)
   - Simulates QNetworkReply for testing
   - Feeds replay data to Stream classes

5. **GUIFrontend** (Src/FrontEnd/GUI/GUIFrontend.cpp)
   - Updated LIVE/REPLAY label click handler
   - Proper mode switching without auto-play
   - Replay widgets reset on mode changes

6. **StockPriceChart** (Src/FrontEnd/GUI/StockPriceChart/StockPriceChart.cpp)
   - Updated Play button handler
   - Calls `startReplayPlayback()` and `pauseReplayPlayback()` appropriately
   - Play/Pause toggle state management

7. **ChartToolbar** (Src/FrontEnd/GUI/StockPriceChart/ChartToolbar.cpp)
   - Date/time selectors for replay start point
   - Play button state reset on mode changes

8. **Trading Session Display** (Src/FrontEnd/GUI/)
   - `getCurrentSession()` enum for market session states
   - Top UI label showing current session (works in live and replay modes)

### Known Issues / In Progress

1. **Now Line Timing** (Debugging)
   - Vertical line showing current time appears 1 minute behind actual time
   - Issue occurs in both live and replay modes
   - Located in: `StockPriceChart::updateCurrentTimeLine()` (~line 1480)
   - **Status**: Awaiting clarification on exact expected behavior
   - Previous fix attempt reverted pending user guidance

## Technical Decisions

### Threading Model
- Stack-allocated `QThread` member variables (not heap pointers)
- `ReplayEngine` runs in `MainAlgo` thread (inherited from core algo structure)
- Proper cleanup with quit/wait/terminate pattern
- See: `Src/Core/MainApp.cpp` destructor and thread initialization

### Memory Management
- Prefer composition over pointers for owned objects
- Use `std::shared_ptr<QVector<Bar>>` for data shared across async operations
- `QPointer<T>` for Qt objects that may be deleted independently
- No smart pointers needed for Qt objects with parents

### Error Handling
- Use `ASSUME()` macros from `Src/Misc/Assume.h` for logic bugs
- Assertions during development catch programming errors early
- Examples: calling `enterReplayMode()` when already in replay mode

### Constants Management
- All replay-related constants in `Src/Misc/CONSTANTS.h`
- Namespace: `TradingHours` (for timing), `FileSystemConstants` (for paths)
- Centralized for easy maintenance

## Files Modified in This Session

### Key Commits
1. **State Machine Fix** (a6f3479)
   - Separated DataSourceMode from PlaybackState
   - Added ASSUME assertions for logic bugs
   - Updated UI flow for mode switching

2. **Path Fix** (8419d13)
   - Updated ReplayDataLoader paths: `RecordedData/` → `RecordedLiveData/`
   - Aligned with Recorder output paths

3. **Bar Equality Operator** (1b92081)
   - Added operator== to Bar struct for data validation
   - Supports replay verification

## Next Steps

### Immediate (Testing Phase)
1. **Verify replay playback works**
   - Data loads correctly from SQLite
   - Bars appear in chart with proper timing
   - Chart updates as replay progresses

2. **Debug "now line" timing issue**
   - Clarify expected vs actual behavior
   - Fix timing calculation in `updateCurrentTimeLine()`
   - Test in both live and replay modes

3. **Test pause/resume functionality**
   - Pause stops time advancement
   - Resume continues from paused point
   - Bars are not re-emitted

### Phase 4 (Deferred)
**Simulated Order Execution**
- Allow MainAlgo strategy to place orders during replay
- Track positions without actual API calls
- OrdersReceiver already captures simulated orders (needs integration)

### Phase 5-7 (Later)
- Additional UI polish and state coordination
- Comprehensive testing suite
- Performance optimization if needed

## Dependencies and Integration Points

### Core Dependencies
- Qt6 (Core, Network, SQL, Widgets)
- SQLite3 (for recorded data)
- QDateTime with NYC timezone handling

### Integration Points
1. **MainApp**: Central state management, time provider
2. **TSClient**: Receives mocked network replies during replay
3. **Streams**: Process data identically whether live or replay
4. **MainAlgo**: Strategy execution with replay state awareness
5. **BarCache**: Handles both historical queries and replay data
6. **OrdersReceiver**: Tracks simulated orders during replay
7. **GUIFrontend**: Displays replay controls and status
8. **StockPriceChart**: Shows replay data with timing overlays

## Architecture Diagrams

```
User Input Flow:
┌─────────────────────────────────────────────────────────────────┐
│  User clicks LIVE/REPLAY label                                   │
├─────────────────────────────────────────────────────────────────┤
│  GUIFrontend::eventFilter() calls toggleReplayMode()             │
│         ↓                                                         │
│  Calls MainApp::exitReplayMode() or enterReplayMode()            │
│         ↓                                                         │
│  Emits replayModeEntered/Exited() signals                        │
│         ↓                                                         │
│  GUIFrontend shows/hides replay widgets                          │
│  ChartToolbar resets play button state                           │
└─────────────────────────────────────────────────────────────────┘

Playback Flow:
┌─────────────────────────────────────────────────────────────────┐
│  User selects date/time and clicks Play button                  │
├─────────────────────────────────────────────────────────────────┤
│  StockPriceChart::onPlayClicked() calls startReplayPlayback()   │
│         ↓                                                         │
│  MainApp::startReplayPlayback() initializes replay               │
│         ↓                                                         │
│  MainAlgo::enterReplayMode() creates ReplayEngine               │
│         ↓                                                         │
│  ReplayEngine creates ReplayDataLoader                           │
│         ↓                                                         │
│  ReplayDataLoader loads SQLite and feeds data via timers        │
│         ↓                                                         │
│  Data flows through MockNetworkReply → Streams → BarCache      │
│         ↓                                                         │
│  MainAlgo processes bars and executes strategy                   │
│         ↓                                                         │
│  Chart updates with replay data                                  │
└─────────────────────────────────────────────────────────────────┘
```

## Testing Checklist

- [ ] Build succeeds for both Debug and Release
- [ ] No compiler warnings
- [ ] Unit tests pass
- [ ] Manual test: Click LIVE/REPLAY label, verify widgets appear
- [ ] Manual test: Select date and click play, verify data flows
- [ ] Manual test: Pause button works and pauses playback
- [ ] Manual test: Resume continues from pause point
- [ ] Manual test: Multiple stocks replay simultaneously
- [ ] Manual test: Speed controls work (1x, 2x, 10x, etc)
- [ ] Manual test: Strategy executes during replay
- [ ] Manual test: Positions update during replay
- [ ] Manual test: Now line shows current time correctly
- [ ] Manual test: Replay mode exit restores live streams

## Known Limitations / Deferred

1. **No backward seeking**: Currently only supports forward playback
2. **No speed change during playback**: Speed locked before play starts
3. **No simulated orders yet**: Phase 4, will be implemented later
4. **No TUI support**: TUI template exists but replay UI not implemented
5. **Single day replay**: Cannot span multiple days (by design)
6. **No pre-market simulation**: Uses live pre-market data, not recorded

## Documentation

- Main plan in: `plan.md` (this repository branch)
- Session context: This file
- Architecture details: `Doc/Architecture_Improvements.md`
- Trading session info: `Doc/TradingSession.md`
- Recorder docs: `Doc/Recorder.md`
