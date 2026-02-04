# Replay Feature Implementation Status

## Completed (✅)

### State Machine Fix - DONE
- [x] Separated DataSourceMode (Live/Replay) from PlaybackState (Playing/Paused/Stopped)
- [x] Fixed double-enter bug in enterReplayMode()
- [x] Fixed path mismatch (RecordedData → RecordedLiveData) in ReplayDataLoader
- **Commits**: a6f3479, 8419d13

### Core Infrastructure - DONE (Phases 1-3)
- [x] ReplayEngine with PlaybackState and speed control
- [x] ReplayDataLoader with database access
- [x] MockNetworkReply for data injection
- [x] TSClient replay mode setup
- [x] UI replay controls in ChartToolbar
- [x] App restart via execv()
- [x] Trading session display

### In Progress (🔄)

### Testing Needed (⏳)
- [ ] Verify replay starts and plays data correctly
- [ ] Verify bars flow through chart
- [ ] Verify speed controls work
- [ ] Verify pause/resume works

## Next Steps - Replay Functionality Testing

After confirming the basic replay playback works, remaining items are:

1. **Phase 4: Simulated Order Execution** - Allow strategy to place orders during replay
2. **Phase 5-6: UI & State Coordination** - Additional UI polish
3. **Phase 7: Testing & Polish** - Comprehensive testing

---

# Replay State Machine Fix (COMPLETED)

## Problem Statement

The current replay implementation has a state management bug: clicking the "Replay" button (LIVE/REPLAY label) AND clicking the "Play" button in the chart toolbar BOTH call `enterReplayMode()`. This causes the warning:
```
[19:33:11.776] WARN : Already in replay mode, ignoring enterReplayMode call
```

The root cause is conflating two distinct concepts:
1. **Data Source Mode**: Live vs Replay (where data comes from)
2. **Playback State**: Playing vs Paused (whether replay is actively advancing)

## Current Architecture

- `MainApp::isInReplayMode` - Boolean flag (too simple)
- `ReplayEngine::PlaybackState` - Already has `Stopped`, `Playing`, `Paused`
- UI has two buttons with overlapping responsibility:
  - LIVE/REPLAY label → currently enters/exits replay mode
  - Play/Pause button → should start/stop playback

## Proposed Solution

Replace the boolean `isInReplayMode` with a proper state enum that clearly separates the two concepts. The ReplayEngine already has the right internal states; we need to expose this properly at the MainApp level and wire up the UI correctly.

### New State Model

```
DataSourceMode:
├── Live       - Connected to live TradeStation streams
└── Replay     - Using recorded data
    └── PlaybackState (from ReplayEngine):
        ├── Stopped  - Replay active but not started
        ├── Playing  - Data advancing
        └── Paused   - Frozen in time
```

### UI Flow Change

**Current (broken):**
- Click LIVE label → enterReplayMode() with play
- Click Play → enterReplayMode() again (duplicate!)

