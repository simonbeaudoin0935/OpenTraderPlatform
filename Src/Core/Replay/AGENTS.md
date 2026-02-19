# Recorder & Replay System - Agent Instructions

## Overview

The Recorder and Replay system enables capturing live market data streams and replaying them later for strategy backtesting and debugging. This document describes the architecture for AI agents working on this codebase.

## Architecture Summary

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           RECORDING PHASE                                    │
│  (Separate Recorder executable)                                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   TSClient (Live)                                                            │
│       │                                                                      │
│       ├──► StreamBars ──────────────► LiveStreamDB (Bars)                   │
│       │                                    │                                 │
│       │                                    ▼                                 │
│       │                         RecordedLiveData/Bars/{date}.db             │
│       │                                                                      │
│       ├──► StreamMarketDepthQuote ──► LiveStreamDB (Depth)                  │
│       │                                    │                                 │
│       │                                    ▼                                 │
│       │                    RecordedLiveData/MarketDepthQuotes/{date}.db      │
│       │                                                                      │
│       └──► StreamQuote (all symbols) ─► LiveStreamDB (Quotes)               │
│                                            │                                 │
│                                            ▼                                 │
│                           RecordedLiveData/Quotes/{date}.db                 │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                            REPLAY PHASE                                      │
│  (Main L2Trader application)                                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   MainApp                                                                    │
│       │                                                                      │
│       │  enterReplayMode()                                                   │
│       │  ─────────────────►                                                  │
│       │                                                                      │
│       ▼                                                                      │
│   MainAlgo (MainAlgoThread)                                                  │
│       │                                                                      │
│       │  Creates & manages                                                   │
│       ▼                                                                      │
│   ReplayEngine ◄────────────────────────────────────────────────────────┐   │
│       │                                                                  │   │
│       │  startReplayPaused() / startReplay()                            │   │
│       │                                                                  │   │
│       ├── m_barTimer ──► ReplayDataLoader (Bar)                         │   │
│       │       │  Loads from Bars SQLite DB                              │   │
│       │       │  Ping-pong buffer strategy                              │   │
│       │       ▼                                                          │   │
│       │   ReplayDataPoint → injectBarData signal                        │   │
│       │                                                                  │   │
│       ├── m_depthTimer ──► ReplayDataLoader (MarketDepthQuote)          │   │
│       │       │  Loads from Depth SQLite DB                              │   │
│       │       │  Ping-pong buffer strategy                              │   │
│       │       ▼                                                          │   │
│       │   ReplayDataPoint → injectDepthData signal                      │   │
│       │                                                                  │   │
│       └── m_quoteTimer ──► ReplayDataLoader (Quote)                     │   │
│               │  Loads from Quotes SQLite DB                             │   │
│               │  Ping-pong buffer strategy                               │   │
│               │  Filters QuoteStream by open streams;                   │   │
│               │  always emits Heartbeat/Error                            │   │
│               ▼                                                          │   │
│           ReplayDataPoint (raw delta bytes) → injectQuoteData signal    │   │
│                                                                          │   │
│   TSClient::onInjectQuoteData()                                          │   │
│       │  Merges delta into m_replayQuoteState[symbol]                   │   │
│       │  Calls OrderEmulator::updateQuote(merged Quote)                 │   │
│       │  Injects raw bytes into MockNetworkReply for displayed symbol   │   │
│       ▼                                                                  │   │
│   StreamQuote (via MockNetworkReply)                                     │   │
│       │  Merges delta into m_symbolState[symbol]                        │   │
│       │  Emits newQuoteReceived(merged Quote)                           │   │
│       ▼                                                                  │   │
│   MainAlgo → GUIFrontend → MarketDepthTable::updateLevel1Data()         │   │
│                                                                          │   │
│   TSClient (Replay Mode)                                                 │   │
│       │                                                                  │   │
│       │  Routes to MockNetworkReply                                     │   │
│       ▼                                                                  │   │
│   MockNetworkReply ──► Stream ──► BarReceiver/MarketDepthQuoteReceiver  │   │
│                                         │                                │   │
│                                         │  Heartbeat timers paused       │   │
│                                         │  when replay paused            │   │
│                                         └────────────────────────────────┘   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Key Components

### Recording Side (Src/Recorder/)

| File | Purpose |
|------|---------|
| `main.cpp` | Recorder executable entry point. Sets up TSClient, LiveStreamDB instances, signal handlers |
| `LiveStreamDB.h/cpp` | Writes raw JSON from streams to SQLite databases. Handles stream recovery on timeout |
| `StatusReporter.h/cpp` | Periodic console output showing recording statistics |
| `RecorderUtils.h/cpp` | Utility functions for the recorder |

