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
    static std::atomic<size_t> s_numberOfQuoteStreams;
};
```

### Stream Data Format

The TradeStation API sends newline-delimited JSON objects of three types:

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
TSClient::onInjectQuoteData(symbol, rawJsonData)
    │
    ├── Parse to Quote → emit newQuoteReceived (for OrderEmulator)
    └── MockNetworkReply::injectData() (for StreamQuote parser)
```

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