**Proposed:**
- Click LIVE label → Switch to Replay mode (Stopped state - shows widgets, doesn't play)
- Select date/time → Pick replay parameters
- Click Play → Start playback (Playing state)
- Click Pause → Pause playback (Paused state)  
- Click REPLAY label → Exit replay mode entirely

## Implementation Tasks

- [x] **1. Update MainApp state model**
  - Replace `bool isInReplayMode` with `DataSourceMode` enum (Live, Replay)
  - Add helper functions for state queries
  - Update `getCurrentAppTime()` to use new enum

- [x] **2. Split enterReplayMode into two functions**
  - `enterReplayMode()` - Switches to Replay mode (widgets show, no playback)
  - `startReplayPlayback(date, time, speed)` - Actually starts playback
  - Add `pauseReplayPlayback()` and `resumeReplayPlayback()` for pause/resume

- [x] **3. Update GUIFrontend eventFilter**
  - LIVE/REPLAY label click should just toggle data source mode, not start playback
  - When entering replay mode, don't auto-start - just show widgets

- [x] **4. Update StockPriceChart play button handler**
  - Play button should call `startReplayPlayback()` not `enterReplayMode()`
  - Handle Play/Pause toggling properly

- [x] **5. Update ChartToolbar button states**
  - Reset play button to stopped state on mode enter/exit

- [x] **6. Fix related state checks throughout codebase**
  - Converted `isInReplayMode` from static bool to static function

- [x] **7. Build and test**
  - Build project ✓
  - Ready for manual testing

## Considerations

1. **Backward compatibility**: Keep behavior the same from user perspective, just fix the double-enter bug
2. **Thread safety**: MainApp state accessed from GUI thread, ReplayEngine runs in MainAlgo thread
3. **Signals/Slots**: May need new signals for playback state changes
4. **Use ASSUME for logic bugs**: Calling `enterReplayMode()` when already in replay mode (or `exitReplayMode()` when not in replay) is a **programming error**, not a runtime condition. Replace `qWarning()` with `ASSUME()` macro to catch these logic bugs during development.

---

# Replay Functionality Implementation Plan (Overall Feature)

## Overview

This plan outlines the implementation of a comprehensive market data replay system for L2Trader. The replay feature will allow users to replay recorded historical market data (bars and Level 2 market depth quotes) at various speeds, including real-time and "as fast as possible" modes. The system will support running trading strategies during replay with simulated order execution.

## Requirements Summary

Based on the requirements clarification:

1. **Hybrid Mode**: Pause live streams when entering replay, resume when exiting
2. **Multi-Stock Replay**: Replay all stocks recorded in the selected day's database
3. **Speed Control**: Configurable playback speed (0.5x, 1x, 2x, 5x, 10x, "as fast as possible")
   - **Speed is set before playback and locked** - cannot change during replay
4. **Strategy Execution**: Strategies run during replay and generate signals
   - **Strategies can query replay state** via StrategySDK (`isInReplayMode()`, `getReplaySpeed()`)
5. **Simulated Orders**: Paper trading only - track positions without real orders
6. **Market Depth**: Replay both bars and market depth quotes synchronously
7. **Simple Navigation**: Jump to time and play forward (no backward seeking initially)
8. **State Management**: Clear positions/orders on replay start, restore streams on exit
9. **Start Time Behavior**: 
   - **Load replay data from user-selected start time forward** (e.g., 10:15 AM)
   - **Strategy requests historical context** via SDK (just like live mode)
   - BarCache handles requests:
     - First checks BarCache database (regular cache)
     - If not cached, queries BarCache database (may require API call if truly cold start)
   - **Replay database ONLY used for live bars from startTime onward**
   - Clean separation: historical = BarCache, live replay = ReplayDataLoader

## Current State Analysis

### Existing Infrastructure

✅ **Recorder Application**: Functional, records bars and market depth to SQLite
- Database schema: `bars` and `market_depth_quotes` tables
- Stores: `stockTicker`, `stockTickerSeq`, `epochMs`, `jsonRawData` (raw JSON from API)
- Separate DB files per day

✅ **UI Components**: ChartToolbar has replay controls
- Day selector combo box
- Time selector (start time)
- Play/Pause button (currently not connected)
- Info label for showing time range and bar count

✅ **Time Abstraction**: `MainApp::getCurrentAppTime()`
- Static function returning `QDateTime`
- `MainApp::isInReplayMode` flag
- `MainApp::currentAppReplayTime` variable
- Already used in: `OrdersReceiver`, `BarCache`, `TUIFrontend`

✅ **Database Scanning**: ChartToolbar can scan recorded data directory
- Populates available days from filesystem
- Queries database for time ranges and bar counts

### Architecture Context

- **TSClient**: Singleton managing all TradeStation API communication
- **MainAlgo**: Core trading logic, coordinates stock screening and strategy execution
- **Streams**: `StreamBars`, `StreamMarketDepthQuote`, `StreamPositions`, `StreamOrders`
- **Threading**: TSClient runs in separate thread, MainAlgo in another
- **Signal/Slot**: Cross-thread communication via Qt signals

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         Replay System                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌──────────────┐      ┌──────────────┐      ┌──────────────┐  │
│  │ ReplayEngine │◄─────┤ ReplayLoader │◄─────┤  SQLite DB   │  │
│  │              │      │              │      │              │  │
│  │ - Time mgmt  │      │ - Ping-pong  │      │ Recorded     │  │
│  │ - Speed ctrl │      │   buffers    │      │ market data  │  │
│  │ - Playback   │      │ - Merge data │      └──────────────┘  │
│  └──────┬───────┘      └──────────────┘                         │
│         │                                                        │
│         │ Inject via injectData()                               │
│         ▼                                                        │
│  ┌──────────────────────────────────────────────────────┐      │
│  │        TSClient (Mode: Replay)                        │      │
│  │                                                         │      │
│  │  openStreamBars() → returns Stream(MockNetworkReply)  │      │
│  │  MockNetworkReply emits readyRead() with replay data  │      │
│  └──────┬───────────────────────────────────────────────┘      │
│         │                                                        │
└─────────┼────────────────────────────────────────────────────────┘
          │
          │ Same Stream/callback flow as live mode
          ▼
   ┌─────────────────────────────────────────────────┐
   │             Application Layer                    │
   │                                                   │
   │  MainAlgo → Strategies → SimulatedOrderExecutor │
   │                                                   │
   └───────────────────────────────────────────────────┘
```

## Detailed Replay Data Flow

### 1. Replay Initialization
```
User selects replay day and start time in UI (e.g., 2024-01-15, 10:15 AM)
   ↓
User sets playback speed (0.5x, 1x, 2x, etc.)
   ↓
User clicks "Play" button
   ↓
ChartToolbar emits replayPlayPauseToggled(true)
   ↓
MainApp::enterReplayMode(date, startTime, speed)
   ↓
1. Set MainApp::isInReplayMode = true
2. Set MainApp::currentAppReplayTime = startTime
3. Set MainApp::currentReplaySpeed = speed
4. TSClient::setMode(Mode::Replay)
5. Close positions/orders streams (via MainAlgo)
   - MainAlgo owns PositionsReceiver and OrdersReceiver
   - These receivers hold the stream QPointers
   - Call positionsReceiver->stopStream() and ordersReceiver->stopStream()
6. MainAlgo::enterReplayMode(date, startTime, speed)
   ↓
MainAlgo creates/initializes ReplayEngine
   ↓
ReplayEngine::startReplay(date, startTime, speed)
   ↓
ReplayDataLoader::loadDatabase(date)
   ↓
IMPORTANT: Load ONLY from user-selected startTime forward (e.g., 10:15 AM)
- Replay database contains recorded "live" bars/quotes
- Used ONLY for streaming data from startTime onward
- Historical context (before startTime) NOT preloaded
   ↓
ReplayDataLoader loads data:
- Open bars.db and market_depth.db for the selected date
- Query records WHERE epochMs >= startTime
- Initialize ping-pong buffers with first 1000 records (from startTime)
- Prefetch second chunk asynchronously
   ↓
ReplayEngine begins normal replay loop:
- Emit first bar at startTime (e.g., 10:15 AM)
- Strategy receives first live bar
- Strategy may then request historical bars via SDK:
    strategy->sdk->getBars(date, 6:01 AM, 10:15 AM)
    ↓
    BarCache handles request (separate from replay data):
    - Check BarCache database (regular cache)
    - If not cached, load from BarCache DB or API if cold
    - Replay database NOT used for historical requests
```

**Key Principle**: 
- **Replay database** = "live" bars from startTime forward (streaming)
- **BarCache** = historical bars before startTime (on-demand query)
- Strategy doesn't know the difference from live mode

### 2. Replay Playback Loop
```
QTimer fires
   ↓
ReplayEngine::onTimerTick()
   ↓
1. Get current data point from ReplayDataLoader
2. Update MainApp::currentAppReplayTime to record's timestamp
3. Check if stream exists for this stock:
   - hasStreamForStock(symbol, type)?
   - If NO → skip this record, move to next
   - If YES → proceed to inject
   ↓
4. Get MockNetworkReply for this stock from TSClient:
   - MockNetworkReply* reply = tsClient->getBarReplyForSymbol(symbol)
   ↓
5. Inject data:
   reply->injectData(jsonRawData)
   ↓
   MockNetworkReply::injectData():
   - Append jsonRawData to m_buffer
   - emit readyRead()
   ↓
6. Stream's onReplyReadyRead() slot triggered:
   - Calls m_networkReply->readAll()
   - QNetworkReply internally calls MockNetworkReply::readData()
   - MockNetworkReply returns buffered data
   - Stream processes via processRawData()
   - Stream emits receivedNewRawData() and processes JSON
   ↓
7. Calculate delta to next record:
   - nextDelta = nextRecord.epochMs - currentRecord.epochMs
   - scaledDelta = applyPlaybackSpeed(nextDelta)
     * 1x: delta unchanged
     * 2x: delta / 2
     * 0.5x: delta * 2
     * As fast as possible: 0ms
   ↓
8. Schedule next timer:
   - m_playbackTimer->start(scaledDelta)
   ↓
9. Check buffer status:
   - If active buffer nearing empty, swap ping-pong buffers
   - Trigger async prefetch of next chunk
   ↓
[Loop repeats until replay stopped or data exhausted]
```

### 3. Stream Opening During Replay
```
MainAlgo calls openStreamBars(symbol)
   ↓
TSClient::openStreamBars(symbol, ...)
   ↓
Check mode:
   if (m_mode == Mode::Replay) {
      // Create MockNetworkReply
      MockNetworkReply* mockReply = new MockNetworkReply(this);
      
      // Store in tracking map
      m_replayBarReplies[symbol] = mockReply;
      
      // Create Stream with mock reply
      StreamBars* stream = new StreamBars(mockReply, ...);
      
      return stream;
   } else {
      // Normal live mode flow
      QNetworkReply* reply = m_networkManager->get(request);
      StreamBars* stream = new StreamBars(reply, ...);
      return stream;
   }
```

### 4. Replay Exit
```
User clicks "Stop" or exits replay mode
   ↓
MainApp::exitReplayMode()
   ↓
1. ReplayEngine::stopReplay()
   - Stop timer
   - Clean up ReplayDataLoader
   ↓
2. Set MainApp::isInReplayMode = false
   ↓
3. TSClient::setMode(Mode::Live)
   ↓
4. Reopen positions/orders streams (via MainAlgo)
   - MainAlgo instructs PositionsReceiver to reopen stream
   - MainAlgo instructs OrdersReceiver to reopen stream
   - Reopening triggers snapshot resend from TradeStation
   ↓
5. Clear replay-created bar/depth streams
   - MockNetworkReply-backed streams deleted
   - TSClient clears m_replayBarReplies and m_replayDepthReplies maps
   ↓
6. UI returns to live mode
```

**Stream Ownership Notes**:
- **PositionsReceiver** and **OrdersReceiver** are owned by MainAlgo
- These receivers hold `QPointer<StreamPositions>` and `QPointer<StreamOrders>`
- Stream lifecycle managed through receivers' `stopStream()` and creation methods
- MainApp coordinates the transition but MainAlgo controls the actual streams

### Key Design Points

1. **Transparency**: MainAlgo doesn't know if it's in replay or live mode. It calls `openStreamBars()` the same way.

2. **Least Invasive**: Stream class unchanged. Only TSClient modified to return different QNetworkReply types.

3. **Memory Efficiency**: Ping-pong buffers handle large datasets without loading everything at once.

4. **Skip Unopened**: Only inject data for stocks with active streams. Reduces processing overhead.

5. **Discrete Time**: `getCurrentAppTime()` returns last emitted record's timestamp. Simple and correct.

6. **Speed Control**: Timer delays scaled by playback speed. 0ms timer for "as fast as possible" gives event loop breathing room.

7. **Historical vs Live Separation**:
   - **Replay database**: Only for streaming "live" bars from startTime forward
   - **BarCache**: Handles all historical bar requests (before startTime)
   - Strategy uses same API (`sdk->getBars()`) in both live and replay modes
   - BarCache routes to regular cache/database, NOT replay database

8. **Newline Delimiters**: Stream class parses newline-delimited JSON. MockNetworkReply::injectData() ensures data ends with '\n' as a safeguard. Recorded data should already include newlines from original stream.

## Threading Architecture

### Component Thread Ownership

**MainAlgoThread**:
- `MainAlgo` object lives here
- `ReplayEngine` composed into MainAlgo (parent=MainAlgo, same thread)
- All replay timing and orchestration logic
- Non-blocking for GUI

**Main/GUI Thread**:
- `MainApp` coordinates replay transitions
- Sets global flags (`isInReplayMode`, `currentAppReplayTime`)
- Invokes MainAlgo methods via `QMetaObject::invokeMethod` (cross-thread)
- UI updates and user interactions

**TSClient Thread**:
- Handles mode switching (Live/Replay)
- Manages MockNetworkReply objects
- **Receives data injection signals from ReplayEngine** (via QueuedConnection)
- Calls `MockNetworkReply::injectData()` in its own thread context

**Background Workers** (QtConcurrent):
- ReplayDataLoader async database prefetching
- Runs in QThreadPool

### Cross-Thread Data Injection

ReplayEngine (MainAlgoThread) must inject data into MockNetworkReply (TSClient thread).
Direct method calls would be unsafe. Solution: **Signal/Slot with QueuedConnection**.

```cpp
// In ReplayEngine
signals:
    void injectBarData(const QString& symbol, std::shared_ptr<const QByteArray> data);
    void injectDepthData(const QString& symbol, std::shared_ptr<const QByteArray> data);

// In TSClient
public slots:
    void onInjectBarData(const QString& symbol, std::shared_ptr<const QByteArray> data);
    void onInjectDepthData(const QString& symbol, std::shared_ptr<const QByteArray> data);
```

**Why `std::shared_ptr<const QByteArray>`?**
- Avoids expensive deep copy when crossing thread boundary
- `QByteArray` has implicit sharing, but QueuedConnection still copies for thread safety
- `std::shared_ptr` transfers ownership semantically with minimal overhead
- `const` ensures replay data cannot be accidentally modified

**Connection setup** (in MainAlgo or MainApp):
```cpp
connect(m_replayEngine, &ReplayEngine::injectBarData,
        TSClient::getInstance(), &TSClient::onInjectBarData,
        Qt::QueuedConnection);
```

**TSClient slot implementation**:
```cpp
void TSClient::onInjectBarData(const QString& symbol, std::shared_ptr<const QByteArray> data)
{
    // Already in TSClient thread - safe to access m_replayBarReplies
    if (m_replayBarReplies.contains(symbol) && !m_replayBarReplies[symbol].isNull())
    {
        m_replayBarReplies[symbol]->injectData(*data);
    }
}
```

### Component Relationships

```
MainApp (GUI thread)
   │
   ├─► TSClient (TSClient thread)
   │
   └─► MainAlgo (MainAlgo thread)
         │
         └─► ReplayEngine (same thread, via parent)
               │
               └─► ReplayDataLoader (async via QtConcurrent)
```

### MainAlgo as Delegation Layer

MainAlgo owns ReplayEngine and delegates to it:

```cpp
class MainAlgo : public QObject
{
    ReplayEngine* m_replayEngine = nullptr;
    
public slots:
    void enterReplayMode(QDate date, QTime startTime) {
        if (!m_replayEngine) {
            // Parent = this → runs in MainAlgoThread automatically
            m_replayEngine = new ReplayEngine(this, TSClient::getInstance());
            connect(m_replayEngine, &ReplayEngine::replayTimeUpdated,
                    this, &MainAlgo::replayTimeUpdated);
        }
        m_replayEngine->startReplay(date, startTime);
    }
    
    void exitReplayMode() {
        if (m_replayEngine) {
            m_replayEngine->stopReplay();
        }
    }
    
signals:
    void replayTimeUpdated(QDateTime currentTime);  // Forward to UI
};
```

**Benefits**:
- Clean separation (MainAlgo delegates, ReplayEngine implements)
- ReplayEngine testable in isolation
- No GUI blocking (runs in MainAlgoThread)
- No new thread needed (inherits MainAlgo's thread via parent)
- Follows composition over inheritance principle

## Implementation Phases

### Phase 1: Core Replay Infrastructure

### Phase 1: Core Replay Infrastructure

#### 1.1 ReplayEngine Class
**Purpose**: Encapsulate all replay orchestration logic, composed into MainAlgo

**Location**: `Src/Core/Replay/ReplayEngine.h/cpp`

**Ownership**: Created by MainAlgo, runs in MainAlgoThread (no separate thread)

**Responsibilities**:
- Manage replay state (playing, paused, stopped)
- Control replay speed and timing (0.5x, 1x, 2x, 5x, 10x, "as fast as possible")
- Own and coordinate ReplayDataLoader
- Calculate timing deltas between records and schedule next emission
- Update `MainApp::currentAppReplayTime` (discrete jumps, not interpolated)
- Inject data into appropriate MockNetworkReply in TSClient
- Handle unopened streams (skip data if no stream exists for stock)
- Emit signals for UI updates (cross-thread to GUI)

**Timing Strategy**:
- Read record at index N with timestamp T_N
- Read next record at index N+1 with timestamp T_{N+1}
- Calculate delta: Δ = T_{N+1} - T_N
- Scale by playback speed:
  - 1x: use Δ as-is
  - 2x: Δ / 2
  - 0.5x: Δ * 2
  - "As fast as possible": 0ms (use QTimer with 0ms to give event loop breathing room)
- Schedule QTimer with scaled delta
- When timer fires: emit record N+1, update `currentAppReplayTime` to T_{N+1}, repeat

**Class Definition**:
```cpp
class ReplayEngine : public QObject
{
    Q_OBJECT
    
public:
    enum class PlaybackState { Stopped, Playing, Paused };
    enum class PlaybackSpeed { 
        Half = 50,       // 0.5x
        Normal = 100,    // 1.0x
        Double = 200,    // 2.0x
        Fast5x = 500,    // 5.0x
        Fast10x = 1000,  // 10.0x
        AsFastAsPossible = -1
    };
    
    explicit ReplayEngine(QObject* parent, TSClient* tsClient);
    ~ReplayEngine();
    
    void startReplay(QDate date, QTime startTime);
    void stopReplay();
    void pauseReplay();
    void resumeReplay();
    void setPlaybackSpeed(PlaybackSpeed speed);
    
    PlaybackState getState() const { return m_state; }
    PlaybackSpeed getSpeed() const { return m_speed; }
    
signals:
    void replayStarted();
    void replayStopped();
    void replayTimeUpdated(QDateTime currentTime);
    
private slots:
    void onTimerTick();  // Process next data point
    
private:
    QTimer* m_playbackTimer = nullptr;
    ReplayDataLoader* m_dataLoader = nullptr;
    TSClient* m_tsClient;  // Reference for data injection
    
    PlaybackState m_state = PlaybackState::Stopped;
    PlaybackSpeed m_speed = PlaybackSpeed::Normal;
    
    ReplayDataLoader::ReplayDataPoint m_currentPoint;
    qint64 m_lastTimestampMs = 0;
    
    void emitCurrentDataPoint();
    void scheduleNextDataPoint();
    qint64 calculateScaledDelta(qint64 deltaMs) const;
    bool hasStreamForStock(const QString& symbol, 
                          ReplayDataLoader::ReplayDataPoint::Type type) const;
};
```

**Unopened Stream Handling**:
- Before injecting data: check if TSClient has a stream for that stock+type
- If no stream exists: skip the data point, move to next one
- This allows replay to only process stocks that are actively monitored

**Thread**: Runs in MainAlgoThread (parent is MainAlgo which is in that thread)

**Responsibilities**:
- Manage replay state (playing, paused, stopped)
- Control replay speed and timing (0.5x, 1x, 2x, 5x, 10x, "as fast as possible")
- Coordinate data loading via ReplayDataLoader
- Calculate timing deltas between records and schedule next emission
- Update `MainApp::currentAppReplayTime` (discrete jumps, not interpolated)
- Inject data into appropriate MockNetworkReply in TSClient
- Handle unopened streams (skip data if no stream exists for stock)
- Emit time ticks for UI updates

**Timing Strategy**:
- Read record at index N with timestamp T_N
- Read next record at index N+1 with timestamp T_{N+1}
- Calculate delta: Δ = T_{N+1} - T_N
- Scale by playback speed:
  - 1x: use Δ as-is
  - 2x: Δ / 2
  - 0.5x: Δ * 2
  - "As fast as possible": 0ms (use QTimer with 0ms to give event loop breathing room)
- Schedule QTimer with scaled delta
- When timer fires: emit record N+1, update `currentAppReplayTime` to T_{N+1}, repeat

**Key Components**:
```cpp
class ReplayEngine : public QObject
{
    Q_OBJECT
public:
    enum class PlaybackState { Stopped, Playing, Paused };
    enum class PlaybackSpeed { 
        Half = 50,       // 0.5x (multiply deltas by 2.0)
        Normal = 100,    // 1.0x (multiply deltas by 1.0)
        Double = 200,    // 2.0x (multiply deltas by 0.5)
        Fast5x = 500,    // 5.0x (multiply deltas by 0.2)
        Fast10x = 1000,  // 10.0x (multiply deltas by 0.1)
        AsFastAsPossible = -1  // 0ms timer
    };
    
    void startReplay(QDate date, QTime startTime);
    void pauseReplay();
    void resumeReplay();
    void stopReplay();
    void setPlaybackSpeed(PlaybackSpeed speed);
    
signals:
    void replayStarted();
    void replayStopped();
    void replayTimeUpdated(QDateTime currentTime);
    
private slots:
    void onTimerTick();  // Process next data point
    
private:
    QTimer* m_playbackTimer;
    ReplayDataLoader* m_dataLoader;
    TSClient* m_tsClient;  // Reference to inject data
    
    PlaybackState m_state;
    PlaybackSpeed m_speed;
    
    ReplayDataLoader::ReplayDataPoint m_currentPoint;
    qint64 m_lastTimestampMs = 0;
    
    void emitCurrentDataPoint();
    void scheduleNextDataPoint();
    qint64 calculateScaledDelta(qint64 deltaMs) const;
    bool hasStreamForStock(const QString& symbol, 
                          ReplayDataLoader::ReplayDataPoint::Type type) const;
};
```

**Unopened Stream Handling**:
- Before injecting data: check if TSClient has a stream for that stock+type
- If no stream exists: skip the data point, move to next one
- This allows replay to only process stocks that are actively monitored

**Thread**: Runs in MainAlgo thread (or dedicated replay thread if needed)

#### 1.2 ReplayDataLoader Class
**Purpose**: Load and manage recorded data from SQLite databases with ping-pong buffering

**Location**: `Src/Core/Replay/ReplayDataLoader.h/cpp`

**Responsibilities**:
- Open replay database files for selected date (separate files for bars and market depth)
- Merge bars and market depth into single chronological sequence
- Implement ping-pong buffer strategy for memory efficiency
- Prefetch data asynchronously while current buffer is being consumed
- Handle missing data gracefully (some stocks may not have market depth data)

**Data Storage Understanding**:
- Recorder writes all incoming stream data sequentially to SQLite
- Each record has: `id` (autoincrement), `stockTicker`, `stockTickerSeq`, `epochMs`, `jsonRawData`
- Data is stored in arrival order, interleaved across all stocks
- To replay: read sequentially, use timestamp deltas to schedule emission timing
- Two separate database files: one for bars, one for market depth quotes

**Ping-Pong Buffer Strategy** (for handling 1000s of stocks, GBs of data):
- Use two buffers: "active" buffer being consumed, "loading" buffer being filled
- Each buffer holds ~1000 records (configurable)
- When active buffer is consumed, swap buffers (ping-pong)
- Start async database load into the now-empty buffer
- Ensures smooth playback without loading entire replay session into memory

**Key Components**:
```cpp
class ReplayDataLoader : public QObject
{
    Q_OBJECT
public:
    struct ReplayDataPoint {
        QString stockTicker;
        qint64 epochMs;
        QByteArray jsonRawData;
        enum Type { Bar, MarketDepthQuote } type;
        
        // For sorting merged data chronologically
        bool operator<(const ReplayDataPoint& other) const {
            return epochMs < other.epochMs;
        }
    };
    
    bool loadDatabase(QDate date);
    
    // Ping-pong buffer interface
    bool hasMoreData() const;
    ReplayDataPoint getNextDataPoint();  // Returns next point, advances buffer
    void prefetchNextBuffer();  // Async load next chunk
    
    QDateTime getFirstTimestamp() const;
    QDateTime getLastTimestamp() const;
    QStringList getAvailableStocks() const;
    
signals:
    void bufferReady();  // Emitted when prefetch completes
    
private:
    QSqlDatabase m_barsDb;
    QSqlDatabase m_marketDepthDb;
    
    // Ping-pong buffers
    QVector<ReplayDataPoint> m_bufferA;
    QVector<ReplayDataPoint> m_bufferB;
    QVector<ReplayDataPoint>* m_activeBuffer;
    QVector<ReplayDataPoint>* m_loadingBuffer;
    int m_activeBufferIndex = 0;
    
    // Database cursor tracking
    qint64 m_nextBarsId = 0;
    qint64 m_nextDepthId = 0;
    
    static constexpr int BUFFER_SIZE = 1000;  // Records per buffer
    
    void loadBufferChunk(QVector<ReplayDataPoint>* buffer);
    QVector<ReplayDataPoint> mergeChronologically(
        const QVector<ReplayDataPoint>& bars,
        const QVector<ReplayDataPoint>& depth);
};
```

**Thread**: Database operations run in QThreadPool via QtConcurrent for async prefetching

#### 1.3 SQL Queries for Replay
**Purpose**: Define queries for loading replay data with ping-pong buffering

**Location**: `Src/SQL/ReplayDataQueries.h`

**Queries Needed**:
```cpp
namespace ReplayDataQueries {
    // Select next N records starting from given ID, ordered by timestamp
    // Used for ping-pong buffer loading
    const QString SELECT_BARS_CHUNK = 
        "SELECT id, stockTicker, epochMs, jsonRawData FROM bars "
        "WHERE id >= ? "
        "ORDER BY id ASC "
        "LIMIT ?";
    
    const QString SELECT_MARKET_DEPTH_CHUNK = 
        "SELECT id, stockTicker, epochMs, jsonRawData FROM market_depth_quotes "
        "WHERE id >= ? "
        "ORDER BY id ASC "
        "LIMIT ?";
    
    // Get timestamp range for a specific date (for UI display)
    const QString SELECT_FIRST_TIMESTAMP = 
        "SELECT MIN(epochMs) FROM bars";
    
    const QString SELECT_LAST_TIMESTAMP = 
        "SELECT MAX(epochMs) FROM bars";
    
    // Get list of stocks in the replay database
    const QString SELECT_AVAILABLE_STOCKS = 
        "SELECT DISTINCT stockTicker FROM bars ORDER BY stockTicker";
    
    // Count total records (for progress calculation)
    const QString COUNT_BARS = "SELECT COUNT(*) FROM bars";
    const QString COUNT_MARKET_DEPTH = "SELECT COUNT(*) FROM market_depth_quotes";
}
```

**Database Indexing**:
For optimal replay performance, the recorded databases should have indexes:
```sql
CREATE INDEX IF NOT EXISTS idx_bars_id ON bars(id);
CREATE INDEX IF NOT EXISTS idx_bars_epoch ON bars(epochMs);
CREATE INDEX IF NOT EXISTS idx_market_depth_id ON market_depth_quotes(id);
CREATE INDEX IF NOT EXISTS idx_market_depth_epoch ON market_depth_quotes(epochMs);
```
These should be added to the Recorder's database creation logic in `LiveStreamDB.cpp`.

### Phase 2: TSClient Replay Mode

#### 2.1 Add Replay Mode to TSClient
**Purpose**: Make TSClient work in both live and replay modes

**Location**: `Src/Clients/TSClient/TSClient.h/cpp`

**Changes Needed**:
```cpp
class TSClient : public QObject
{
    Q_OBJECT
public:
    enum class Mode { Live, Replay };
    
    void setMode(Mode mode);
    Mode getMode() const { return m_mode; }
    
private:
    Mode m_mode = Mode::Live;
    
    // Track replay streams (one MockNetworkReply per stock)
    // We assume no duplicate stream openings for same symbol in replay mode
    QMap<QString, QPointer<MockNetworkReply>> m_replayBarReplies;
    QMap<QString, QPointer<MockNetworkReply>> m_replayDepthReplies;
    
    // Modify existing stream opening methods to check mode
    // In replay mode, create Stream with MockNetworkReply instead of real reply
    QPointer<StreamBars> openStreamBars(/* existing params */);
    QPointer<StreamMarketDepthQuote> openStreamMarketDepthQuote(/* existing params */);
};
```

**Implementation Strategy (Approach C - Least Invasive)**:
- Modify existing `openStreamBars()` and `openStreamMarketDepthQuote()` methods
- Check `if (m_mode == Mode::Replay)` → create MockNetworkReply, else create real request
- MainAlgo and other components call `openStreamBars()` as normal
- Transparently returns streams backed by MockNetworkReply in replay mode
- ReplayEngine feeds data to MockNetworkReply based on stock symbol

#### 2.2 MockNetworkReply Class
**Purpose**: Mock QNetworkReply to inject replay data into Stream

**Location**: `Src/Clients/TSClient/Stream/MockNetworkReply.h/cpp`

**Key Implementation Details**:
- Inherits `QNetworkReply` to work seamlessly with existing Stream class
- Stores incoming replay data in internal buffer
- Emits `readyRead()` signal when data is injected
- Stream's `onReplyReadyRead()` calls `readAll()` which internally calls our `readData()`
- Never emits `finished()` - stays open for entire replay session (like live streaming)

**Implementation**:
```cpp
class MockNetworkReply : public QNetworkReply
{
    Q_OBJECT
    
public:
    explicit MockNetworkReply(QObject* parent = nullptr);
    
    // Called by ReplayEngine to inject replay data
    void injectData(const QByteArray& jsonData);
    
    // QNetworkReply overrides
    void abort() override;
    qint64 bytesAvailable() const override;
    bool isSequential() const override { return true; }
    
protected:
    qint64 readData(char* data, qint64 maxSize) override;
    
private:
    QByteArray m_buffer;  // Pending data to be read by Stream
};
```

**How it works**:
1. ReplayEngine has timestamp-ordered data from database
2. When timer fires, ReplayEngine finds appropriate MockNetworkReply by symbol
3. Calls `mockReply->injectData(jsonRawData)` 
4. MockNetworkReply appends to buffer and emits `readyRead()`
5. Stream's `onReplyReadyRead()` slot is triggered
6. Stream calls `m_networkReply->readAll()`
7. QNetworkReply internally calls our overridden `readData()`
8. Stream gets the data and processes it normally through `processRawData()`

**Handling Unopened Streams (Option B)**:
- ReplayEngine only injects data if a stream exists for that stock
- When traversing replay data: `if (!hasStreamForStock(symbol)) { skip; }`
- Only processes data for stocks that MainAlgo/user has actively opened streams for
- Reduces memory overhead and focuses on relevant stocks only

### Phase 3: Stream Management During Replay

#### 3.1 Stream Lifecycle Control
**Purpose**: Properly pause/resume live streams during replay transitions

**Location**: `Src/Algo/MainAlgo.cpp`, `Src/Algo/PositionsReceiver/PositionsReceiver.cpp`, `Src/Algo/OrdersReceiver/OrdersReceiver.cpp`

**Stream Ownership**:
- **MainAlgo** owns `PositionsReceiver` and `OrdersReceiver`
- **PositionsReceiver** holds `QPointer<StreamPositions> m_stream`
- **OrdersReceiver** holds `QPointer<StreamOrders> m_stream`
- Both receivers have `stopStream(account)` methods

**Implementation**:
```cpp
// In MainAlgo
void MainAlgo::pauseLiveStreams()
{
    // Close positions stream via receiver
    if (m_positionsReceiver) {
        m_positionsReceiver->stopStream(m_account);
    }
    
    // Close orders stream via receiver
    if (m_ordersReceiver) {
        m_ordersReceiver->stopStream(m_account);
    }
}

void MainAlgo::resumeLiveStreams()
{
    // Recreate receivers (which will create new streams)
    // This triggers snapshot resend from TradeStation
    if (m_positionsReceiver) {
        // Receiver will call TSClient to create new stream
        m_positionsReceiver = new PositionsReceiver(m_account, this);
    }
    
    if (m_ordersReceiver) {
        m_ordersReceiver = new OrdersReceiver(m_account, this);
    }
}
```

**Note on Bar/Depth Streams**:
Bar and depth streams don't need explicit pause/resume because:
- In replay mode, `openStreamBars()` returns streams with MockNetworkReply
- In live mode, `openStreamBars()` returns streams with real QNetworkReply
- The Stream class itself doesn't know the difference
- MainAlgo can call `openStreamBars()` the same way in both modes

#### 3.2 ~~Stream State Tracking~~
**Purpose**: ~~Manage which stocks have active replay streams~~

**STATUS**: NOT NEEDED

With Approach C (modify `openStreamBars()` to check replay mode), we don't need separate replay stream management. The existing TSClient stream tracking works for both live and replay modes. The only difference is whether the Stream receives a real QNetworkReply or a MockNetworkReply.

ReplayEngine can query TSClient directly to check if a stream exists before injecting data:
```cpp
bool ReplayEngine::hasStreamForStock(const QString& symbol, 
                                     ReplayDataPoint::Type type) const
{
    // Query TSClient's existing stream maps
    if (type == ReplayDataPoint::Bar) {
        return m_tsClient->hasOpenBarStream(symbol);
    } else {
        return m_tsClient->hasOpenMarketDepthStream(symbol);
    }
}
```

TSClient may need helper methods:
```cpp
bool TSClient::hasOpenBarStream(const QString& symbol) const;
bool TSClient::hasOpenMarketDepthStream(const QString& symbol) const;
MockNetworkReply* TSClient::getBarReplyForSymbol(const QString& symbol);
MockNetworkReply* TSClient::getDepthReplyForSymbol(const QString& symbol);
```

### Phase 4: Simulated Order Execution

#### 4.1 SimulatedOrderExecutor Class
**Purpose**: Paper trading order execution and position tracking

**Location**: `Src/Algo/SimulatedOrderExecutor.h/cpp`

**Responsibilities**:
- Accept order requests during replay
- Simulate fills based on replayed market data
- Track simulated positions
- Generate position update events
- Calculate P&L

**Key Components**:
```cpp
class SimulatedOrderExecutor : public QObject
{
    Q_OBJECT
public:
    void placeOrder(const Order& order);
    void cancelOrder(const QString& orderId);
    void onBarReceived(const QString& symbol, const Bar& bar);
    QVector<Position> getCurrentPositions() const;
    
signals:
    void orderFilled(Order order);
    void positionUpdated(Position position);
    
private:
    struct SimulatedOrder {
        Order order;
        QDateTime placedAt;
        bool isFilled = false;
    };
    
    QMap<QString, SimulatedOrder> m_pendingOrders;
    QMap<QString, Position> m_currentPositions;
    
    void checkForFills(const QString& symbol, const Bar& bar);
    void fillOrder(const QString& orderId, double fillPrice);
};
```

#### 4.2 Order Routing Logic
**Purpose**: Route orders to real or simulated executor

**Location**: `Src/Clients/TSClient/OrderExecution/PlaceOrder/PlaceOrder.cpp`

**Changes**:
```cpp
QFuture<std::expected<Order, TSClient::Error>> 
TSClient::placeOrder(/* params */)
{
    if (MainApp::isInReplayMode) {
        // Route to simulated executor
        return simulatedOrderExecutor->placeOrder(/* params */);
    } else {
        // Existing real order execution
        return placeRealOrder(/* params */);
    }
}
```

#### 4.3 StrategySDK Replay Awareness
**Purpose**: Allow strategies to query replay state and adapt behavior

**Location**: `Src/Strategy/StrategySDK.h/cpp`

**New API Methods**:
```cpp
class StrategySDK {
public:
    // Existing methods...
    
    // Replay awareness API
    bool isInReplayMode() const {
        return MainApp::isInReplayMode;
    }
    
    ReplayEngine::PlaybackSpeed getCurrentReplaySpeed() const {
        return MainApp::currentReplaySpeed;
    }
    
    // Convenience method
    bool isReplayFastMode() const {
        return isInReplayMode() && 
               getCurrentReplaySpeed() == ReplayEngine::PlaybackSpeed::AsFastAsPossible;
    }
};
```

**Usage in Strategies**:
```cpp
void MyStrategy::onBar(const Bar& bar) {
    // Adapt behavior based on replay mode
    if (sdk->isInReplayMode()) {
        // Skip time-dependent delays
        // Disable external API calls
        // Reduce logging
    }
    
    if (sdk->isReplayFastMode()) {
        // Skip expensive calculations
        // Disable rate limiting
    }
    
    // Normal strategy logic...
}
```

**MainApp Changes**:
```cpp
class MainApp {
public:
    static bool isInReplayMode;
    static QDateTime currentAppReplayTime;
    static ReplayEngine::PlaybackSpeed currentReplaySpeed;  // NEW
    
    // Updated signature to include speed
    void enterReplayMode(QDate date, QTime startTime, 
                        ReplayEngine::PlaybackSpeed speed);
};
```

### Phase 5: UI Integration

#### 5.1 Connect ChartToolbar Signals
**Purpose**: Wire up existing UI controls to replay engine

**Location**: `Src/FrontEnd/GUI/StockPriceChart/StockPriceChart.cpp`

**Connections**:
```cpp
// In StockPriceChart constructor or init
connect(m_toolbar, &ChartToolbar::replayDayChanged,
        this, &StockPriceChart::onReplayDayChanged);

connect(m_toolbar, &ChartToolbar::replayStartTimeChanged,
        this, &StockPriceChart::onReplayStartTimeChanged);

connect(m_toolbar, &ChartToolbar::replayPlayPauseToggled,
        this, &StockPriceChart::onReplayPlayPauseToggled);
```

**Handler Implementation**:
```cpp
void StockPriceChart::onReplayPlayPauseToggled(bool playing)
{
    if (playing) {
        // Get selected date and time from toolbar
        QDate date = m_toolbar->getSelectedReplayDay();
        QTime time = m_toolbar->getReplayStartTime();
        
        // Start replay
        ReplayEngine::getInstance()->startReplay(date, time);
    } else {
        // Pause or stop replay
        ReplayEngine::getInstance()->pauseReplay();
    }
}
```

#### 5.2 Speed Control Widget
**Purpose**: Add UI for controlling playback speed

**Location**: `Src/FrontEnd/GUI/StockPriceChart/ChartToolbar.h/cpp`

**Add to toolbar**:
- Speed combo box: "0.5x", "1.0x", "2.0x", "5.0x", "10.0x", "Max"
- Place near play/pause button

#### 5.3 Replay Mode Indicator
**Purpose**: Visual indicator that replay is active

**Location**: Multiple UI components

**Implementation**:
- Status bar indicator: "REPLAY MODE - 2024-01-15 09:35:42"
- Different chart background color or border
- Disable certain actions (e.g., real order entry)

#### 5.4 Progress Indicator
**Purpose**: Show replay progress through the day

**Location**: `Src/FrontEnd/GUI/StockPriceChart/ChartToolbar.h/cpp`

**Add to toolbar**:
- Progress bar showing time elapsed vs total time
- Current time label (updates as replay advances)

### Phase 6: State Coordination

#### 6.1 MainApp Replay Coordination
**Purpose**: High-level coordination of replay mode transitions in Main/GUI thread

**Location**: `Src/Core/MainApp.h/cpp`

**New Methods**:
```cpp
class MainApp
{
public:
    void enterReplayMode(QDate date, QTime startTime);
    void exitReplayMode();
    
    // Already exist
    static QDateTime currentAppReplayTime;
    static bool isInReplayMode;
    static QDateTime getCurrentAppTime();
    
private:
    TSClient* tradeStationClient;  // Already exists
    MainAlgo* mainAlgo;  // Already exists
    FrontEnd* appFrontend;  // Already exists
    
    // Note: No ReplayEngine - logic moved to MainAlgo
};
```

**Entry Sequence** (runs in Main/GUI thread):
```cpp
void MainApp::enterReplayMode(QDate date, QTime startTime, 
                             ReplayEngine::PlaybackSpeed speed)
{
    qInfo() << "Entering replay mode for" << date << "starting at" << startTime;
    
    // 1. Set global flags
    isInReplayMode = true;
    currentAppReplayTime = QDateTime(date, startTime, TradingHours::MARKET_TIMEZONE);
    currentReplaySpeed = speed;
    
    // 2. Switch TSClient to replay mode (TSClient thread)
    QMetaObject::invokeMethod(tradeStationClient, [this]() {
        tradeStationClient->setMode(TSClient::Mode::Replay);
    }, Qt::BlockingQueuedConnection);
    
    // 3. Tell MainAlgo to start replay and close streams (MainAlgo thread)
    QMetaObject::invokeMethod(mainAlgo, [this, date, startTime, speed]() {
        // MainAlgo closes positions/orders streams via receivers
        mainAlgo->pauseLiveStreams();
        
        // Then enters replay mode
        mainAlgo->enterReplayMode(date, startTime, speed);
    }, Qt::QueuedConnection);
    
    // 4. Update UI
    appFrontend->onReplayModeEntered();
}
```

**Exit Sequence** (runs in Main/GUI thread):
```cpp
void MainApp::exitReplayMode()
{
    qInfo() << "Exiting replay mode";
    
    // 1. Tell MainAlgo to stop replay first (blocking to ensure clean stop)
    QMetaObject::invokeMethod(mainAlgo, [this]() {
        mainAlgo->exitReplayMode();
        
        // MainAlgo reopens positions/orders streams via receivers
        mainAlgo->resumeLiveStreams();
    }, Qt::BlockingQueuedConnection);
    
    // 2. Reset global flags
    isInReplayMode = false;
    
    // 3. Resume TSClient mode (TSClient thread)
    QMetaObject::invokeMethod(tradeStationClient, [this]() {
        tradeStationClient->setMode(TSClient::Mode::Live);
    }, Qt::BlockingQueuedConnection);
    
    // 4. Update UI
    appFrontend->onReplayModeExited();
}
```

**Key Points**:
- MainApp orchestrates but doesn't do heavy lifting
- Cross-thread calls to MainAlgo and TSClient via QMetaObject::invokeMethod
- Blocking calls for critical transitions (exit replay, mode switching)
- Non-blocking for replay start (MainAlgo handles async setup)

#### 6.2 BarCache Replay Handling
**Purpose**: Ensure BarCache works correctly in replay mode

**Location**: `Src/Core/Cache/BarCache/BarCache.cpp`

**Key Principles**:
- BarCache behavior is **mostly unchanged** in replay mode
- Uses `MainApp::getCurrentAppTime()` which automatically returns replay time
- Handles historical bar requests the same way as live mode

**Specific Behaviors**:

1. **Historical Requests (before replay startTime)**:
   - Strategy calls `sdk->getBars(date, 6:01 AM, 10:15 AM)` during replay
   - BarCache checks its regular database cache
   - If not cached, queries BarCache database
   - **Does NOT use replay database** for historical requests
   - May require API call if truly cold start (rare in replay scenarios)

2. **Live Bars (from replay stream)**:
   - Bars arrive via replayed stream (MockNetworkReply)
   - BarCache receives them via normal signal/slot
   - Stores in memory cache (same as live mode)
   - Should NOT persist to BarCache database (temporary replay data)

3. **API Calls**:
   - During replay, BarCache might still need to call API for historical data
   - This is acceptable - replay only streams "live" bars from startTime
   - Historical context comes from regular cache/API, not replay data

**Changes Needed**:
```cpp
// In BarCache::onReceivedBar()
void BarCache::onReceivedBar(const Bar& bar) {
    // Add to memory cache
    m_bars.append(bar);
    
    // Only persist to database if NOT in replay mode
    if (!MainApp::isInReplayMode) {
        m_databaseThread->storeBar(m_symbol, bar);
    }
}
```

**No Changes Needed**:
- `getBars()` method works as-is (queries database/API)
- Time-based logic works via `getCurrentAppTime()`
- Cache expiration logic works normally

### Phase 7: Testing & Polish

#### 7.1 Unit Tests
**Files to create**:
- `Tests/test_replay_engine.cpp`
- `Tests/test_replay_data_loader.cpp`
- `Tests/test_simulated_executor.cpp`

**Test coverage**:
- ReplayEngine playback timing accuracy
- ReplayDataLoader data ordering and synchronization
- SimulatedOrderExecutor fill logic
- TSClient mode switching
- State transitions (live → replay → live)

#### 7.2 Integration Testing
**Scenarios**:
- Start replay, watch bars flow through system
- Run strategies during replay, verify signals
- Place simulated orders, verify fills and P&L
- Pause/resume replay
- Change speed during replay
- Exit replay, verify live streams resume correctly

#### 7.3 Edge Cases
- Empty database (no recorded data for selected day)
- Incomplete data (missing bars or quotes)
- Replay during market hours (prevent confusion)
- Multiple replay sessions without restarting app
- Very fast replay speed ("as fast as possible")

#### 7.4 Performance Optimization
- Database query optimization (indexes on epochMs, stockTicker)
- Batch loading of replay data
- Efficient JSON parsing
- Memory management for large replay sessions

## Implementation Checklist

### Phase 1: Core Infrastructure
- [x] Create `ReplayEngine` class (runs in MainAlgoThread via parent)
- [x] Implement state management (Stopped/Playing/Paused)
- [x] Implement speed control enums and scaling logic
- [x] Implement timer-based playback with delta calculation
- [x] Implement discrete time updates to `MainApp::currentAppReplayTime`
- [x] Add ReplayEngine member to MainAlgo with delegation methods
- [x] Create `ReplayDataLoader` class with database access
- [x] Implement ping-pong buffer strategy (1000 records per buffer)
- [x] Implement async prefetching with QtConcurrent
- [x] Implement chronological merging of bars and market depth data
- [x] Add `ReplayDataQueries.h` with chunk-based SQL queries
- [x] Add database indexes to LiveStreamDBQueries.h for optimal replay performance
- [x] Implement stopStream() for PositionsReceiver and OrdersReceiver
- [ ] Add unit tests for ReplayEngine timing accuracy
- [ ] Add unit tests for ReplayDataLoader buffer management

### Phase 2: TSClient Integration
- [x] Add `Mode` enum to TSClient (Live/Replay)
- [x] Implement `setMode()` and `getMode()` methods
- [x] Create `MockNetworkReply` class inheriting QNetworkReply
- [x] Implement `injectData()`, `readData()`, `bytesAvailable()` in MockNetworkReply
- [x] Modify `openStreamBars()` to check mode and return appropriate reply type
- [x] Modify `openStreamMarketDepthQuote()` similarly
- [x] Add helper methods: `hasOpenBarStream()`, `hasOpenMarketDepthStream()`
- [x] Add helper methods: `getBarReplyForSymbol()`, `getDepthReplyForSymbol()`
- [x] Implement tracking maps for MockNetworkReply pointers
- [ ] Test data injection through existing Stream callbacks
- [ ] Verify Stream class works unchanged with MockNetworkReply

### Phase 3: Stream Management (COMPLETED in Phase 1.3)
- [x] Add `pauseLiveStreams()` to MainAlgo (calls stopStream on receivers)
- [x] Add `resumeLiveStreams()` to MainAlgo (recreates receivers with new streams)
- [x] Update PositionsReceiver to handle stream recreation
- [x] Update OrdersReceiver to handle stream recreation
- [ ] Test stream pause/resume cycle
- [ ] Verify snapshot resend on stream resume
- [ ] Verify bar/depth streams work transparently via mode switching

### Phase 4: Simulated Execution
- [ ] Create `SimulatedOrderExecutor` class
- [ ] Implement order fill logic based on bar data
- [ ] Implement position tracking and P&L calculation
- [ ] Route orders through simulated executor when `MainApp::isInReplayMode`
- [ ] Add `MainApp::currentReplaySpeed` static member
- [ ] Add `isInReplayMode()` to StrategySDK
- [ ] Add `getCurrentReplaySpeed()` to StrategySDK
- [ ] Add `isReplayFastMode()` convenience method to StrategySDK
- [ ] Update strategy documentation with replay awareness API
- [ ] Test order fills and position updates
- [ ] Integrate with UI position/order displays
- [ ] Handle edge cases (partial fills, cancellations)

### Phase 5: UI Integration
- [x] Connect ChartToolbar play/pause button to ReplayEngine
- [x] Connect day selector to replay date selection (already existed)
- [x] Connect time selector to replay start time (already existed)
- [x] Add speed control combo box to toolbar (0.5x-10x, "Max")
- [x] Add replay mode indicator in top-right corner of main window
- [ ] Add progress bar/time display to toolbar
- [ ] Implement real-time progress updates during replay
- [ ] Test UI responsiveness during fast replay ("as fast as possible")

#### Visual Distinction for Replay Mode (Chart)
- [x] Change chart background color slightly when in replay mode
- [x] Add "REPLAY" watermark text at center of chart (pale, transparent, behind candlesticks)
- [x] Ensure watermark is drawn at lower Z-order than bars

#### Enhanced Status Labels (Trading Mode & Data Source)
The status display needs two separate state dimensions:
1. **Trading Mode**: LIVE vs SIM (TradeStation API connection mode)
   - LIVE = real money trading via `api.tradestation.com` (orange - caution!)
   - SIM = simulated trading via `sim-api.tradestation.com` (blue/cyan - safe)
2. **Data Source Mode**: LIVE vs REPLAY (existing implementation)
   - LIVE = real-time market data from API (green)
   - REPLAY = recorded market data playback (red)

**Design Decisions:**
- Two separate labels side by side: `🔵 SIM` `🟢 LIVE` or `🔵 SIM` `🔴 REPLAY`
- Trading mode is a toggle switch (clickable), persists to AppState.ini
- Toggling trading mode triggers app restart to reconnect with correct API endpoint
- Color scheme:
  - SIM trading: Blue/Cyan (#1E90FF) - safe indicator
  - LIVE trading: Orange (#FF8C00) - caution, real money
  - LIVE data: Green (#228B22) - current
  - REPLAY data: Dark Red (#8B0000) - current

Implementation tasks:
- [x] Add TradingMode enum to MainApp (Live, Sim)
- [x] Add static TradingMode member to MainApp
- [x] Add getTradingMode()/setTradingMode() to MainApp with persistence
- [x] Add API host URLs to CONSTANTS.h (TSClientHosts namespace)
- [x] Update TSClient to get host URL from MainApp::getTradingMode()
- [x] Split m_replayIndicator into m_tradingModeLabel and m_dataSourceLabel
- [x] Make m_tradingModeLabel clickable (toggle switch behavior)
- [x] Show confirmation dialog before toggle (warns about restart)
- [x] Save trading mode to AppState.ini on change
- [x] Load trading mode from AppState.ini on startup
- [x] Restart app after mode change (QCoreApplication::exit(42))
- [x] Update onReplayModeEntered/Exited to only update data source label

#### Clickable Data Source Label (LIVE/REPLAY Toggle)
Make the data source label clickable to toggle between LIVE and REPLAY modes:

**Design Decisions:**
- **Replay is only available in SIM trading mode** - When trading mode is LIVE (real money), the data source label AND all replay widgets are hidden entirely
- Click on data source label toggles between LIVE data and REPLAY modes
- In LIVE data mode: Hide replay toolbar widgets (day selector, time selector, play/pause button, speed selector)
- In REPLAY mode: Show replay toolbar widgets
- Clicking in LIVE data mode: Start replay with last used date/time (or earliest available if none)
- Clicking in REPLAY mode: Exit replay (calls MainApp::exitReplayMode())

**Visibility Rules:**
| Trading Mode | Data Source | Data Source Label | Replay Widgets |
|--------------|-------------|-------------------|----------------|
| LIVE (real $)| LIVE data   | Hidden            | Hidden         |
| SIM          | LIVE data   | Visible (🟢 LIVE) | Hidden         |
| SIM          | REPLAY      | Visible (🔴 REPLAY)| Visible       |

Implementation tasks:
- [x] Make m_dataSourceLabel clickable (add to eventFilter)
- [x] Add setReplayWidgetsVisible(bool) to ChartToolbar
- [x] On startup: if trading mode is LIVE, hide data source label entirely
- [x] On startup: if trading mode is SIM, show data source label, hide replay widgets
- [x] Store last used replay date/time in AppState.ini (uses toolbar values)
- [x] On click in LIVE data mode: get last date/time from toolbar, call MainApp::enterReplayMode()
- [x] On click in REPLAY mode: call MainApp::exitReplayMode()
- [x] When replay starts (onReplayModeEntered): show widgets, update label
- [x] When replay exits (onReplayModeExited): hide widgets, update label
- [x] Add toolbar() getter to StockPriceChart

- [x] Add TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION (4:01 AM) and TIME_LAST_CANDLE_EARLY_PRE_MARKET_SESSION (6:00 AM) to CONSTANTS.h
- [x] Update MINUTE_BARS_PER_DAY from 840 to 960 (16 hours × 60 minutes)
- [x] Update timeToIndex() and indexToTime() to use 4:01 AM start
- [x] Add m_earlyPreMarketRects to StockPriceChart for background rectangles
- [x] Draw early pre-market background (4:01am-6:00am) with paler orange (alpha 90 vs 180)

### Phase 6: State Coordination
- [x] Implement `MainApp::enterReplayMode()` with cross-thread coordination
- [x] Implement `MainApp::exitReplayMode()` with proper blocking calls
- [x] Add `FrontEnd::onReplayModeEntered()` and `onReplayModeExited()` virtual methods
- [x] Fix `MainApp::getCurrentAppTime()` to work in replay mode
- [x] Connect `MainAlgo::replayTimeUpdated` to `FrontEnd::onReplayTimeUpdated`
- [ ] Implement GUI replay mode indicators (status bar, background color)
- [ ] Test cross-thread communication (Main → MainAlgo, Main → TSClient)
- [ ] Clear positions/orders UI on replay start
- [ ] Restore live streams on replay stop
- [ ] Update BarCache to handle replay mode (disable API calls, temp cache)
- [ ] Test full replay lifecycle (enter → play → stop → exit → resume live)
- [ ] Handle edge case: switching stocks during replay
- [ ] Test pause/resume functionality

### Phase 7: Testing & Polish
- [ ] Write unit tests for MainAlgo replay logic (timing, speed scaling)
- [ ] Write unit tests for ReplayDataLoader (ping-pong, merging)
- [ ] Write unit tests for MockNetworkReply
- [ ] Write unit tests for SimulatedOrderExecutor
- [ ] Perform integration testing with recorded data
- [ ] Test with small dataset (10 stocks, 1 hour)
- [ ] Test with large dataset (1000 stocks, full day)
- [ ] Test edge cases:
  - [ ] Empty database (no recorded data for selected day)
  - [ ] Incomplete data (missing bars or quotes)
  - [ ] Stock with bars but no market depth
  - [ ] Replay during market hours (prevent confusion)
  - [ ] Multiple replay sessions without restarting app
  - [ ] Speed changes mid-replay
  - [ ] Pause/resume multiple times
  - [ ] Cross-thread race conditions (MainApp → MainAlgo transitions)
- [ ] Performance profiling and optimization
- [ ] Test "as fast as possible" mode with 1000+ stocks
- [ ] Verify memory usage with ping-pong buffers
- [ ] Verify MainAlgoThread doesn't block during replay
- [ ] Documentation updates (Doc/ARCHITECTURE.md, Doc/README.md)
- [ ] User guide for replay functionality

## Future Enhancements (Not in Initial Implementation)

These improvements should be tracked for future iterations:

### Timeline Controls
- Step-by-step mode (advance one bar at a time)
- Seek backward/forward
- Jump to specific timestamp
- Bookmark interesting moments

### Advanced Features
- Record replay sessions for later review
- Compare multiple replay runs side-by-side
- Replay speed based on volatility (slow down during high-volatility periods)
- Multi-day replay (span across multiple database files)
- Replay filters (skip certain stocks, filter by criteria)

### Strategy Development Tools
- Strategy parameter optimization during replay
- A/B testing of strategy variants
- Performance metrics dashboard
- Heat maps of entry/exit points

### UI Enhancements
- Timeline scrubber with minimap
- Replay annotations (mark interesting events)
- Side-by-side live vs replay comparison
- Replay history log

## Dependencies & Prerequisites

### Required Components
- Qt 6.x (already available)
- SQLite3 (already available)
- Existing Recorder databases with recorded data

### New Dependencies
- None (using existing frameworks)

### File Structure
```
Src/Core/Replay/
├── ReplayEngine.h
├── ReplayEngine.cpp
├── ReplayDataLoader.h
└── ReplayDataLoader.cpp

Src/Algo/
├── MainAlgo.h              # Add ReplayEngine* member and delegation methods
├── MainAlgo.cpp
├── SimulatedOrderExecutor.h
└── SimulatedOrderExecutor.cpp

Src/Core/
├── MainApp.h               # Replay coordination methods
└── MainApp.cpp

Src/Clients/TSClient/Stream/
├── MockNetworkReply.h
└── MockNetworkReply.cpp

Src/SQL/
└── ReplayDataQueries.h

Tests/
├── test_replay_engine.cpp
├── test_replay_data_loader.cpp
├── test_mock_network_reply.cpp
└── test_simulated_executor.cpp
```

## Risk Assessment

### High Risk Areas
1. **Thread Safety**: Multiple threads accessing replay state
2. **Timing Accuracy**: Maintaining accurate replay speed
3. **Memory Management**: Large data sets for long replay sessions
4. **State Consistency**: Ensuring clean transitions between modes

### Mitigation Strategies
1. Use Qt's thread-safe signal/slot mechanism
2. Use high-precision timers (QElapsedTimer)
3. Implement data buffering and streaming from database
4. Comprehensive state machine with validation

## Success Criteria

The replay feature will be considered complete when:

1. ✅ Users can select a recorded day and start replay
2. ✅ Bars and market depth quotes flow through the system as in live mode
3. ✅ Charts update correctly with replayed data
4. ✅ Strategies execute and generate signals
5. ✅ Orders are simulated and positions tracked
6. ✅ Playback speed can be adjusted (including "as fast as possible")
7. ✅ Replay can be paused and resumed
8. ✅ Exiting replay cleanly returns to live mode
9. ✅ All unit and integration tests pass
10. ✅ Performance is acceptable even at maximum speed

## Timeline Estimate

**Note**: No specific time estimates provided as per instructions. Phases should be completed sequentially, with each phase building on the previous one.

## Open Questions & Decisions

### ✅ Resolved

1. **Cross-thread data injection** (Q1): Use signal/slot with `std::shared_ptr<const QByteArray>` to avoid copying and ensure thread safety. ReplayEngine emits signals, TSClient has slots that call `MockNetworkReply::injectData()`.

### ⏳ Pending

2. **Stream cleanup on replay exit**: When exiting replay mode and calling `setMode(Live)`, we clear the tracking maps. But MockNetworkReply objects are parented to TSClient. Should we:
   - Explicitly delete them before clearing maps?
   - Rely on Stream deletion to clean them up (Stream owns its QNetworkReply)?
   - Let TSClient destructor handle them?
   
   **Likely answer**: Stream class calls `delete m_reply` in destructor, so closing/deleting the Stream should clean up MockNetworkReply. Need to verify.

3. **SimulatedOrderExecutor location and design**: Where should it live and how should it intercept orders?
   - Separate class composed into MainAlgo?
   - Modify existing TSClient order methods to check replay mode?
   - How to populate PositionsReceiver/OrdersReceiver with simulated data?
   
   **To be decided during Phase 4 planning.**

4. **First data point timing**: When replay starts, do we immediately emit the first data point (index 0), or schedule it for later?
   - Current implementation: schedules with 0ms delay (immediate via event loop)
   - This seems correct - emit first point immediately, then use deltas for subsequent points.

## Notes

- This is a complex feature touching many parts of the codebase
- Careful planning and incremental implementation is crucial
- Testing at each phase is essential to catch issues early
- The existing infrastructure (`getCurrentAppTime()`, recorded databases) provides a solid foundation
- The hybrid mode approach (pause/resume) is simpler than running live and replay simultaneously

---

# Current Task: App Restart via execv()

## Problem
When user clicks the trading mode toggle button and confirms the dialog, the trading mode is saved to config but the app doesn't restart. The app needs to reload to use the new trading mode (since TSClient selects its API host at startup).

## Proposed Solution
Use `execv()` system call to replace the current process with a fresh instance of the same executable. This:
- Requires no external launcher or script
- Keeps the same PID range
- Avoids orphaned processes
- Is clean and immediate

## Workplan

- [x] Add `#include <unistd.h>` to MainApp.cpp for execv()
- [x] Create MainApp::restartApplication() method that:
  - Gets executable path via QCoreApplication::applicationFilePath()
  - Gets command line arguments via QCoreApplication::arguments()
  - Converts to C-style arrays (char* argv[])
  - Calls execv() to replace current process
- [x] Update eventFilter in GUIFrontend.cpp:
  - After confirmation dialog is accepted (already saves trading mode)
  - Call MainApp::restartApplication() instead of just closing dialog
- [x] Build and verify
- [x] Commit

## Implementation Notes
- No app state saving needed - clean restart is acceptable
- No user feedback needed beyond the confirmation dialog
- Trading mode already persisted to AppState.ini before restart call
- On Linux, execv() will restart the app immediately

---

# Current Task: Trading Session Tracking and Display

## Problem
The app needs to track which trading session is currently active (early pre-market, pre-market, regular hours, after-hours, or closed) and display this in the UI. The session should update in real-time and work correctly in both live and replay modes.

## Proposed Solution
1. Add `TradingSession` enum to MainApp with five states
2. Add `MainApp::getCurrentSession()` function that determines session from current app time
3. Add session display label at the top of the GUI showing current session
4. Update label whenever app time changes (live or replay)

## Session Definitions
Based on CONSTANTS.h TradingHours namespace:
- **Early Pre-Market**: 4:01 AM - 6:00 AM ET
- **Pre-Market**: 6:01 AM - 9:30 AM ET  
- **Regular Hours**: 9:31 AM - 4:00 PM ET
- **After Hours**: 4:01 PM - 8:00 PM ET
- **Closed**: 8:01 PM - 4:00 AM ET

## Workplan

- [ ] Add TradingSession enum to MainApp.h with five states
- [ ] Add MainApp::getCurrentSession() static function that:
  - Uses MainApp::getCurrentAppTime() to get current time
  - Compares time against TradingHours constants to determine session
  - Returns TradingSession enum value
- [ ] Add session label to GUIFrontend.h
- [ ] Create and configure session label in GUIFrontend constructor
  - Position at top-left (next to trading mode label)
  - Show session name with appropriate color
  - Use monospace font for consistency
- [ ] Add MainApp signal for session changes (or periodic update mechanism)
- [ ] Update label when session changes (live mode) or updates (replay timer tick)
- [ ] Build and verify
- [ ] Commit

## Implementation Notes
- Session display should mirror the existing trading mode/data source label styling
- Consider if label needs tooltip explaining each session
- In replay mode, label updates with each new bar/timestamp from ReplayEngine
- Session is determined purely from time, works in both live and replay modes
