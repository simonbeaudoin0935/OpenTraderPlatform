# Opt-in TradeStation Simulation integration

[TradeStationApiSmokeTests.cpp](TradeStationApiSmokeTests.cpp) is read-only.
It reuses OS-keyring client/auth tokens and original OS-keyring metadata,
lets TSClient reuse or refresh credentials, and discovers the configured paper
account. It never places orders or unlocks the YubiKey vault.

## Execution boundary

CTest label: `tradestation-api`; serial execution; 240-second process timeout.
Without `OTP_TEST_TRADESTATION_API=1`, main returns skip code 77 BEFORE fixture
construction or credential access. Account input is
`OTP_TEST_TRADESTATION_ACCOUNT`; the VS Code explicit task supplies SIM2956555M.
Routine tasks must exclude this label; the executable gate is a second safeguard.

Select the `build-then-run-tradestation-api-tests` VS Code task explicitly.
See [testing prerequisites](../../../Doc/TESTING.md). Report real API execution
separately from compilation/skipped tests. Do not claim connectivity from a skip.

## Credentials and account safety

The temporary NativeFormat preference selects OSKeyring only for this process.
Do NOT redirect IniFormat token metadata to a temporary path: TSClient needs
the original metadata and persists rotated tokens there. Existing OS-keyring
credentials may change on refresh. Avoid concurrent refresh of the same login.
No secrets may appear in assertions, URLs, artifacts, or source.

Force `TradingMode::Sim` before creating TSClient. `TSClient::Mode::Live` means
network operation, not real-money trading; Live trading mode is forbidden here.
Account discovery requires valid client/token prerequisites, bounded auth wait,
successful account result, and exact configured-account membership.

## Read-only scenarios

Account discovery, account-scoped finite balance values, SPY quote contracts,
historical minute ordering/OHLC/range on a completed trading date selected from
daily bars, explicit invalid-symbol errors, complete positions snapshots
(including empty accounts), and initial quote stream delivery/cleanup.
Stream connections are installed on the worker before it can deliver data.
These external paths are compiled but must be reported as unverified until
actually executed against TradeStation.

## Planned extensions, not current coverage

Paper-order lifecycle is not implemented yet.
Order tests MUST have a separate opt-in, exact dedicated-account and Simulation
endpoint guards, bounded cancellation/terminal-state cleanup, and tracking only
test-created orders. Never cancel all account orders or close existing positions.
