# BarAggregator — Real-Time Multi-Timescale Bar Aggregation

## Purpose

`BarAggregator` converts the stream of closed 1-minute bars (from `LiveBarAccumulator`) into
higher-timescale OHLCV bars (5m, 15m, 30m, 1h, 4h, 1d, 1w, 1M) in real-time.

One `BarAggregator` instance lives inside each `SymbolContext` on the **MainAlgo thread**.

## Files

- `BarAggregator.h` — Class declaration, `Accumulator` inner struct
- `BarAggregator.cpp` — `onNewBar` dispatch + per-TF close detection

## API

```cpp
class BarAggregator : public QObject {
    Q_OBJECT
public:
    explicit BarAggregator(QObject* parent = nullptr);

public slots:
    /**
     * Thread context: called from MainAlgo thread via direct connection
     * Feed every closed 1m bar here.
     */
    void onNewBar(const QString& symbol, const Bar& bar);

signals:
    /**
     * Thread context: emitted from MainAlgo thread
     * Fired after every 1m bar is accumulated (in-progress composite bar).
     */
    void barUpdated(TimeFrame tf, const QString& symbol, const Bar& bar);

    /**
     * Thread context: emitted from MainAlgo thread
     * Fired when a composite bar is complete and ready to be stored.
     */
    void barClosed(TimeFrame tf, const QString& symbol, const Bar& bar);
};
```

## Closing Rules per TimeFrame

| TimeFrame | Close condition |
|-----------|----------------|
| 5m, 15m, 30m, 1h | `(minutesFromOpen + 1) % tfMinutes == 0` |
| 4h | Same formula with `tfMinutes = 240` |
| 1d | Bar timestamp == `TIME_LAST_CANDLE_AFTER_MARKET_SESSION` |
| 1w | Friday && timestamp == `TIME_LAST_CANDLE_AFTER_MARKET_SESSION` |
| 1M | Next trading day falls in a different calendar month |

`minutesFromOpen` is `(hour - 4) * 60 + minute` where hour is the America/New_York hour of the 1m bar.

## Wiring in SymbolContext

```cpp
// In SymbolContext constructor:
connect(&barReceiver.m_liveBarAccumulator,
        &LiveBarAccumulator::barClosed,
        &m_barAggregator,
        &BarAggregator::onNewBar,
        Qt::DirectConnection);

connect(&m_barAggregator,
        &BarAggregator::barClosed,
        &barCache,
        [this](TimeFrame tf, const QString& sym, const Bar& bar) {
            barCache.storeBar(tf, sym, bar);
        },
        Qt::DirectConnection);
```

## Internal Structure

```cpp
struct Accumulator {
    bool active = false;
    QDateTime openTime;
    double open = 0, high = 0, low = 0, close = 0;
    int64_t volume = 0;

    void start(const Bar& bar);
    void update(const Bar& bar);   // std::max/min with static_cast<double> for float getters
    Bar toBar() const;
};

// One accumulator per non-1m TimeFrame:
QMap<TimeFrame, Accumulator> m_accumulators;
```

## Design Notes

- Only accumulates the 7 non-1m timescales (ONE_MINUTE bars pass through untouched — stored
  directly by the existing `BarReceiver` → `BarCache::storeBar(ONE_MINUTE)` path).
- Does not store bars itself — all storage goes through `BarCache::storeBar`.
- Historical bars at higher timescales are fetched by `DBClient::fetchHistoricalBars` with
  in-app aggregation from native Databento schemas — `BarAggregator` only handles live data.

## Unit Tests

`Tests/Unit/Algo/BarAggregatorTests.cpp` exercises the public slots and observes their
signals without starting application singletons or contacting a data provider:
- Intraday OHLCV aggregation and closing boundaries for 5m, 15m, 30m, 1h, and 4h
- Accumulator reset after a completed period
- Daily, Friday weekly, and weekend-aware monthly/year-end closing boundaries
- Live price updates with volume committed only by closed minute bars
- Ignoring placeholders and non-open tick updates

The CTest entry is `BarAggregatorTests`, labeled `unit`, and is included in the
`build-then-run-tests` and `run-unit-tests` VS Code tasks.
