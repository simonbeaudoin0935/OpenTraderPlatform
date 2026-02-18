# Implementation Plan: Level 1 Quote Stream Support

## Problem Statement

The TradeStation API has a hard limit of 10 concurrent Level 2 (market depth) streams and a soft limit of ~100 concurrent bar streams. When recording/replaying 100 stocks, only 10 can have Level 2 data. In replay mode, the OrderEmulator currently rejects orders for symbols without Level 2 data ("No market data available for symbol: X"), severely limiting simulation relevance.

## Solution Overview

Implement support for Level 1 (Quote) streams using the TradeStation "Stream Quotes" API endpoint. Level 1 provides best bid/ask without full book depth. This will enable:

1. **All 100 stocks** to have bid/ask data (via Level 1 streams)
2. **First 10 stocks** to also have full Level 2 depth (via 10 separate Level 2 streams)
3. **OrderEmulator** to fall back to Level 1 best bid/ask when Level 2 unavailable
4. **Strategy notifications** for market flags (IsHalted, IsDelayed, IsBats, IsHardToBorrow)

## Progress Summary

| Phase | Description | Status |
|-------|-------------|--------|
| Phase 1-2 | MarketFlags, Quote, StreamQuote, TSClient integration | ✅ DONE |
| Phase 3 | OrderEmulator Level 1 fallback | ✅ DONE |
| Phase 4 | Recording & Replay system | ✅ DONE |
| Phase 4B | Records Info Tab Quote Display | ✅ DONE |
| Phase 5 | MarketFlags GUI display | ✅ DONE |
| Phase 6 | Market Depth widget L1 mode | ✅ DONE |

## Completed Work

### Phases 1-2: Core Infrastructure (DONE)

**Commit**: `feat(TSClient): Add Level 1 Quote stream support (Phases 1-2)`

- [x] Created `MarketFlags` class (`Src/Clients/TSClient/MarketData/GetQuoteSnapshots/MarketFlags.h/.cpp`)
  - IsBats, IsDelayed, IsHalted, IsHardToBorrow booleans
  - Constructor from QJsonObject, toJson(), isValid()

- [x] Enhanced `Quote` class with full QuoteStream API fields:
  - 52-week high/low with timestamps
  - Futures-specific fields (MinPrice, MaxPrice, FirstNoticeDate, LastTradingDate, DailyOpenInterest)
  - MarketFlags integration
  - Restrictions array, TickSizeTier

- [x] Created `StreamQuote` class (`Src/Clients/TSClient/MarketData/StreamQuote/`)
  - Extends StreamMarketData
  - Multi-symbol support (up to 100 symbols per stream)
  - Static stream counter `s_numberOfQuoteStreams`
  - Signal: `newQuoteReceived(Quote quote)`

- [x] Added TSClient integration:
  - `openStreamQuote(const QStringList& symbols)` method
  - Replay mode support with `m_replayQuoteReplies` map
  - `hasOpenQuoteStream()` method
  - `onInjectQuoteData()` for replay injection

- [x] Added constants to `CONSTANTS.h`:
  - `QuoteConstants::MAX_SYMBOLS_PER_STREAM = 100`
  - `BarStreamConstants::MAX_CONCURRENT_BAR_STREAMS = 100`
  - `TSClientEndpoints::STREAM_QUOTES`

### Phase 3: OrderEmulator Integration (DONE)

**Commit**: `feat(OrderEmulator): Add Level 1 quote fallback for order fills (Phase 3)`

- [x] Added Quote snapshot storage (`m_quoteSnapshots` map)
- [x] Added `updateQuote()` method for receiving Level 1 data
- [x] Added `canFillLimitOrderFromQuote()` helper
- [x] Added `calculateMarketOrderFillPriceFromQuote()` helper
- [x] Modified `validateOrderRequest()` to accept L1 or L2 data
- [x] Modified `onReceptionDelayElapsed()` to prefer L2, fallback to L1
- [x] Modified `recalculatePositionPnL()` to use L1 bid/ask when L2 unavailable
- [x] Updated `clear()` to reset quote snapshots
- [x] Fixed `TSClient::onInjectQuoteData()` to forward quotes to OrderEmulator

## Remaining Work

### Phase 4: Recording & Replay System Integration

**Key decisions made**:
- One separate LiveStreamDB instance for Quotes (matching Bars/Depth pattern)
- One DB file per day (`RecordedLiveData/Quotes/{YYYY-MM-DD}.db`)
- Store ALL object types: `QuoteStream`, `Heartbeat`, `Error` (heartbeats needed for replay)
- Add `objectType` field to ReplayDataPoint struct

- [x] **4.1: Add Quote recording storage directory**
  - Directory: `~/.cache/L2Trader/RecordedLiveData/Quotes/`
  - Follow same pattern as existing `Bars/` and `MarketDepthQuotes/` folders