### Replay Side (Src/Core/Replay/)

| File | Purpose |
|------|---------|
| `ReplayEngine.h/cpp` | Orchestrates replay playback with three independent streams (bars, depth, quotes). Each stream has its own single-shot QTimer and ReplayDataLoader. Uses wall-clock anchored timing for accurate speed control |
| `ReplayDataLoader.h/cpp` | Loads one data type (Bar, MarketDepthQuote, or Quote) from its SQLite DB into memory buffers. Ping-pong buffering with prefetch at 80% |
| `OrderEmulator/` | Subfolder containing order/position simulation. See `OrderEmulator/AGENTS.md` for details |

### Integration Points

| File | Role in Replay |
|------|----------------|
| `MainApp.cpp` | Entry point for mode switching: `enterReplayMode()`, `exitReplayMode()`, `setReplaySpeed()` |
| `MainAlgo.cpp` | Creates ReplayEngine, manages stock instruments, pauses/resumes heartbeat timers |
| `TSClient.cpp` | Switches between Live/Replay modes. In Replay, creates MockNetworkReply objects |
| `MockNetworkReply.h/cpp` | Fake QNetworkReply that receives injected JSON data from ReplayEngine |
| `Stream.h/cpp` | Has `pauseHeartbeat()`/`resumeHeartbeat()` for replay pause support |
| `StreamReceiver.h` | Base class for receivers with common heartbeat management |

## Database Schema

### Bars Database ({YYYY-MM-DD}.db in Bars folder)

```sql
CREATE TABLE bars (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol TEXT NOT NULL,
    epoch_ms INTEGER NOT NULL,
    json_data TEXT NOT NULL
);
CREATE INDEX idx_bars_symbol_epoch ON bars(symbol, epoch_ms);
```

### Market Depth Database ({YYYY-MM-DD}.db in MarketDepthQuotes folder)

```sql
CREATE TABLE depth_quotes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol TEXT NOT NULL,
    epoch_ms INTEGER NOT NULL,
    json_data TEXT NOT NULL
);
CREATE INDEX idx_depth_symbol_epoch ON depth_quotes(symbol, epoch_ms);
```

### Quotes Database ({YYYY-MM-DD}.db in Quotes folder)

```sql
CREATE TABLE quotes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    stockTicker TEXT NOT NULL,       -- Empty string for Heartbeat/Error
    epochMs INTEGER NOT NULL,
    objectType TEXT NOT NULL,        -- "QuoteStream", "Heartbeat", or "Error"
    jsonRawData TEXT NOT NULL
);
CREATE INDEX idx_quotes_ticker_epoch ON quotes(stockTicker, epochMs);
CREATE INDEX idx_quotes_object_type ON quotes(objectType);
```

Key differences from bars/depth schema:
- Column names use camelCase (`stockTicker`, `epochMs`, `jsonRawData`) instead of snake_case
- Has an `objectType` column for filtering QuoteStream vs Heartbeat vs Error
- All three object types are stored (heartbeats needed for accurate replay timing)
- `stockTicker` is empty string for Heartbeat/Error rows (they are stream-global)

> **⚠️ Delta Protocol**: Raw JSON bytes are stored **as-is** — including partial delta
> messages that contain only changed fields. Merging happens at parse time in
> `StreamQuote::m_symbolState` (GUI path) and `TSClient::m_replayQuoteState`
> (OrderEmulator path), **not** at record or playback time. See
> `StreamQuote/AGENTS.md` for the full protocol description.

## Replay Flow

### 1. Entering Replay Mode

```
User clicks "Replay" button
    │
    ▼
GUIFrontend::eventFilter() detects click on m_dataSourceLabel
    │
    ▼
MainApp::enterReplayMode(date, startTime, speed)
    │
    ├── Set m_dataSourceMode = Replay
    ├── TSClient::setMode(Replay) [BlockingQueuedConnection]
    ├── MainAlgo::deleteAllStockInstruments()
    ├── MainAlgo::createAndSetDisplayedStockInstrument()
    └── MainAlgo::enterReplayModePaused()
            │
            ├── Create ReplayEngine
            ├── Connect signals (injectBarData, injectDepthData)
            ├── startReplayPaused() → emits first bar, then pauses
            └── pauseHeartbeat() on all stream receivers
```

### 2. Playing Replay

