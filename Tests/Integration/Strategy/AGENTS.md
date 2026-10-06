# Local strategy runtime integration

[StrategySubscriptionErrorTests.cpp](StrategySubscriptionErrorTests.cpp) runs
the external SDK runtime against a temporary Unix-domain mock host.
Cases cover continuing after subscription errors, failing a required symbol,
and failure during startup claim. Assert callback details, exit status, stop
reason, suppressed post-failure log requests, and emitted failure envelopes.

Restore strategy ID/socket environment values. Finish the owned runtime thread
before fixture destruction; do not terminate unrelated processes.
This suite checks runtime transport/callback behavior without loading a real
strategy or making broker requests. Future fragmentation/oversize/disconnect
cases should drive the actual transport with bounded shutdown.

[StrategyTransportTests.cpp](StrategyTransportTests.cpp) drives the SDK's actual
UnixSocketConnection with a temporary local server: bytewise writes, split
payloads, coalesced frames, partial-prefix/payload disconnects, oversized
advertised lengths, and oversized outbound serialization. Reader threads are
joined before fixture destruction. Runtime-level malformed-message callbacks
and host-side buffering still need dedicated coverage.

The shared host/SDK payload limit is 16 MiB. It lives beside the wire-prefix
constant in the public SDK header because the standalone SDK cannot depend on
the application's Qt-based CONSTANTS.h.
