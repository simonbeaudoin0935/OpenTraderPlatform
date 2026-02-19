# StreamQuote/ - Level 1 Quote Stream - Agent Instructions

**Location**: `Src/Clients/TSClient/MarketData/StreamQuote/`

## Overview

`StreamQuote` is a WebSocket stream that receives real-time Level 1 quote updates from the TradeStation "Stream Quotes" API endpoint. It subscribes to up to 100 symbols in a single stream, unlike the per-symbol Level 2 streams.

## Files

| File | Purpose |
|------|---------|
| `StreamQuote.h/cpp` | WebSocket stream class for Level 1 quotes |

## Class: StreamQuote

Inherits from `StreamMarketData` (which inherits from `Stream`).

### Interface

```cpp
class StreamQuote final : public StreamMarketData {
    Q_OBJECT
public:
    explicit StreamQuote(const QStringList& symbols, QNetworkReply* reply, QObject* parent = nullptr);
    ~StreamQuote();

    // Get all subscribed symbols
    QStringList getSymbols() const;

    // Global stream count
    static size_t getNumberOfQuoteStreams();

signals:
    void newQuoteReceived(Quote quote);  // Emitted for each parsed QuoteStream object

private:
    void processJsonObject(const QJsonObject& jsonObj) override;  // Parse incoming JSON

    QStringList m_symbols;
    QMap<QString, QJsonObject> m_symbolState;  // Accumulated per-symbol state for delta merging
    static std::atomic<size_t> s_numberOfQuoteStreams;
};
```

### ⚠️ Delta Stream Protocol (Critical Architecture Detail)

The TradeStation Quote Stream is **differential / incremental**. This is a fundamental property that affects all code touching quotes:

- **First message per symbol**: Full snapshot with **all** fields (Bid, Ask, Open, High, Low, Volume, MarketFlags, …)
- **Every subsequent message**: Delta patch — **only changed fields** are present. Missing fields must be inherited from the prior state.

**Example sequence from a real recording**:
```
Message 1: {"Symbol":"GERN","Bid":"1.82","Ask":"1.83","Open":"1.79","Volume":"6756902",...}  ← full
Message 2: {"Symbol":"GERN","AskSize":"21900","Volume":"6757155"}                            ← delta only
Message 3: {"Symbol":"GERN","Volume":"6757535"}                                              ← delta only
Message 4: {"Symbol":"GERN","AskSize":"22100"}                                               ← delta only
```

Parsing each message independently via `Quote::fromJson()` would yield `bid=0, ask=0` for messages 2–4 (because `parseDoubleField` defaults to `0.0` when the key is absent). This caused:
- Sporadic L1 display (zeroed bid/ask between real values)
- Position P&L oscillating between correct value and `-fullCost` in replay
- OrderEmulator clobbering good snapshots with zero prices

### Delta Merging in StreamQuote

`StreamQuote` maintains `QMap<QString, QJsonObject> m_symbolState` — one accumulated JSON object per symbol. In `processJsonObject()`:

```cpp
// Merge: insert/overwrite only the keys present in this delta
QJsonObject& state = m_symbolState[symbol];
for (auto it = jsonObj.constBegin(); it != jsonObj.constEnd(); ++it)
    state.insert(it.key(), it.value());

// Parse the merged (always-complete) state
Quote quote(state);
emit newQuoteReceived(quote);  // always emitted, even volume-only deltas
```

The "always emit" design keeps MarketFlags display live and simplifies downstream consumers — they don't need to know which fields changed.

Non-symbol messages (Heartbeat, Error) have no `"Symbol"` key and bypass the accumulator entirely.



**QuoteStream** (actual quote data):
```json
{
    "Symbol": "AAPL",
    "Bid": 227.50,
    "Ask": 227.55,
    "BidSize": 10,
    "AskSize": 20,
    "MarketFlags": { "IsHalted": false, "IsDelayed": false, "IsHardToBorrow": false },
    "Last": 227.52,
    "Open": 226.10,
    "High52Week": 237.23,
    "Low52Week": 164.08
}
```

**Heartbeat** (keep-alive):
```json
{ "Heartbeat": 1739875200000 }
```

**Error**:
```json
{ "Error": "...", "Message": "..." }
```

### Signal Flow

```
TSClient::openStreamQuote(symbols)
    │
    ▼
StreamQuote created (one stream for all symbols)
    │
    │ WebSocket data arrives (newline-delimited JSON)
    ▼
StreamMarketData::onRawDataReceived()
    │  Accumulates bytes, splits on newlines
    ▼
StreamQuote::processJsonObject()
    │  Parses JSON, creates Quote object
    ▼
emit newQuoteReceived(quote)
    │
    ├──► TSClient::newQuoteReceived(symbol, quote)  [re-emitted for routing]
    ├──► RecordDB recording (raw data via receivedNewRawData)
    └──► OrderEmulator::updateQuote() (replay mode)
```

### Concurrency

- **Single stream**: All symbols (up to 100) use **one** StreamQuote instance
- **Constant**: `QuoteConstants::MAX_SYMBOLS_PER_STREAM = 100`
- **No hard limit**: Unlike Level 2 (max 10 streams), only 1 StreamQuote needed

### Opening the Stream

```cpp
// Open via TSClient (handles both live and replay modes)
QPointer<StreamQuote> stream = TSClient::getInstance()->openStreamQuote(symbols);

connect(stream, &StreamQuote::newQuoteReceived, this, [](Quote quote) {
    qDebug() << "Quote for" << quote.getSymbol()
             << "Bid:" << quote.getBid()
             << "Ask:" << quote.getAsk()
             << "Halted:" << quote.getMarketFlags().isHalted();
});
```

### Replay Mode

In replay mode, `openStreamQuote()` creates a `MockNetworkReply` instead of a real WebSocket. Data is injected via:

```cpp
TSClient::onInjectQuoteData(symbol, rawDeltaBytes)
    │
    ├── Merge delta into m_replayQuoteState[symbol] → build complete Quote → OrderEmulator::updateQuote()
    └── MockNetworkReply::injectData(rawDeltaBytes) → StreamQuote parser → m_symbolState merge → newQuoteReceived
```

**Important**: Raw delta bytes are stored and replayed as-is (no merging at record/playback time). Merging happens at parse time in two independent layers:
1. `StreamQuote::m_symbolState` — for the GUI path (displayed symbol's MockNetworkReply → StreamQuote)
2. `TSClient::m_replayQuoteState` — for the OrderEmulator path (all symbols)

### Recording

During live recording (`LiveStreamDB::Quotes` stream type), the raw JSON bytes are stored as-is with:
- `stockTicker`: symbol (empty for Heartbeat/Error)
- `epochMs`: wall-clock timestamp
- `objectType`: "QuoteStream", "Heartbeat", or "Error"

Heartbeats are stored because they are needed for accurate replay timing.

## Related Components

- **`GetQuoteSnapshots/Quote.h`**: Data structure emitted by `newQuoteReceived`
- **`GetQuoteSnapshots/MarketFlags.h`**: Market status flags within Quote
- **`TSClient::openStreamQuote()`**: Factory method in `TSClientStreams.cpp`
- **`TSClient::onInjectQuoteData()`**: Replay injection in `TSClientStreams.cpp`
- **`LiveStreamDB`**: Records this stream to `RecordedLiveData/Quotes/{date}.db`
- **`ReplayEngine`**: Replays stored quotes via `m_quoteTimer`
