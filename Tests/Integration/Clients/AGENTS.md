# Local client/stream integration

[StreamRecoveryTests.cpp](StreamRecoveryTests.cpp) uses an isolated config and
locked simulated-YubiKey vault, then starts TSClient in Replay mode.
It checks final error bodies parsed once, terminal errors versus capped retry
backoff, stream factories on the client thread, and queued depth subscription
completion without self-deadlock.

Use explicit Qt organization/application names and temporary settings.
Cleanup tracks whether the client was created so init failure does not cause
an unrelated singleton-destruction assertion. Keep the simulated backend locked
before startup; these scenarios must not retrieve real credentials or open
broker streams. Tests use mock replies and Replay, not paper API trading.
