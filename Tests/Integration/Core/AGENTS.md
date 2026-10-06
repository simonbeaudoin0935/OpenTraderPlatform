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

Temporary-SQLite BarCache public-API persistence/range tests are planned here
or in a dedicated Cache subfolder; they do not exist yet.
