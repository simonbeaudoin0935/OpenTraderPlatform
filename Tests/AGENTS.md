# Test architecture and contributor guide

Read the repository [AGENTS.md](../AGENTS.md) first. This tree uses Qt Test
executables registered with CTest; it is not a second application runtime.
Tests are built when `BUILD_TESTS=ON`.

## Navigation

- [Unit](Unit/AGENTS.md): deterministic logic and isolated component behavior.
- [Integration](Integration/AGENTS.md): local subsystem/process interactions
  and separately gated external API checks.
- [User-facing testing guide](../Doc/TESTING.md): tasks and API prerequisites.

## Running and extending

The VS Code `configure-test` task configures `build/test`; `build-test` compiles
it. `build-then-run-tests` builds and runs routine tests.
`run-unit-tests` and `run-local-integration-tests` build before selecting their
CTest labels. Inspect actual CTest output to confirm selection; never infer
success from task creation or configuration alone.

Register new sources explicitly in the relevant suite's `CMakeLists.txt`, with
`add_executable`, required dependencies, `add_test`, and an appropriate label.
Keep stable test/executable names and use data-driven rows for behavior matrices.
Prefer Qt Core-only test mains unless widgets are necessary.

Use fixed America/New_York timestamps for market fixtures. Expected values
must be independent of the production helper under test. Assert public results,
signal payloads, counts, statuses, ranges, and error outcomes, not log text alone.
Do not expose private members or duplicate production logic to make tests pass.
Format changed C++ sources and build/run the smallest relevant suites first.

## Isolation and failures

- Use temporary directories/databases and test-specific Qt organization/application
  identities. Never clear real app settings, caches, ledgers, or credential stores.
- Restore changed process globals/environment where needed. Track initialized
  resources so cleanup remains valid after early fixture failure.
- Use bounded signal/future waits rather than arbitrary sleeps.
- Distinguish setup/environment failures from failed product assertions.
  Keep explicit error checks; do not suppress a status just to turn a test green.
- Treat keyring/hardware tests as integration tests even when they share a source
  executable with unit cases.
- Never print credentials, authorization headers, or raw OAuth responses.

## External services

`tradestation-api` is distinct from `unit` and `local-integration`. API executables
must exit with the registered skip code before credential access without explicit
opt-in. Simulation API is a real network service, not local replay.
Read [TradeStation guidance](Integration/TradeStation/AGENTS.md) before modifying
these tests. Compilation, skipped execution, and successful API execution are
different validation outcomes and must be reported separately.

## Expansion roadmap (not existing coverage)

Temporary-SQLite BarCache persistence/range and SDK socket fragmentation tests
now exist under Integration/Core and Integration/Strategy, including warmup
merge protection against older Open/Null history. `RiskManager` evaluation,
drawdown/cooldown lifecycle, and SQLite runtime-state persistence are now
covered under Integration/Algo. Actual provider backfill
overlap/coalescing, full host child-process transport, provider error/retry
integration, `OrderEmulator` replay fill/position emulation, `MainAlgo`'s
risk-gate wiring (`processPlaceOrder`), and the GUI `RiskStatusWidget`/strategy
user-confirm preview-bracket flow remain to be implemented. Read-only API and
separately gated paper-order lifecycle scenarios are compiled but
authenticated execution has not been verified. Keep order opt-in, bounded
cleanup, exact account checks, and cleanup restricted to IDs returned by the
test's placement.