- [x] **4.2: Extend LiveStreamDB for Quote streams**
  - Add `Quotes` to StreamType enum
  - Add `m_streamQuote` member (single stream for all symbols)
  - Add quote raw-data accumulator buffer
  - Add SQL queries in `LiveStreamDBQueries.h`:
    - CREATE_QUOTES_TABLE with `objectType` column
    - INSERT_QUOTE
  - Schema: id, stockTicker, epochMs, objectType, jsonRawData

- [x] **4.3: Connect recording to StreamQuote**
  - When starting recording with N symbols:
    - Open StreamBars per symbol (existing)
    - Open StreamMarketDepthQuote up to 10 (existing)
    - NEW: Open single StreamQuote with all N symbols
  - Connect via `receivedNewRawData` signal path
  - Handle stream errors/reconnection

- [x] **4.4: Add Quote playback to ReplayEngine**
  - Extend `ReplayDataLoader::DataType` with `Quote`
  - Add `objectType` to `ReplayDataPoint` struct
  - Add Quote queries in `ReplayDataQueries.h`
  - Add third replay path in ReplayEngine:
    - Quote loader, quote timer, injectQuoteData signal
  - Symbol filtering: replay all Heartbeat/Error, filter QuoteStream by open streams

- [x] **4.5: Update recorder/replay metadata surfaces**
  - Ensure Quotes folder is created, discovered, reported

### Phase 4B: Records Info Tab - Quote Display (DONE)

**Commit**: `feat(RecordsInfoTab): Add Quote stream display to Records Info tab (Phase 4B)`

Add Quote stream information to the Records Info tab for browsing recorded data.

- [x] **4B.1: Add Quote database discovery**
  - Add `getQuotesDbPath(QDate)` method
  - Scan `RecordedLiveData/Quotes/` directory alongside Bars/Depth
  - Check quotes database existence per date

- [x] **4B.2: Extend StockMetrics struct**
  - Add `hasQuotes`, `quoteCount`, `quoteFirstTimestampMs`, `quoteLastTimestampMs`
  - Add breakdown counts: `quoteStreamCount`, `heartbeatCount`, `errorCount`

- [x] **4B.3: Add Quotes column to stocks table**
  - Add 4th column: "Quotes" (Yes/No indicator)
  - Update table header and column setup

- [x] **4B.4: Add Quotes GroupBox to details panel**
  - Add "Quotes Data" GroupBox with:
    - Status label (Available/Not Available)
    - Total count, first/last timestamps, duration
    - Breakdown: "X QuoteStream, Y Heartbeat, Z Error"

- [x] **4B.5: Wire up database queries**
  - Query `quotes` table for selected symbol
  - Query by objectType: `SELECT objectType, COUNT(*) FROM quotes WHERE stockTicker = ? GROUP BY objectType`
  - Handle empty stockTicker for Heartbeat/Error (global events)

### Phase 5: UI - MarketFlags Display (DONE)

**Commit**: `feat(GUI): Add MarketFlags display labels for Level 1 Quote data`

- [x] **5.1: Add MarketFlags labels to main window**
  - Add QLabel members for IsHalted, IsDelayed, IsBats, IsHardToBorrow
  - Style: Red "HALTED", Yellow "DELAYED", Blue "BATS", Orange "HTB"
  - Initially hidden, show when flag is true

- [x] **5.2: Wire quote updates through MainAlgo -> FrontEnd**
  - Add TSClient::newQuoteReceived signal for parsed quotes
  - Add MainAlgo::displayedStockReceivedNewQuote signal
  - Connect through MainApp: TSClient -> MainAlgo -> FrontEnd
  - TUI: no-op implementation

### Phase 6: Market Depth Widget Integration (DONE)

**Commit**: `feat(MarketDepthTable): Add Level 1 display mode and data source indicator`

- [x] **6.1: Add Level 1 display mode to MarketDepthTable**
  - Add enum: `DisplayMode { Level2, Level1, NoData }`
  - When Level 1 mode: show only best bid/ask with "L1" name marker

- [x] **6.2: Auto-switch between Level 1 and Level 2**
  - Check `TSClient::hasOpenMarketDepthStream()` for current symbol
  - If no L2 stream: use L1 data from Quote stream

- [x] **6.3: Add visual indicator for data source**
  - L2/L1/-- indicator between BID and ASK headers
  - Color coding: Green L2, Yellow L1, Grey NoData

## Notes

- Level 2 data is always prioritized over Level 1 when both available
- Quote streams have no hard limit (unlike Level 2's 10-stream limit)
- MarketFlags are only available via Level 1 quotes, not Level 2 depth
