# Market-data unit scenarios

- [BarUtilsTests.cpp](BarUtilsTests.cpp): explicit slot counts and all-index
  round trips across supported timeframes; native/source timeframe selection;
  intraday/day/week/month timestamp alignment; sparse OHLCV aggregation skipping
  placeholders; historical/live aggregation parity for two complete periods.
- [LiveBarAccumulatorTests.cpp](LiveBarAccumulatorTests.cpp): 10s/60s boundary
  flooring including UTC and millisecond inputs; OHLCV updates and close/reset;
  independent symbols; gaps without invented trades; next-session rollover.

Fixtures are chronologically ordered and valid. Out-of-order trades and invalid
sizes/intervals are not yet covered; establish intended rejection behavior
before encoding it. Historical/live parity currently starts at a canonical
04:00 boundary with complete periods, not an arbitrary mid-period startup.
Stream reception and provider schemas belong to integration tests.

Regression: `barIndex` must accept final-minute sub-minute slots through
19:59:59, not stop at the minute candle's 19:59:00 open time. The all-index
round trip originally exposed that production precondition mismatch.
