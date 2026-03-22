# Bundled Strategies

Bundled strategy executables and reference implementations for the L2Trader platform.

## Included samples

- `ExampleStrategyProcess/` — canonical out-of-process strategy executable sample built against `L2Trader::StrategySDK`
- `CrashTestStrategy/` — out-of-process crash-isolation sample
- `HistoricalBarsStrategy/` — out-of-process historical-bars sample
- `OrderTestStrategy/` — out-of-process order-lifecycle sample
- `DumpPatternStrategy/` — out-of-process cycle/state-machine sample that exercises host clock, chart logging, and order flow

## Building the out-of-process samples

From the repository root:

```bash
cmake --build build/GUI --target \
    ExampleStrategyProcess \
    CrashTestStrategyProcess \
    HistoricalBarsStrategyProcess \
    OrderTestStrategyProcess \
    DumpPatternStrategyProcess
cmake --build build/GUI --target stage-strategy-sdk
```

This produces:

- `build/GUI/bin/ExampleStrategyProcess`
- `build/GUI/bin/CrashTestStrategyProcess`
- `build/GUI/bin/HistoricalBarsStrategyProcess`
- `build/GUI/bin/OrderTestStrategyProcess`
- `build/GUI/bin/DumpPatternStrategyProcess`
- `build/GUI/strategy-sdk/` — an install-style SDK prefix with headers, `.proto` files, shared library artifacts, and CMake package metadata

The executable strategies also copy themselves into the local runtime directory used by the app:

- `~/.local/share/L2Trader/Strategies/`

Load a strategy by selecting its executable in the GUI. The platform runs `<executable> --describe-strategy` to discover the strategy name, version, and dynamic configuration schema before launch.
