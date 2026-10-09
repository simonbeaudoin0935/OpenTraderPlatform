# Replay System - Agent Instructions

## Overview

The replay system enables replaying historical market data for strategy backtesting and debugging. Replay playback logic lives in `DBClient`, which reads Databento `.dbn.zst` archive files directly using `databento::DbnFileStore`. In replay mode, DBClient emits the same `newLevel2`/`newTrade` signals as live mode, enabling a unified data pipeline.

## File Structure

| File | Purpose |
|------|---------|
| `OrderEmulator/` | Order/position simulation for replay mode. See `OrderEmulator/AGENTS.md` for details |

> **Note**: `ReplayEngine.h/cpp` was removed. All playback logic now resides in `Src/Clients/DBClient/DBClient.cpp`. Playback enums are in `Src/Clients/DBClient/PlaybackTypes.h`.

## Replay Data Source

DBClient uses one replay book store plus a trades store for replay:

| Member | Schema | File Path |
|--------|--------|-----------|
| `m_level2Store` | `Mbp10` or `Mbp1` (Level 2 / top-of-book) | `~/.local/share/OpenTraderPlatform/ReplayData/{YYYY-MM-DD}/{SYMBOL}_mbp10.dbn.zst` or `_mbp1.dbn.zst` |
| `m_tradesStore` | `Trades` | `~/.local/share/OpenTraderPlatform/ReplayData/{YYYY-MM-DD}/{SYMBOL}_trades.dbn.zst` |

Files are downloaded by `DBClient::downloadReplayData(symbol, date)` before replay begins. Use
`DBClient::hasReplayData(date, symbol)` to check availability; it accepts either `mbp10 + trades` or
`mbp1 + trades`.

## Unified Signal Pipeline

In replay mode, DBClient emits the **same signals** as live mode:

```cpp
// These signals are emitted in both live AND replay modes:
void newLevel2(QString symbol, Level2 level2);
void newTrade(QString symbol, Trade trade);

// Replay lifecycle signals:
void replayStarted();
void replayStopped();
void replayPaused();
void replayResumed();
void replayTimeUpdated(QDateTime currentTime);
void replayEndReached();
void replayDataLoadFailed(QString symbol, QString errorMessage);
```

## Signal Routing (Unified Live/Replay)

```
DBClient::newLevel2      ──► MainAlgo::onNewLevel2Received()
                              ──► SymbolContext::enqueueLevel2()
                              ──► QThreadPool drain() → Level2Receiver
                         ──► OrderEmulator::updateMarketDepth (replay only)

DBClient::newTrade       ──► MainAlgo::onNewTradeReceived()
                              ──► SymbolContext::enqueueTrade()
                              ──► QThreadPool drain() → LiveBarAccumulator

LiveBarAccumulator::barClosed  ──► BarReceiver → BarCache + GUI
                               ──► OrderEmulator::updateBarClose (replay only)
```

`LiveBarAccumulator` (from `Src/Clients/DBClient/`) is reused in replay to build bars from trade records, identical to how live mode works.

## Integration Points

| File | Role in Replay |
|------|----------------|
| `MainApp.cpp` | Entry point for mode switching: `enterReplayMode()`, `exitReplayMode()`, `setReplaySpeed()` |
| `MainAlgo.cpp` | Forwards DBClient replay signals, manages SymbolContexts, pauses/resumes heartbeat timers |
| `DBClient.cpp` | Replay playback engine: opens `.dbn.zst` files, timer-based emission, speed control |
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
    ├── MainAlgo::deleteAllSymbolContext()
    ├── MainAlgo::createAndSetDisplayedStockInstrument()
    └── MainAlgo::enterReplayModePaused()
            │
            ├── Connect DBClient replay signals to MainAlgo (replayStarted, etc.)
            ├── Connect DBClient::newLevel2 to OrderEmulator::updateMarketDepth
            ├── DBClient::startReplayPaused() → opens .dbn.zst, emits first records, pauses
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
    └── DBClient::resumeReplay()
            │
            ├── Adjust wall-clock anchor by pause duration
            ├── Set state = Playing
            └── scheduleNextReplayTick() → starts QTimer-based event loop
```

### Replay strategy gating

- Replay preload is **visible but non-tradable**. `startReplayPaused()` is for chart/data setup only.
- Before the first Play press, replay strategies may exist in a **primed** state, but their child processes are not launched yet.
- The first Play press starts all primed strategies, waits for their explicit ready acknowledgements, and only then resumes DBClient playback.
- After replay has started once, replay pause/resume maps to strategy pause/resume.

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
    ├── DBClient::pauseReplay() → stops timer, records pause time
    └── pauseHeartbeat() on ALL SymbolContexts
```

