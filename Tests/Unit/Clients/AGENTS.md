# Client component unit tests

[TradeStationAuthenticationGateTests.cpp](TradeStationAuthenticationGateTests.cpp)
uses a loopback TCP server and AuthenticatedNetworkAccessManager.
It checks that denied GET/POST/DELETE requests never reach the server and that
requests resume, then stop again, as the authorization predicate changes.
It also covers the stalled-connection watchdog with a silent loopback server
(shortened via `setWatchdogTimeouts`): silent streams report a stall without
being aborted, silent REST requests report and abort, answered requests stay
quiet, and a retired manager deletes itself after its last reply.

This is local network I/O, not a TradeStation request. Do not use production
credentials or external addresses here. Assert reply errors/body and server
request counts; destroy replies through Qt deferred deletion.
Actual provider connectivity belongs to Integration/TradeStation.

[TradeStationOrderTestPolicyTests.cpp](TradeStationOrderTestPolicyTests.cpp)
tests the paper-order fixture's gates without credentials or network: both exact
opt-ins, dedicated-account equality, explicit symbol/finite positive price,
and exact Simulation HTTPS URL (including path/userinfo/port).
