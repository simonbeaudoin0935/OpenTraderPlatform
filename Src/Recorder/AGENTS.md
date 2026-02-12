# Recorder Executable - Agent Instructions

## Overview

The Recorder is a **standalone headless executable** that captures live market data streams from TradeStation and stores them in SQLite databases. This recorded data can later be replayed in the main L2Trader application for strategy backtesting and debugging.

## Quick Start

```bash
# Build the recorder
cmake --build ./build/GUI -j$(nproc)

# Run with a stock list CSV
./build/GUI/Src/Recorder/Recorder --stocks /path/to/stocks.csv

# Optional: specify custom output directory
./build/GUI/Src/Recorder/Recorder --stocks stocks.csv --output /data/recordings
```

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      Recorder Executable                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   main.cpp                                                       │
│       │                                                          │
│       ├── Parse arguments (--stocks, --output)                   │
│       ├── Load stock tickers from CSV                            │
│       ├── Initialize TSClient                                    │
│       │                                                          │
│       ├── Create LiveStreamDB (Bars)                             │
│       │       │                                                  │
│       │       └── Opens: {date}.db                            │
│       │                                                          │
│       ├── Create LiveStreamDB (MarketDepthQuotes)                │
│       │       │                                                  │
│       │       └── Opens: {date}.db                               │
│       │                                                          │
│       ├── Start StatusReporter (10s interval)                    │
│       │                                                          │
│       └── Run Qt event loop                                      │
│                                                                  │
│   SIGINT Handler                                                 │
│       │                                                          │
│       ├── Print error statistics                                 │
│       ├── Print timeout recovery stats                           │
│       └── Graceful shutdown                                      │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## File Structure

| File | Purpose |
|------|---------|
| `main.cpp` | Entry point. Sets up TSClient, databases, signal handlers, status reporter |
| `LiveStreamDB.h/cpp` | Core recording logic. Opens streams, receives data, writes to SQLite |
| `StatusReporter.h/cpp` | Periodic console output with recording statistics |
| `RecorderUtils.h/cpp` | Utilities: load CSV, create folder structure |
| `RecorderLogic.h/cpp` | (Currently minimal) Additional recording logic |

## Key Components

### LiveStreamDB

The main recording class. One instance per data type (Bars or MarketDepthQuotes).

**Responsibilities:**
- Opens SQLite database file for the current date
- Creates streams for each stock symbol via TSClient with ramped startup
- Receives raw JSON data from streams
- Stores data with timestamp to database
- Tracks and attempts recovery from stream timeouts
- Tracks error statistics per symbol

**Stream Ramp-Up Mechanism:**

When recording large numbers of symbols (e.g., 100+ stocks), opening all streams simultaneously can overwhelm the TradeStation API servers with rate limiting. To prevent this, we use a **graduated ramp-up strategy** that slows down progressively:

**Adaptive Delay Strategy:**
- **Streams 1-100**: 500ms delay (2 streams/second)
- **Streams 101-200**: 1000ms delay (1 stream/second)  
- **Streams 201+**: 2000ms delay (0.5 streams/second)

**Why Graduated?**
The TradeStation API has cumulative rate limits that become stricter as more concurrent streams are opened. Starting fast and slowing down allows:
- Quick startup for small recordings (50 stocks in ~25 seconds)
- Reliable scaling to large recordings (250 stocks without errors)
- Automatic adaptation based on stream count

**Behavior:**
- First stream opens immediately
- Subsequent streams open with adaptive delay
- Logs tier transitions: "Reached 100 streams - slowing down to 1000ms delay"
- Ramp-up can be interrupted by calling `stopRecording()`
- Progress logged for each stream: "Opened bar stream for AAPL (1/250)"

**Testing Results:**
- ✅ 50 simultaneous streams: Works
- ❌ 100 simultaneous streams: API rate limiting errors
- ✅ 100 streams with 500ms ramp-up: Works
- ❌ 200+ streams with 500ms ramp-up: API rate limiting at higher counts
- ✅ 250+ streams with graduated ramp-up: Works reliably

**Key Methods:**
```cpp
// Constructor - opens DB and creates table
LiveStreamDB(StreamType type, const QString& dbPath, QStringList& stockTickers);

// Start recording all configured stocks (uses ramping)
void startRecording();

// Stop recording and cancel any pending ramp-up
void stopRecording();

// Internal: Opens next stream in the ramp-up sequence
void openNextStream();

// Store a single data point
bool storeData(const QString& stock, qint64 timestamp, const QByteArray& rawData);

// Attempt to restart a timed-out stream
void attemptStreamRecovery(const QString& symbol);
```

### StatusReporter

Prints periodic status to console every 10 seconds.

