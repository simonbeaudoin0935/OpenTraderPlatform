# OrderEmulator integration coverage

[OrderEmulatorTests.cpp](OrderEmulatorTests.cpp) exercises replay order
validation, market/limit/stop order lifecycles, cancellation, regular-session
gating, position open/close accounting, realized P&L, and clear/reset behavior
through the public `OrderEmulator` API.

The fixture uses fixed New York timestamps, synthetic top-of-book `Level2`
snapshots, and `setReplaySpeed(-1)` so reception and execution timers complete
without waiting for randomized replay delays. Tests observe the public order
and position update signals rather than accessing private queues.