```
User clicks "Play" button
    │
    ▼
ChartToolbar::replayPlayPauseToggled(true)
    │
    ▼
MainApp::resumeReplayPlayback() or startReplayPlayback()
    │
    ▼
MainAlgo::resumeReplay()
    │
    ├── resumeHeartbeat() on all stream receivers
    └── ReplayEngine::resumeReplay()
            │
            ├── Adjust wall-clock anchor by pause duration
            ├── Set state = Playing
            ├── scheduleNextBar() → starts bar timer
            ├── scheduleNextDepth() → starts depth timer
            └── scheduleNextQuote() → starts quote timer
```

### 3. Data Emission Loop

Each stream (bars, depth, and quotes) has its own independent timer and emission loop:

```
ReplayEngine::onBarTimerTick()    onDepthTimerTick()    onQuoteTimerTick()
    │                                 │                      │
    ├── emitNextBar()                 ├── emitNextDepth()    ├── emitNextQuote()
    │       │                         │       │              │       │
    │       ├── Get next from         │       ├── Get next   │       ├── Get next from
    │       │   m_barLoader           │       │   m_depth-   │       │   m_quoteLoader
    │       ├── updateReplayTime()    │       │   Loader     │       ├── Filter symbol
    │       └── emit injectBarData()  │       ├── updateTime │       │   (QuoteStream only;
    │                                 │       └── emit       │       │    Heartbeat/Error
    └── scheduleNextBar()             │         injectDepth  │       │    always pass)
            │                         └── scheduleNext-      │       └── emit
            └── m_barTimer.start(t)       Depth()            │         injectQuoteData()
                                                             └── scheduleNextQuote()
```

**Wall-Clock Anchored Timing**: All three streams share a single wall-clock anchor point.
Instead of computing delay from timestamp deltas between consecutive points, each
data point's delay is calculated as:
```
targetWallMs = m_wallClockAnchorMs + (replayEpochMs - m_replayEpochAnchorMs) * 100 / speed
delay = max(0, targetWallMs - now)
```
This prevents timing drift from three independent timers and ensures accurate playback speed.

**Quote filtering**: In `emitNextQuote()`, rows with `objectType == "QuoteStream"` are only
emitted if `TSClient::hasOpenQuoteStream()` returns true. Heartbeat and Error rows always pass
through to maintain stream health.

### 4. Pausing Replay

```
User clicks "Pause" or presses Spacebar
    │
    ▼
MainApp::pauseReplayPlayback()
    │
    ▼
MainAlgo::pauseReplay()
    │
    ├── ReplayEngine::pauseReplay() → stops all three timers (bar, depth, quote)
    └── pauseHeartbeat() on ALL stock instruments
            │
            └── Stream::pauseHeartbeat() [thread-safe via invokeMethod]
```

## Playback Speed

Defined in `ReplayEngine::PlaybackSpeed`:

| Enum Value | Speed | Effect |
|------------|-------|--------|
| SuperSlow (1) | 0.01x | 100x slower |
| VerySlow (10) | 0.1x | 10x slower |
| Half (50) | 0.5x | 2x slower |
| Normal (100) | 1.0x | Real-time |
| Double (200) | 2.0x | 2x faster |
| Fast5x (500) | 5.0x | 5x faster |
| Fast10x (1000) | 10.0x | 10x faster |
| AsFastAsPossible (-1) | Max | 0ms timer delays |

Speed can be changed on-the-fly via `MainApp::setReplaySpeed()`. When changed during
playback, `setSpeed()` re-anchors the wall-clock mapping to the current instant and
reschedules both stream timers, so the new speed takes effect immediately.

### Pause/Resume Timing

- **Pause**: Records `m_pauseWallClockMs = now`, stops both timers
- **Resume**: Shifts `m_wallClockAnchorMs` forward by the pause duration so timing
  stays accurate across pauses. If resuming after `startReplayPaused()`, the anchor
  is set to "now" on first resume.
- `updateReplayTime()` only advances `MainApp::currentAppReplayTime` forward (never backward),
  so whichever stream has the latest timestamp drives the displayed clock.

## Heartbeat Timer Management

Streams have a 10-second heartbeat timer that triggers if no data arrives. In replay mode:

1. **When paused**: Timers must be paused to prevent timeout errors
2. **When resumed**: Timers must be restarted
3. **Thread safety**: `pauseHeartbeat()`/`resumeHeartbeat()` use `Qt::BlockingQueuedConnection` when called cross-thread

The `StreamReceiver` base class provides a unified interface for heartbeat management across:
- BarReceiver
- MarketDepthQuoteReceiver
- PositionsReceiver
- OrdersReceiver

## File Locations