**Output includes:**
- Uptime
- Active stream counts
- Total records written
- Error counts by symbol and type
- Timeout recovery statistics
- Memory usage

### RecorderUtils

Utility functions:

```cpp
// Load stock symbols from CSV file (first column)
QStringList loadStockTickers(const QString& csvFilePath);

// Create folder structure for recordings
QString createRecordingFolders(const QString& cacheLocation);
```

## Database Schema

### Bars Table

```sql
CREATE TABLE IF NOT EXISTS bars (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol TEXT NOT NULL,
    sequence INTEGER NOT NULL,
    epoch_ms INTEGER NOT NULL,
    json_data BLOB NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_bars_symbol_epoch ON bars(symbol, epoch_ms);
```

### Market Depth Quotes Table

```sql
CREATE TABLE IF NOT EXISTS market_depth_quotes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol TEXT NOT NULL,
    sequence INTEGER NOT NULL,
    epoch_ms INTEGER NOT NULL,
    json_data BLOB NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_depth_symbol_epoch ON market_depth_quotes(symbol, epoch_ms);
```

## Output File Locations

Default location: `~/.cache/L2Trader/RecordedLiveData/`

```
RecordedLiveData/
├── Bars/
│   ├── 2026-02-05.db
│   ├── 2026-02-06.db
│   └── ...
└── MarketDepthQuotes/
    ├── 2026-02-05.db
    ├── 2026-02-06.db
    └── ...
```

## Stock CSV Format

Simple CSV with stock symbols in the first column:

```csv
Symbol,Name,Exchange
AAPL,Apple Inc,NASDAQ
TSLA,Tesla Inc,NASDAQ
MSFT,Microsoft Corporation,NASDAQ
NVDA,NVIDIA Corporation,NASDAQ
```

Only the first column is used; other columns are optional metadata.

## Command Line Arguments

| Argument | Description | Required |
|----------|-------------|----------|
| `--stocks <file>` | Path to CSV file with stock symbols | Yes |
| `--output <dir>` | Custom output directory for recordings | No |

## Error Handling

### Stream Timeouts

When a stream times out (no data for 10 seconds):
1. Error is logged
2. Timeout is tracked in `unrecoveredTimeouts` map
3. Recovery can be attempted via `attemptStreamRecovery()`
4. If data resumes, timeout is moved to `recoveredTimeouts`

### Graceful Shutdown

SIGINT (Ctrl+C) triggers:
1. Finalize unrecovered timeouts
2. Print comprehensive error statistics
3. Print timeout recovery statistics
4. Call `QCoreApplication::quit()`

## Data Flow

```
TSClient
    │
    ├── openStreamBars(symbol, 1, Minute, 0, USEQ24Hour)
    │       │
    │       └── StreamBars
    │               │
    │               │ receivedNewRawData signal
    │               ▼
    │       LiveStreamDB::onReceivedNewRawDataForStock()
    │               │
    │               ├── Get current epoch timestamp
    │               ├── Check for timeout recovery
    │               └── storeData() → SQLite INSERT
    │
    └── openStreamMarketDepthQuote(symbol, depth=10)
            │
            └── StreamMarketDepthQuote
                    │
                    │ receivedNewRawData signal
                    ▼
            LiveStreamDB::onReceivedNewRawDataForStock()
                    │
                    └── storeData() → SQLite INSERT
```

## Threading Model

The Recorder runs entirely on the **main thread**:
- TSClient singleton handles HTTP/streaming
- Qt event loop processes signals
- SQLite writes are synchronous (blocking)

This is intentional for simplicity - recording is I/O bound, not CPU bound.

## Typical Recording Session

1. Start recorder before market open
2. Let it run throughout trading session
3. Monitor status output for errors
4. Press Ctrl+C after market close
5. Review error statistics
6. Database files are ready for replay

## Integration with Replay

The databases created by the Recorder are directly consumed by:
- `ReplayDataLoader` (in `Src/Core/Replay/`)
- Queries data chronologically by timestamp
- Merges bars and depth quotes into single timeline

See `Src/Core/Replay/AGENT.md` for replay system documentation.

## Common Issues

### No Data Being Recorded
- Check TSClient authentication (credentials in SecureStorage)
- Verify stock symbols are valid
- Check if market is open (streams only active during trading hours)

### Database Errors
- Ensure write permissions to output directory
- Check disk space
- SQLite file may be locked by another process

### Stream Timeouts
- Network connectivity issues
- TradeStation API rate limits
- Symbol may be halted or delisted

## Future Improvements (TODOs in code)

1. Automatic stream recovery on timeout
2. Better error classification and handling
3. Configurable heartbeat timeout
4. Multi-day recording (auto-rotate at midnight)
5. Compression of stored JSON data
