# Restart backfill policy

[BarHistoryBackfillTests.cpp](BarHistoryBackfillTests.cpp) directly tests the
header-only `BarHistoryBackfill::start` and `shouldReplace` policies.

Scenarios: empty/stale/fresh snapshots, captured restart anchor despite later
live arrival, forming candles not proving coverage, verified no-trade tails,
pre-session no-fetch, unsorted snapshots selecting the latest closed candle,
all 16 existing/incoming status combinations, and a fixture merge restoring
missing bars while retaining closed data.

This is NOT an end-to-end BarCache test: the fixture merge loop does not exercise
BarCache's private merge, request coalescing, fetching, or persistence.
Add those through public APIs and temporary SQLite integration fixtures.
Keep null placeholders distinct from uninitialized (unknown) coverage.
