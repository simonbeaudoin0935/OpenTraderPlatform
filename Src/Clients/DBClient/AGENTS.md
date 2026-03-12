# DBClient — Databento API Client

## Purpose

`DBClient` is the singleton that wraps the **databento-cpp** library, providing live market data streaming and historical bar fetching. It translates raw Databento records into L2Trader domain types (`Level2`, `Trade`, `Bar`) and emits Qt signals for downstream consumers.

**TradeStation** remains the brokerage client (orders, positions, accounts). **Databento** handles all market data.

## Files

| File | Description |
|------|-------------|
| `DBClient.h` | Singleton header — ConnectionState enum, live/historical API, signals |
| `DBClient.cpp` | Implementation — LiveThreaded lifecycle, Historical fetching, replay download, callbacks |
| `DBRecordTranslator.h` | Header-only static conversion functions (Databento record → domain type) |
| `LiveBarAccumulator.h` | Accumulates Trade records into forming 1-minute OHLCV bars |
| `LiveBarAccumulator.cpp` | Implementation — floor-to-open-time, minute-boundary rollover |

## Architecture

### Threading Model

```
DBClient singleton (lives on DBClient QThread)
  │
  ├─ Databento LiveThreaded (Databento's internal thread)
  │   └─ RecordCallback (runs on Databento's thread)
  │       ├─ Updates PitSymbolMap (mutex-protected)
  │       ├─ Translates record via DBRecordTranslator
  │       └─ Emits Qt signal (auto-queued to receiver thread)
  │            └─ MainAlgo thread (Level2Receiver, etc.)
  │
  ├─ Historical fetch (QThreadPool via QtConcurrent::run)
  │   └─ Emits historicalBarsReceived on completion
  │
  └─ DBClient thread event loop
      └─ Hosts state-modifying methods (connectLive, subscribeLive, etc.)
      └─ Will host replay QTimer tick loop (future Phase 5)
```

