# Algorithm unit scenarios

[BarAggregatorTests.cpp](BarAggregatorTests.cpp) feeds the public BarAggregator
slots and observes `barUpdated`/`barClosed` signals with QSignalSpy.

Coverage: OHLCV across 5m/15m/30m/1h/4h intervals; no early close; Open update
versus Closed completion; accumulator reset; daily end, Friday weekly end,
weekend-aware month end and year end; tick-price updates without counting tick
volume twice; ignored placeholders and non-Open tick updates.

Use a fresh aggregator per scenario and fixed market-timezone bars. Check
payloads as well as emission counts. Do not treat holiday handling, missing
session-end bars, late/duplicate bars, or partial-start alignment as tested
contracts: these require reviewing intended behavior before adding assertions.
The historical/live parity fixture is in the MarketData suite.

[BarTapeReconstructorTests.cpp](BarTapeReconstructorTests.cpp) drives
`BarTapeReconstructor::onBar` directly with fixed 1m bars and a BBO. Coverage:
first update is baseline only; volume delta becomes one print priced at the
close; unchanged/shrinking volume and older-minute bars are ignored; Closed
update prints the remaining volume; new minute prints the whole bar volume;
`reset()` restores baseline behaviour; bid/ask side classification.
