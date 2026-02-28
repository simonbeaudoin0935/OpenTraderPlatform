# DBClient — Databento API Client

## Purpose

`DBClient` is the singleton that wraps the **databento-cpp** library, providing live market data streaming and historical bar fetching. It translates raw Databento records into L2Trader domain types (`Level2`, `Level1`, `Trade`, `Bar`) and emits Qt signals for downstream consumers.

**TradeStation** remains the brokerage client (orders, positions, accounts). **Databento** handles all market data.

## Files

| File | Description |
|------|-------------|
| `DBClient.h` | Singleton header — ConnectionState enum, live/historical API, signals |
| `DBClient.cpp` | Implementation — LiveThreaded lifecycle, Historical fetching, callbacks |
| `DBRecordTranslator.h` | Header-only static conversion functions (Databento record → domain type) |

## Architecture

### Threading Model

```
Databento LiveThreaded (internal thread)
  └─ RecordCallback (runs on Databento's thread)
      ├─ Updates PitSymbolMap (mutex-protected)
      ├─ Translates record via DBRecordTranslator
      └─ Emits Qt signal (auto-queued to receiver thread)
           └─ MainAlgo thread (Level2Receiver, Level1Receiver, etc.)
```

- **Live callbacks** run on Databento's internal thread. Qt `AutoConnection` queues signals to the receiver's event loop.
- **Historical fetching** runs on `QThreadPool` via `QtConcurrent::run()`. Results arrive via `historicalBarsReceived` signal.
- **API key management** runs on the main/GUI thread.

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
| `connectionStateChanged(bool)` | Main/GUI thread | API-key-based connection state (for GUI button) |
| `liveConnectionStateChanged(ConnectionState)` | Databento callback thread | Live session lifecycle transitions |
| `newLevel2(QString, Level2)` | Databento callback thread | 10-level book snapshot |
| `newLevel1(QString, Level1)` | Databento callback thread | BBO snapshot |
| `newTrade(QString, Trade)` | Databento callback thread | Trade print |
| `historicalBarsReceived(QString, QVector<Bar>)` | QThreadPool worker | Historical OHLCV bars |

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

- **Phase 5**: LiveBarAccumulator — accumulate TradeMsg into real-time 1-minute bars
- **Phase 6**: Wire DBClient signals to MainAlgo receivers, BarCache historical backfill
- **Phase 7**: DbnFileStore replay integration, replace old Recorder/ReplayEngine