- **DBClient lives on its own QThread** (like TSClient), created via `moveToThread(&m_thread)`.
- **State-modifying methods** self-route to the DBClient thread if called from another thread (e.g., GUI). This pattern uses `QMetaObject::invokeMethod` with `Qt::QueuedConnection`.
- **Live callbacks** run on Databento's internal thread. Qt `AutoConnection` queues signals to the receiver's event loop.
- **Historical fetching** runs on `QThreadPool` via `QtConcurrent::run()`. Results arrive via `historicalBarsReceived` signal.
- **In live mode**, the DBClient thread mostly idles (Databento's LiveThreaded does the heavy lifting).
- **API key** is loaded eagerly in the constructor (before `moveToThread`) so `hasApiKey()` is valid immediately.

### Connection State Machine

```
Disconnected ──[connectLive()]──► Connecting ──[MetadataCallback]──► Connected
                                                                        │
Connected ──[disconnectLive()]──► Disconnected                          │
Connected ──[exception]──► Reconnecting ──[ExceptionAction::Restart]──► Connecting
```

States are tracked via `ConnectionState` enum. Transitions emit `liveConnectionStateChanged()`.

### Subscription Model

- Each symbol subscribes to `Schema::Mbp10` (10-level book) + `Schema::Trades`.
- Subscriptions **accumulate** — Databento does NOT support unsubscribe.
- Old subscriptions remain active; data for non-displayed symbols is ignored by receivers.
- Call `subscribeLive()` only after `ConnectionState::Connected`.

### Symbol Resolution

Databento records carry `instrument_id` (uint32). The `PitSymbolMap` (built from `Metadata` on session start) maps these to ticker strings. The map is updated on `SymbolMappingMsg` records via `OnRecord()`.

Access to `m_symbolMap` is protected by `m_symbolMapMutex` since the callback thread and potential query callers may both access it.

## Signals

| Signal | Thread Context | Description |
|--------|---------------|-------------|
| `connectionStateChanged(bool)` | DBClient thread | API-key-based connection state (for GUI button) |
| `liveConnectionStateChanged(ConnectionState)` | Databento callback thread | Live session lifecycle transitions |
| `newLevel2(QString, Level2)` | Databento callback thread | 10-level book snapshot |
| `newTrade(QString, Trade)` | Databento callback thread | Trade print |
| `newStatus(QString, bool, QString, bool)` | Databento callback thread | Trading status update — args: symbol, isHalted, haltReason, isSsr |
| `liveGatewayError(QString, bool)` | Databento callback thread | Gateway error — args: errorText, isFatal |
| `historicalBarsReceived(QString, QVector<Bar>)` | QThreadPool worker | Historical OHLCV bars |
| `replayDownloadFinished(QString, QDate, bool, QString)` | QThreadPool worker | Replay data download result (symbol, date, success, error) |

### Status Stream Subscription

Each `subscribeLive()` call subscribes to three schemas simultaneously:
- `Schema::Mbp10` → `newLevel2` signal
- `Schema::Trades` → `newTrade` signal
- `Schema::Status` → `newStatus` signal

`StatusMsg` records carry trading halt and SSR (short-sale restriction) state. The `newStatus(symbol, isHalted, haltReason, isSsr)` signal is emitted for each `StatusMsg` after symbol resolution.

### Gateway Error Handling

`ErrorMsg` records are handled **before** symbol resolution (they carry no instrument_id) and immediately emit `liveGatewayError(errorText, isFatal)`. Fatal error codes:

| Code | Meaning |
|------|---------|
| 1 | AuthFailed |
| 2 | ApiKeyDeactivated |
| 3 | ConnectionLimitExceeded |
| 5 | InvalidSubscription |

Non-fatal errors are logged but do not trigger UI changes.

## Replay Data Download

`downloadReplayData(symbol, date)` downloads **Mbp10** (Level 2) + **Trades** for one symbol on one date:

- Runs asynchronously via `QtConcurrent::run()` (sequential per call)
- Files stored as `~/.local/share/L2Trader/ReplayData/{YYYY-MM-DD}/{SYMBOL}_mbp10.dbn.zst` and `_trades.dbn.zst`
- Emits `replayDownloadFinished()` on completion (success or failure)
- `hasReplayData(date, symbol)` checks if both files exist
- `getReplayDataDir(date)` / `getReplayFilePath(date, symbol, schema)` provide path helpers

Called from `DownloadsTab` download section (sequential queue — one symbol at a time).

## LiveBarAccumulator

Accumulates individual `Trade` records into forming 1-minute OHLCV bars using **open-time convention**:

- A trade at 09:31:04 contributes to the bar timestamped **09:31:00** (floor to current minute)
- On minute-boundary rollover: emits `barClosed()` for the completed bar, starts new forming bar
- Every trade update: emits `barUpdated()` with the in-progress bar
- Lives on MainAlgo thread; connected to `DBClient::newTrade`
- `barClosed` is fed into `BarAggregator` which derives higher-timescale bars in real-time

## Multi-Timescale Historical Fetch

`fetchHistoricalBars(symbol, start, end, TimeFrame tf)` fetches bars at any supported timescale:

**Databento native schemas**:
- `ONE_MINUTE` → `Schema::Ohlcv1M` (fetched directly)
- `ONE_HOUR` → `Schema::Ohlcv1H` (fetched directly)
- `ONE_DAY` → `Schema::Ohlcv1D` (fetched directly)

**In-app aggregation** (fetches source, then reduces):
- `FIVE_MINUTES`, `FIFTEEN_MINUTES`, `THIRTY_MINUTES` → fetch `Ohlcv1M` then aggregate
- `FOUR_HOURS` → fetch `Ohlcv1H` then aggregate
- `ONE_WEEK`, `ONE_MONTH` → fetch `Ohlcv1D` then aggregate

Aggregation happens inside `aggregateBars(bars, targetTf)` (anonymous namespace in `DBClient.cpp`). The `exclusiveEnd` calculation uses the source TF's bar width to correctly include the last bar of any timescale.

## Bar Timestamp Convention

**Open-time** (Databento native): bars are timestamped at their **open** time.
- Bar covering 4:00:00–4:00:59 → timestamped `4:00`
- First bar of day: index 0 = `4:00 AM`
- Last bar of day: index 899 = `6:59 PM`
- 900 bars per day (4:00 AM – 6:59 PM, XNAS.ITCH hours)

## DBRecordTranslator

Header-only namespace with pure static conversion functions:

| Function | Input | Output |
|----------|-------|--------|
| `toLevel2(symbol, Mbp10Msg)` | 10-level BidAskPair array | `Level2` with 10 bid/ask rows |
| `toLevel1(symbol, Mbp1Msg)` | 1-level BidAskPair | `Level1` with bid/ask BBO |
| `toTrade(symbol, TradeMsg)` | Price/size/side | `Trade` with TradeSide |
| `toBar(symbol, OhlcvMsg)` | OHLCV fields | `Bar` with BarStatus::Closed |
| `toDouble(int64_t)` | Fixed-point price | `double` (÷ 1e9) |
| `toDateTime(UnixNanos)` | Nanosecond timestamp | `QDateTime` in America/New_York |

## Configuration

- **Dataset**: Defaults to `XNAS.ITCH` (NASDAQ TotalView). Stored in `QSettings` under `Databento/Dataset`. Configurable via `setDataset()` / Config tab (future).
- **API Key**: Stored XOR-obfuscated in `SecureStorage.ini` under `[Databento]/api_key`.

## Dependencies

- **databento-cpp**: `LiveThreaded`, `Historical`, `PitSymbolMap`, record types
- **Qt6**: Core (signals, QMutex), Concurrent (QThreadPool), Network (implicit via databento)
- **Domain models**: `Level2.h`, `Trade.h`, `Bar.h` (in `Src/Core/Models/`)

## Future Work

- **Data usage tracking**: Monitor and display Databento API costs
- **Reconnection logic**: Auto-reconnect on network drops with exponential backoff
- **Multi-dataset support**: Allow switching between XNAS.ITCH and other datasets