| Data Type | Path Pattern |
|-----------|--------------|
| Recorded Bars | `~/.cache/L2Trader/RecordedLiveData/Bars/{YYYY-MM-DD}.db` |
| Recorded Depth | `~/.cache/L2Trader/RecordedLiveData/MarketDepthQuotes/{YYYY-MM-DD}.db` |
| Recorded Quotes | `~/.cache/L2Trader/RecordedLiveData/Quotes/{YYYY-MM-DD}.db` |

## Common Tasks

### Adding a New Data Type to Replay

1. Add new `StreamType` to `LiveStreamDB` (recorder side)
2. Add new table creation in `LiveStreamDB::createTable()`
3. Add new `DataType` enum value to `ReplayDataLoader`
4. Create a new `ReplayDataLoader` instance in `ReplayEngine` for the data type
5. Add a new `QTimer`, emit/schedule function pair, and stream-ended flag in `ReplayEngine`
6. Add loading logic in `ReplayDataLoader::loadBufferChunk()` for the new table
7. Add injection signal in `ReplayEngine` (e.g., `injectNewDataType`)
8. Connect signal to `TSClient::onInjectNewDataType()`
9. Route to appropriate `MockNetworkReply` in TSClient

If the new stream records multiple object types (like Quotes), add an `objectType` field to `ReplayDataPoint` and filter in the emit function.

### Debugging Replay Issues

1. Enable `ReplayEngine` and `ReplayDataLoader` logging categories
2. Check heartbeat timer states in `Stream` logs
3. Verify data exists in SQLite databases using sqlite3 CLI
4. Check `MainApp::getDataSourceMode()` is correctly set to Replay

## Threading Model

```
Main Thread (GUI)
    │
    ├── TSClient (singleton)
    │       └── MockNetworkReply objects
    │
    └── GUIFrontend
            └── StockPriceChart / ChartToolbar

MainAlgoThread
    │
    └── MainAlgo
            ├── ReplayEngine
            │       ├── m_barTimer + ReplayDataLoader (Bar)
            │       ├── m_depthTimer + ReplayDataLoader (MarketDepthQuote)
            │       └── m_quoteTimer + ReplayDataLoader (Quote)
            │
            ├── StockInstruments (BarReceiver, MarketDepthQuoteReceiver)
            └── PositionsReceiver, OrdersReceiver
```

Cross-thread communication uses `Qt::QueuedConnection` or `Qt::BlockingQueuedConnection` for synchronization.

## Order & Position Emulation

When in replay mode, orders and positions are emulated locally rather than sent to any external API:

```
Strategy/GUI ──► placeOrder() ──► MockNetworkAccessManager
                                          │
                                          ▼
                                    OrderEmulator
                                          │
                                    ┌─────┴─────┐
                                    ▼           ▼
                              OPN status   FLL status
                                    │           │
                                    └─────┬─────┘
                                          ▼
                                   MockNetworkReply (orders)
                                          │
                                          ▼
                                    StreamOrders
                                          │
                                          ▼
                                    OrdersReceiver
                                          │
                                          ▼
                                         GUI
```

### Key Components

| Component | Role |
|-----------|------|
| `OrderEmulator` | Simulates order lifecycle with realistic delays |
| `MockNetworkAccessManager` | Intercepts HTTP requests, routes to emulator |
| Mock StreamOrders/StreamPositions | Receive emulated updates via MockNetworkReply |

### Market Depth Integration

The OrderEmulator monitors market depth (Level 2) to fill pending limit orders:

1. `ReplayEngine` emits `injectDepthData` signal
2. `TSClient::onInjectDepthData()` receives it
3. Depth is forwarded to `OrderEmulator::updateMarketDepth()`
4. Emulator checks if any open limit orders can now fill
5. Fills are processed with appropriate delays

### Level 1 Quote Integration

The OrderEmulator also uses Level 1 quotes as a fallback when no Level 2 data exists:

1. `ReplayEngine` emits `injectQuoteData` signal
2. `TSClient::onInjectQuoteData()` receives it
3. Parsed Quote is forwarded to `OrderEmulator::updateQuote()`
4. Emulator uses bid/ask from Quote when no depth snapshot exists
5. `recalculatePositionPnL()` uses Quote bid/ask for mark-to-market

**Priority**: Level 2 is always used when available. Level 1 is strictly a fallback.

### Simulated Account

A single simulated account `SIM123456` is used for all replay orders:
- Starting balance: $100,000
- Full order validation (balance, boxing prevention)
- Position tracking with P&L calculations

See `OrderEmulator/AGENTS.md` for detailed documentation.
