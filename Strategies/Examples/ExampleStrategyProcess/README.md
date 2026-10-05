# ExampleStrategyProcess

`ExampleStrategyProcess` is the first out-of-process strategy sample for OpenTraderPlatform.

It demonstrates:

- loading runtime environment variables (`OPENTRADERPLATFORM_STRATEGY_ID`, `OPENTRADERPLATFORM_STRATEGY_SOCKET`)
- connecting to the host over a Unix domain socket
- sending the initial Protobuf handshake
- receiving host lifecycle and market-data envelopes
- emitting structured strategy logs back to the host

## Build

Inside the monorepo:

```bash
cmake --build build --target ExampleStrategyProcess
```

If you want an install-style SDK layout for external strategy projects:

```bash
cmake --build build --target stage-strategy-sdk
```

## Runtime contract

The executable expects:

- `OPENTRADERPLATFORM_STRATEGY_ID`
- `OPENTRADERPLATFORM_STRATEGY_SOCKET`

The host is expected to:

1. accept the outgoing handshake
2. send a `start` envelope
3. stream market/lifecycle envelopes
4. eventually send `stop` or `shutdown`

## Local install

The normal monorepo build also copies:

- `ExampleStrategyProcess` to `~/.local/share/OpenTraderPlatform/Strategies/`

The GUI loads the executable directly and queries `--describe-strategy` to render the configuration form.
