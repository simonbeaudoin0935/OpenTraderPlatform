# RiskManager integration coverage

[RiskManagerTests.cpp](RiskManagerTests.cpp) exercises `RiskManager` end-to-end
through its public API only: config clamping, `evaluateOrder` rejection
reasons (missing reference/stop price, max position shares/notional, max
planned loss per trade, max daily entries, max open positions, drawdown lock,
cooldown), reduce-only orders bypassing checks, equity-peak drawdown lock
derivation from `onBalanceUpdate`, entry-order fill de-duplication,
cooldown start/expiry from `onPositionClosed` (including the deferred case
when realized PnL is not yet reconciled), `resetRiskDay` vs `unlockTrading`
semantics (counters cleared vs preserved), status-snapshot drawdown math/text,
and runtime-state persistence across separate `RiskManager` instances backed
by the same SQLite ledger.

It redirects both Qt settings formats and `cacheRootDir` to a temporary
directory and forces `TradingMode::Sim` so the ledger lands under an isolated
`Simulation` subdirectory. Critically, `LedgerPaths::ledgersRootDir()` resolves
through `getDataLocation()`, which is backed by
`QStandardPaths::AppDataLocation` (not `cacheRootDir`) — so `XDG_DATA_HOME` is
also redirected into the same temporary directory in `initTestCase()` (and
restored in `cleanupTestCase()`). Without this, the SQLite ledger would be
created under the real `~/.local/share/<AppName>/Ledgers/...` path and leak
state across separate test runs. It never touches real app settings or
ledgers. Each scenario uses a distinct account ID so state persisted through
the shared `QSettings` instance and SQLite ledger file does not leak between
test slots. The persistence-round-trip case scopes the first `RiskManager`
so its `Store` closes its SQLite connection before a second instance opens
the same connection name.

Config-snapshot audit rows and risk-event logging are exercised incidentally
through the public calls above; their SQLite content is not asserted
directly. `MainAlgo` wiring, GUI `RiskStatusWidget` rendering, and the
strategy user-confirm preview-bracket flow remain uncovered here — see
[Doc/RISK_MANAGEMENT.md](../../../Doc/RISK_MANAGEMENT.md) and
[Src/Algo/AGENTS.md](../../../Src/Algo/AGENTS.md).