## Playback Speed

Defined in `Playback::Speed` (from `Src/Clients/DBClient/PlaybackTypes.h`):

| Enum Value | Speed | Effect |
|------------|-------|--------|
| SuperSlow (1) | 0.01x | 100x slower |
| VerySlow (10) | 0.1x | 10x slower |
| Half (50) | 0.5x | 2x slower |
| Normal (100) | 1.0x | Real-time |
| Double (200) | 2.0x | 2x faster |
| Fast5x (500) | 5.0x | 5x faster |
| Fast10x (1000) | 10.0x | 10x faster |
| AsFastAsPossible (-1) | Max | 0ms timer delays, batch budget limited |

Speed can be changed on-the-fly via `MainApp::setReplaySpeed()`. When changed during
playback, `setReplaySpeed()` re-anchors the wall-clock mapping to the current instant and
reschedules the replay timer, so the new speed takes effect immediately.

### Pause/Resume Timing

- **Pause**: Records `m_pauseWallClockMs = now`, stops timer
- **Resume**: Shifts `m_wallClockAnchorMs` forward by the pause duration so timing
  stays accurate across pauses. If resuming after `startReplayPaused()`, the anchor
  is set to "now" on first resume.
- `updateReplayTime()` only advances `MainApp::currentAppReplayTime` forward (never backward),
  so whichever stream has the latest timestamp drives the displayed clock.

## Heartbeat Timer Management

Streams have a ten-second heartbeat timer. In replay mode:

1. **When paused**: Timers paused to prevent timeout errors
2. **When resumed**: Timers restarted
3. **Thread safety**: `pauseHeartbeat()`/`resumeHeartbeat()` use `Qt::BlockingQueuedConnection` when called cross-thread

## Debugging Replay Issues

1. Enable `DBClient` logging category
2. Check heartbeat timer states in `Stream` logs
3. Verify `.dbn.zst` files exist: `~/.local/share/OpenTraderPlatform/ReplayData/{YYYY-MM-DD}/{SYMBOL}_mbp10.dbn.zst`
   or `.../{SYMBOL}_mbp1.dbn.zst`, plus `{SYMBOL}_trades.dbn.zst`
4. Check `MainApp::getDataSourceMode()` is set to Replay
5. Use `DBClient::hasReplayData(date, symbol)` to verify data availability
6. Use LTTng `replay_tick` tracepoint to monitor batch sizes and event emission rate

## Threading Model

```
DBClient Thread
    │
    └── DBClient
            ├── m_mbp10Store (DbnFileStore — Level2 replay)
            ├── m_tradesStore (DbnFileStore — Trades replay)
            ├── m_replayTimer (QTimer — drives replay tick loop)
            └── emits newLevel2/newTrade (same as live mode)
                    │
                    ▼
MainAlgo Thread (routing)
    │
    └── MainAlgo
            ├── m_symbolContexts[symbol]->enqueueLevel2/Trade()
            ├── PositionsReceiver, OrdersReceiver
            └── forwards to GUI via displayedStock* signals
                    │
                    ▼
QThreadPool Workers (drain loops)
    │
    └── SymbolContext::drain()
            ├── Level2Receiver::onReceivedNewLevel2()
            └── LiveBarAccumulator::onNewTrade()

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

1. DBClient emits `newLevel2` signal
2. MainAlgo's `connectReplaySignals()` wires it to `OrderEmulator::updateMarketDepth()`
3. Emulator checks if any open limit orders can now fill
4. Fills are processed with appropriate delays
5. `LiveBarAccumulator::barClosed` → `OrderEmulator::updateBarClose`

### Replay order safety

- The GUI order-entry widget already blocks orders while replay is not running.
- The shared host order path now also rejects replay order placement unless playback state is `Playing`, so strategy orders cannot trade during preload or pause.
- `MockNetworkAccessManager` performs the same check as defense in depth before forwarding replay order requests to `OrderEmulator`.

### Simulated Account

A single simulated account `SIM123456` is used for all replay orders:
- Starting balance: $100,000
- Full order validation (balance, boxing prevention)
- Position tracking with P&L calculations

See `OrderEmulator/AGENTS.md` for detailed documentation.
