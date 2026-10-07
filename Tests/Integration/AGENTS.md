# Integration suites

Inherit [root test rules](../AGENTS.md). Register targets/scenarios in
[CMakeLists.txt](CMakeLists.txt).

- [Clients](Clients/AGENTS.md): local stream/client lifecycle, `local-integration`.
- [Core](Core/AGENTS.md): launched platform startup, `local-integration`.
- [Strategy](Strategy/AGENTS.md): local SDK/host socket exchange, `local-integration`.
- [TradeStation](TradeStation/AGENTS.md): opt-in external Simulation API,
  `tradestation-api`, never interchangeable with local integration.

Additional keyring/simulated-YubiKey integration entries select functions from
the Unit/Security executable; see its guide. Local integration may require host
services (native keyring), even though it does not call the brokerage API.
Bound waits and process runtime; cleanup must work on partial initialization.
Use separate processes for process-wide Qt/backend state. Do not rely on CTest
serialization to protect another running application.
