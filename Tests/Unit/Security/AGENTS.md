# Token and storage scenarios

[OAuthSecurityTests.cpp](OAuthSecurityTests.cpp) is a shared Qt Test executable.
Its default entry covers vault encryption/tamper rejection, refresh-token
selection/validation, callback log redaction, obsolete storage cleanup, and
worker-thread keyring reads. It is not completely environment-independent:
worker-thread keyring reads invoke the host keyring.

Integration CMake registers individual functions in separate processes:
`OAuthKeyringRoundTripTests` uses a real native keyring with unique test service
names; YubiKey session/reset cases use a temporary fake `ykman`, not real hardware;
`OAuthKeyringUnavailableTests` uses an unavailable D-Bus address.

Set explicit Qt application/organization metadata before default QSettings use.
Backend selection is frozen per process, so incompatible backend scenarios need
separate processes. Never use the real TradeStation service for these fixtures.
Use sentinel secrets and check that logs/files do not expose them. Preserve
error-path assertions and cleanup of unique keyring test entries.
