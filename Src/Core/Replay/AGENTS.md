# Replay System - Agent Instructions

## Overview

The replay system enables replaying historical market data for strategy backtesting and debugging. `ReplayEngine` reads Databento `.dbn.zst` archive files directly using `databento::DbnFileStore`. There is no custom data loader — Databento files are the sole replay data source.

## File Structure

| File | Purpose |
|------|---------|
| `ReplayEngine.h/cpp` | Orchestrates replay playback using `databento::DbnFileStore` for Level 2 and Trades data |
| `OrderEmulator/` | Order/position simulation for replay mode. See `OrderEmulator/AGENTS.md` for details |

## Replay Data Source

ReplayEngine uses two `databento::DbnFileStore` instances:

| Member | Schema | File Path |
|--------|--------|-----------|
| `m_mbp10Store` | `Mbp10` (Level 2) | `~/.cache/L2Trader/ReplayData/{YYYY-MM-DD}/{SYMBOL}_mbp10.dbn.zst` |
| `m_tradesStore` | `Trades` | `~/.cache/L2Trader/ReplayData/{YYYY-MM-DD}/{SYMBOL}_trades.dbn.zst` |

Files are downloaded by `DBClient::downloadReplayData(symbol, date)` before replay begins. Use `DBClient::hasReplayData(date, symbol)` to check availability.

## Signals Emitted by ReplayEngine

```cpp
signals:
    void replayLevel2(QString symbol, Level2 level2);
    void replayTrade(QString symbol, Trade trade);
    // Lifecycle signals
    void replayStarted();
    void replayStopped();
    void replayPaused();
    void replayResumed();
    void replayTimeUpdated(QDateTime currentTime);
    void replayEndReached();
```

## Signal Routing (MainAlgo::connectReplaySignals)

```
ReplayEngine::replayLevel2  ──► Level2Receiver::onReceivedNewLevel2
                            ──► OrderEmulator::updateMarketDepth

ReplayEngine::replayTrade   ──► LiveBarAccumulator (builds 1-min bars)
                            ──► TimeAndSales widget

LiveBarAccumulator::barUpdated ──► (chart receives forming bar)
LiveBarAccumulator::barClosed  ──► OrderEmulator::updateBarClose
```

`LiveBarAccumulator` (from `Src/Clients/DBClient/`) is reused in replay to build bars from trade records, identical to how live mode works.

## Integration Points

## Integration Points

| File | Role in Replay |
|------|----------------|
| `MainApp.cpp` | Entry point for mode switching: `enterReplayMode()`, `exitReplayMode()`, `setReplaySpeed()` |
| `MainAlgo.cpp` | Creates ReplayEngine, calls `connectReplaySignals()`, manages stock instruments, pauses/resumes heartbeat timers |
| `TSClient.cpp` | Switches to Replay mode: creates `MockNetworkReply` objects, routes orders to `OrderEmulator` |
| `MockNetworkReply.h/cpp` | Fake QNetworkReply that receives injected data from OrderEmulator |

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
            ├── connectReplaySignals() — wire replayLevel2/replayTrade to receivers
            ├── startReplayPaused() → emits first records, then pauses
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
            ├── scheduleNextMbp10() → starts Level2 timer
            └── scheduleNextTrade() → starts Trades timer
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

Streams have a 10-second heartbeat timer. In replay mode:

1. **When paused**: Timers paused to prevent timeout errors
2. **When resumed**: Timers restarted
3. **Thread safety**: `pauseHeartbeat()`/`resumeHeartbeat()` use `Qt::BlockingQueuedConnection` when called cross-thread

## Debugging Replay Issues

1. Enable `ReplayEngine` logging category
2. Check heartbeat timer states in `Stream` logs
3. Verify `.dbn.zst` files exist: `~/.cache/L2Trader/ReplayData/{YYYY-MM-DD}/{SYMBOL}_mbp10.dbn.zst`
4. Check `MainApp::getDataSourceMode()` is set to Replay
5. Use `DBClient::hasReplayData(date, symbol)` to verify data availability

## Threading Model

```
MainAlgoThread
    │
    └── MainAlgo
            ├── ReplayEngine
            │       ├── m_mbp10Store (DbnFileStore — Level2)
            │       ├── m_tradesStore (DbnFileStore — Trades)
            │       ├── m_mbp10Timer (schedules Level2 events)
            │       └── m_tradesTimer (schedules Trade events)
            │
            ├── StockInstruments (Level2Receiver)
            ├── LiveBarAccumulator
            └── PositionsReceiver, OrdersReceiver

Main/GUI Thread
    │
    └── TSClient
            └── MockNetworkReply objects (replay order injection)
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

The OrderEmulator monitors Level 2 depth to fill pending limit orders:

1. `ReplayEngine` emits `replayLevel2` signal
2. `OrderEmulator::updateMarketDepth()` receives it
3. Emulator checks if any open limit orders can now fill
4. Fills are processed with appropriate delays
5. `LiveBarAccumulator::barClosed` → `OrderEmulator::updateBarClose`

### Simulated Account

A single simulated account `SIM123456` is used for all replay orders:
- Starting balance: $100,000
- Full order validation (balance, boxing prevention)
- Position tracking with P&L calculations

See `OrderEmulator/AGENTS.md` for detailed documentation.
