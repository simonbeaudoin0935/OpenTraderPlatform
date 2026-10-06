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

Live/history overlap, request coalescing, and provider error/retry paths remain
uncovered; no fixture should silently fall through to a real provider.
