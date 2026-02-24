# GetQuoteSnapshots/ - Quote Snapshot & MarketFlags - Agent Instructions

**Location**: `Src/Clients/TSClient/MarketData/GetQuoteSnapshots/`

This directory contains the data structures for Level 1 Quote data: the `Quote` class (enhanced with all QuoteStream API fields) and the `MarketFlags` class.

## Files

| File | Purpose |
|------|---------|
| `Quote.h/cpp` | Level 1 quote data structure (bid, ask, sizes, flags, 52-week data, futures fields) |
| `MarketFlags.h/cpp` | Market status flags (IsHalted, IsDelayed, IsBats, IsHardToBorrow) |

---

## MarketFlags

**Location**: `MarketFlags.h/cpp`

Represents the four boolean market status flags present in TradeStation QuoteStream data.

### Fields

| Flag | Accessor | Meaning |
|------|----------|---------|
| IsBats | `isBats()` | Quote is sourced from BATS exchange |
| IsDelayed | `isDelayed()` | Data is delayed (not real-time) |
| IsHalted | `isHalted()` | Trading is halted for this symbol |
| IsHardToBorrow | `isHardToBorrow()` | Short selling may be restricted |

### Interface

```cpp
class MarketFlags {
public:
    MarketFlags() = default;
    explicit MarketFlags(const QJsonObject& p_json);  // Parse from API response

    bool isBats() const;
    bool isDelayed() const;
    bool isHalted() const;
    bool isHardToBorrow() const;

    bool isValid() const;              // True if at least one flag parsed
    QJsonObject toJson() const;
    QString toJsonString() const;
    void fromJson(const QJsonObject& p_json);
};
Q_DECLARE_METATYPE(MarketFlags)
```

### JSON Format

```json
{
    "IsBats": false,
    "IsDelayed": false,
    "IsHalted": false,
    "IsHardToBorrow": true
}
```

---

## Quote (enhanced)

**Location**: `Quote.h/cpp`

The `Quote` class represents a full Level 1 quote from the TradeStation streaming API. It was significantly enhanced to support all QuoteStream fields including MarketFlags integration.

### Key Fields

```cpp
class Quote {
public:
    // Core bid/ask
    double getBid() const;
    double getAsk() const;
    int getBidSize() const;   // In lots
    int getAskSize() const;

    // Symbol
    QString getSymbol() const;

    // Market status flags
    const MarketFlags& getMarketFlags() const;

    // 52-week data
    double get52WeekHigh() const;
    double get52WeekLow() const;
    QDateTime get52WeekHighDate() const;
    QDateTime get52WeekLowDate() const;

    // Daily price data
    double getOpen() const;
    double getHigh() const;
    double getLow() const;
    double getClose() const;   // Last price
    double getPreviousClose() const;
    double getNetChange() const;
    double getNetChangePct() const;

    // Futures-specific
    double getMinPrice() const;
    double getMaxPrice() const;
    QDate getFirstNoticeDate() const;
    QDate getLastTradingDate() const;
    int getDailyOpenInterest() const;

    // Restrictions and tiers
    QStringList getRestrictions() const;
    int getTickSizeTier() const;

    // Validation
    bool isValid() const;
};
```

### JSON Parsing

Quotes are parsed from TradeStation "StreamQuote" format:

```json
{
    "Symbol": "AAPL",
    "Bid": 227.50,
    "Ask": 227.55,
    "BidSize": 10,
    "AskSize": 20,
    "MarketFlags": {
        "IsBats": false,
        "IsDelayed": false,
        "IsHalted": false,
        "IsHardToBorrow": false
    },
    "High52Week": 237.23,
    "Low52Week": 164.08,
    "Open": 226.10,
    "Last": 227.52,
    "NetChange": 1.42,
    "NetChangePct": 0.628
}
```

### Usage

```cpp
// Receive from StreamQuote signal
connect(stream, &StreamQuote::newQuoteReceived, this, [](Quote quote) {
    if (quote.isValid()) {
        double spread = quote.getAsk() - quote.getBid();
        bool isHalted = quote.getMarketFlags().isHalted();
    }
});

// Use in OrderEmulator for L1 fill price
double fillPrice = isBuyOrder ? quote.getAsk() : quote.getBid();
```

## Related Components

- **StreamQuote**: Streams and emits Quote objects (see `../StreamQuote/`)
- **OrderEmulator**: Uses Quote bid/ask for fills when Level 2 unavailable
- **GUIFrontend**: Reads MarketFlags to show HALTED/DELAYED/HTB labels
- **MarketDepthTable**: Uses Quote bid/ask in Level 1 display mode
