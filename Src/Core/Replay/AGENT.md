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
│       │                         {date}.db                                   │
│       │                                                                      │
│       └──► StreamMarketDepthQuote ──► LiveStreamDB (Depth)                  │
│                                            │                                 │
│                                            ▼                                 │
│                                    {date}.db                                │
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
│       ▼                                                                  │   │
│   ReplayDataLoader                                                       │   │
│       │                                                                  │   │
│       │  Loads from SQLite DBs                                          │   │
│       │  Ping-pong buffer strategy                                      │   │
│       │  Merges bars + depth chronologically                            │   │
│       │                                                                  │   │
│       ▼                                                                  │   │
│   ReplayDataPoint (bars/depth JSON)                                     │   │
│       │                                                                  │   │
│       │  injectBarData / injectDepthData signals                        │   │
│       ▼                                                                  │   │
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
| `ReplayEngine.h/cpp` | Orchestrates replay playback. Manages state (Stopped/Playing/Paused), speed, timing |
| `ReplayDataLoader.h/cpp` | Loads data from SQLite DBs into memory buffers. Ping-pong buffering for efficiency |

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
            ├── Set state = Playing
            └── scheduleNextDataPoint() → starts timer
```

### 3. Data Emission Loop

```
ReplayEngine::onTimerTick()
    │
    ├── emitCurrentDataPoint()
    │       │
    │       ├── Get next ReplayDataPoint from ReplayDataLoader
    │       ├── emit injectBarData(symbol, jsonData) OR
    │       └── emit injectDepthData(symbol, jsonData)
    │
    └── scheduleNextDataPoint()
            │
            ├── Calculate delta to next timestamp
            ├── Scale by playback speed
            └── Start single-shot timer
```

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
    ├── ReplayEngine::pauseReplay() → stops timer
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

Speed can be changed on-the-fly via `MainApp::setReplaySpeed()`.

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

## Common Tasks

### Adding a New Data Type to Replay

1. Add new `StreamType` to `LiveStreamDB` (recorder side)
2. Add new table creation in `LiveStreamDB::createTable()`
3. Add new `ReplayDataPoint::Type` in `ReplayDataLoader`
4. Add loading logic in `ReplayDataLoader::loadBufferChunk()`
5. Add injection signal in `ReplayEngine` (e.g., `injectNewDataType`)
6. Connect signal to `TSClient::onInjectNewDataType()`
7. Route to appropriate `MockNetworkReply` in TSClient

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
            │       └── ReplayDataLoader
            │
            ├── StockInstruments (BarReceiver, MarketDepthQuoteReceiver)
            └── PositionsReceiver, OrdersReceiver
```

Cross-thread communication uses `Qt::QueuedConnection` or `Qt::BlockingQueuedConnection` for synchronization.
