# Replay System - Agent Instructions

> **⚠️ TRANSITIONAL SUBSYSTEM — Databento Migration Phase 7**
>
> The entire replay system is being revamped as part of Phase 7 of the Databento
> migration. The current `ReplayEngine` and `ReplayDataLoader` contain TODO stubs
> and will be **replaced** by Databento `DbnFileStore::Replay()`. Recording is no
> longer done via a custom recorder — it is replaced by Databento `.dbn` archive
> downloads.
>
> **Do not reference deleted files or types** (see Deleted Components below).
> The `OrderEmulator/` subdirectory is the only component that will survive the
> revamp unchanged.

## Overview

The Replay system enables replaying historical market data for strategy backtesting and debugging. This document describes the current transitional state for AI agents working on this codebase.

## Current State

The replay subsystem is in a transitional state:

- **ReplayEngine** still exists but has TODO stubs (e.g., `hasStreamForStock()` always returns `true`).
- **ReplayDataLoader** still exists and uses inlined SQL queries from the `LegacyReplayQueries` namespace (formerly in the deleted `LiveStreamDBQueries.h`).
- **OrderEmulator/** is kept and fully functional — it uses the `Level2` type (from `Src/Core/Models/Level2.h`) instead of the deleted `MarketDepthQuote`.
- **Recording** no longer uses a custom recorder executable. It is being replaced by Databento `.dbn` archive downloads.

### What Phase 7 Will Do

Phase 7 will replace `ReplayEngine` and `ReplayDataLoader` with Databento `DbnFileStore::Replay()`, which natively handles bar, Level 2, and trade data from `.dbn` files. The `OrderEmulator/` will be retained and wired into the new replay pipeline.

## Deleted Components (DO NOT REFERENCE)

### Deleted Files

| File | Status |
|------|--------|
| `LiveStreamDB.h/.cpp` | Deleted in Phase 3 |
| `LiveStreamDBQueries.h` | Deleted — queries inlined into `ReplayDataLoader` as `LegacyReplayQueries` |
| `RecorderLogic.h/.cpp` | Deleted |
| `RecorderUtils.h/.cpp` | Deleted |
| `StatusReporter.h/.cpp` | Deleted |
| `main.cpp` (recorder binary) | Deleted |

### Deleted Types

| Type | Status |
|------|--------|
| `StreamBars` | Deleted |
| `StreamMarketDepthQuote` | Deleted |
| `StreamQuote` | Deleted |
| `MarketDepthQuote` | Replaced by `Level2` (from `Src/Core/Models/Level2.h`) |
| `Quote` | Removed entirely |
| `MarketDepthQuoteReceiver` | Replaced by `Level2Receiver` |

## File Structure (Current)

| File | Purpose |
|------|---------|
| `ReplayEngine.h/cpp` | Orchestrates replay playback. Currently has TODO stubs — `hasStreamForStock()` always returns `true`. Will be replaced by Databento `DbnFileStore::Replay()` in Phase 7 |
| `ReplayDataLoader.h/cpp` | Loads legacy recorded data from SQLite databases. Uses inlined `LegacyReplayQueries` namespace. Will be replaced in Phase 7 |
| `OrderEmulator/` | Order/position simulation for replay mode. Uses `Level2` type. Will survive Phase 7 revamp. See `OrderEmulator/AGENTS.md` for details |

## Integration Points

| File | Role in Replay |
|------|----------------|
| `MainApp.cpp` | Entry point for mode switching: `enterReplayMode()`, `exitReplayMode()`, `setReplaySpeed()` |
| `MainAlgo.cpp` | Creates ReplayEngine, manages stock instruments, pauses/resumes heartbeat timers |
| `TSClient.cpp` | Switches between Live/Replay modes. In Replay, creates MockNetworkReply objects |
| `MockNetworkReply.h/cpp` | Fake QNetworkReply that receives injected data from ReplayEngine |

## Database Schema (Legacy — Will Be Replaced)

> **Note**: These schemas describe the legacy SQLite databases written by the deleted
> recorder. Phase 7 will replace these with Databento `.dbn` files. `ReplayDataLoader`
> still reads from these databases using `LegacyReplayQueries`.

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
    stockTicker TEXT NOT NULL,
    epochMs INTEGER NOT NULL,
    objectType TEXT NOT NULL,
    jsonRawData TEXT NOT NULL
);
CREATE INDEX idx_quotes_ticker_epoch ON quotes(stockTicker, epochMs);
CREATE INDEX idx_quotes_object_type ON quotes(objectType);
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
            ├── Adjust wall-clock anchor by pause duration
            ├── Set state = Playing
            ├── scheduleNextBar() → starts bar timer
            └── scheduleNextDepth() → starts depth timer
```

### 3. Pausing Replay

```
User clicks "Pause" or presses Spacebar
    │
    ▼
MainApp::pauseReplayPlayback()
    │
    ▼
MainAlgo::pauseReplay()
    │
    ├── ReplayEngine::pauseReplay() → stops timers
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
reschedules stream timers, so the new speed takes effect immediately.

### Pause/Resume Timing

- **Pause**: Records `m_pauseWallClockMs = now`, stops timers
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
- Level2Receiver
- PositionsReceiver
- OrdersReceiver

## File Locations (Legacy)

> **Note**: These paths apply to legacy recorded data. Phase 7 will use Databento `.dbn` archives instead.

| Data Type | Path Pattern |
|-----------|--------------|
| Recorded Bars | `~/.cache/L2Trader/RecordedLiveData/Bars/{YYYY-MM-DD}.db` |
| Recorded Depth | `~/.cache/L2Trader/RecordedLiveData/MarketDepthQuotes/{YYYY-MM-DD}.db` |
| Recorded Quotes | `~/.cache/L2Trader/RecordedLiveData/Quotes/{YYYY-MM-DD}.db` |

## Common Tasks

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
            │       └── m_depthTimer + ReplayDataLoader (Level2)
            │
            ├── StockInstruments (BarReceiver, Level2Receiver)
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

### Simulated Account

A single simulated account `SIM123456` is used for all replay orders:
- Starting balance: $100,000
- Full order validation (balance, boxing prevention)
- Position tracking with P&L calculations

See `OrderEmulator/AGENTS.md` for detailed documentation.
