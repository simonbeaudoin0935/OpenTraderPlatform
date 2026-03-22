# ExampleStrategyProcess

`ExampleStrategyProcess` is the first out-of-process strategy sample for L2Trader.

It demonstrates:

- loading runtime environment variables (`L2TRADER_STRATEGY_ID`, `L2TRADER_STRATEGY_SOCKET`)
- connecting to the host over a Unix domain socket
- sending the initial Protobuf handshake
- receiving host lifecycle and market-data envelopes
- emitting structured strategy logs back to the host

## Build

Inside the monorepo:

```bash
cmake --build build/GUI --target ExampleStrategyProcess
```

If you want an install-style SDK layout for external strategy projects:

```bash
cmake --build build/GUI --target stage-strategy-sdk
```

## Runtime contract

The executable expects:

- `L2TRADER_STRATEGY_ID`
- `L2TRADER_STRATEGY_SOCKET`

The host is expected to:

1. accept the outgoing handshake
2. send a `start` envelope
3. stream market/lifecycle envelopes
4. eventually send `stop` or `shutdown`

## Local manifest install

The normal monorepo build also copies:

- `ExampleStrategyProcess` to `~/.local/share/L2Trader/Strategies/`
- `example_strategy.json` to `~/.config/L2Trader/Strategies/`

That manifest lets the GUI prefill the strategy configuration when you load the sample.
