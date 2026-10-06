# Unit suites

Inherit [root test rules](../AGENTS.md). Register suites in
[CMakeLists.txt](CMakeLists.txt), using label `unit`.

## Domains

- [Algo](Algo/AGENTS.md): live higher-timeframe aggregation.
- [MarketData](MarketData/AGENTS.md): bar arithmetic and trade accumulation.
- [Core/BarCache](Core/BarCache/AGENTS.md): restart backfill policies.
- [Clients](Clients/AGENTS.md): authentication request gating with local peers.
- [FrontEnd](FrontEnd/AGENTS.md): offscreen indicator behavior.
- [Security](Security/AGENTS.md): token validation and vault algorithms; the
  same executable also contains separately registered integration cases.
- [Strategy](Strategy/AGENTS.md): SDK framing primitives.

Some component suites link `OpenTraderPlatformCommon` for production code reuse,
but must not start its singletons or authenticate simply because they link it.
Keep assertions deterministic and use the lightest dependencies possible.
