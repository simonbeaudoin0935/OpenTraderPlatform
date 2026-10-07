# Platform startup integration

[StartupRestoreTests.cpp](StartupRestoreTests.cpp) launches the actual platform
offscreen with isolated XDG config/state/data/cache and a fake touch-enabled
`ykman`. It holds unlock, checks restoration has not run during the nested event
loop, releases unlock, and checks restoration then occurs.

The target depends on the platform binary and is Linux/Unix gated in CMake.
It terminates only its owned QProcess on failure. Never launch against user
settings or real hardware. The native preference currently uses an empty
organization to match the platform's settings namespace; the fixture verifies
saved content by read-back rather than solely checking QSettings status.
Investigate that namespace/status behavior before changing product settings.

[BarCachePersistenceTests.cpp](BarCachePersistenceTests.cpp) uses public BarCache
and DatabaseThread APIs with temporary SQLite files. It covers full-day reload,
inclusive memory/database ranges, last-session candle, symbol/date/timeframe
isolation, duplicate replacement, latest closed timestamps, transient forming
bars, and incomplete database ranges. It redirects both Qt settings formats and
cacheRootDir; a locked isolated YubiKey backend prevents host-keyring access.
The TSClient thread runs only for cache validation dependencies with no tokens.
Drain queued cache closes before destroying DatabaseThread.

Current-day warmup overlap also seeds an older Open/Null historical snapshot
after a live closed bar and checks that the public warmup merge preserves its
closed timestamp. Because live merge is gated by the real application date,
these rows skip on weekends or before 04:03 New York time; all other rows use
fixed dates. This does not exercise an actual provider request or verify OHLC
through a subsequent current-day chart fetch.

Database completeness tests cover the existing count-based policy at 80%, 90%,
and 100%. Accepted 90% coverage returns only the stored rows; it does not prove
contiguous coverage or synthesize the missing tail.
Request coalescing and provider backfill/error/retry paths remain uncovered;
no fixture should silently fall through to a real provider.
