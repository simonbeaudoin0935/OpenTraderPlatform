# Strategy Projects

This repository includes public SDK examples and test harnesses. Proprietary
strategies and their support tooling are maintained separately and are not
included in the public repository.

## Examples

- [`Examples/ExampleStrategyProcess/`](Examples/ExampleStrategyProcess/) — minimal out-of-process strategy using the public SDK.
- [`Examples/HistoricalBarsStrategy/`](Examples/HistoricalBarsStrategy/) — demonstrates historical-bar requests. It requests data repeatedly while running; use it deliberately.

Build the examples from the repository root:

```bash
cmake --build build --target ExampleStrategyProcess HistoricalBarsStrategyProcess --parallel
```

The executables are written to `build/bin/` and staged under
`~/.local/share/OpenTraderPlatform/Strategies/`. The GUI loads an executable
directly and invokes `--describe-strategy` to read its name, version, and
configuration schema.

## Test Harnesses

Crash and order-driving harnesses are excluded from normal builds and packages.
Enable them only when intentionally testing process isolation or order flows:

```bash
cmake -S . -B build -DBUILD_STRATEGY_TEST_HARNESSES=ON
cmake --build build --target CrashTestStrategyProcess OrderTestStrategyProcess --parallel
```

`CrashTestStrategyProcess` intentionally crashes. `OrderTestStrategyProcess`
submits orders using its configured account; do not run it against a live account.

## Replay Lifecycle

- Replay preload is not tradable. Before the first Play press, replay strategies may be primed in the host, but their child processes do not start.
- On the first Play press, the host starts primed replay strategies, waits for `onStart()` to finish, and expects the SDK runtime's ready acknowledgement.
- The host reports an unsupported replay-ready protocol version instead of silently falling back.
- After replay starts, replay pause/resume maps to strategy pause/resume.
