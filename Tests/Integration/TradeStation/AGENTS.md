# Opt-in TradeStation Simulation integration

[TradeStationApiSmokeTests.cpp](TradeStationApiSmokeTests.cpp) runs read-only
scenarios under its smoke-test CTest registration.
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

## Separately gated paper-order lifecycle

`TradeStationPaperOrderTests` selects only `paperOrderLifecycle` in a separate
process and uses label `tradestation-orders`, never `tradestation-api`.
It requires both API and order opt-ins, exact account SIM2956555M, explicit equity
symbol and finite positive buy limit price, network mode, and the exact Simulation
HTTPS base URL. Inputs are checked before credential access. It buys one share
with Day duration, only below the current bid, waits for stream acknowledgment,
cancels the returned test-created order ID, and requires a cancellation terminal
update. The order can fill; a fill fails the test and is NOT automatically
liquidated. Existing positions and unrelated orders are never touched.

Cleanup also runs after failed assertions. It waits for a late placement result,
collects only returned IDs, cancels nonterminal test-created orders, and reports
missing IDs, cancellation errors, missing terminal updates, or fills explicitly.
An ambiguous placement without an ID requires manual account inspection: do not
guess identities or retry placement. Forced process termination cannot guarantee
cleanup; inspect the account if the test times out or crashes.

Run `build-then-run-tradestation-order-tests` explicitly; it prompts for symbol
and limit price. [TradeStationOrderTestPolicy.h](TradeStationOrderTestPolicy.h)
holds shared input/endpoint gates exercised by local unit tests.
[TradeStationOptInGuardTests.cpp](TradeStationOptInGuardTests.cpp) is a
`local-integration` process test: it launches the actual API executable with
disabled gates or invalid order inputs and asserts exit 77/1 before the Qt Test
fixture starts. Child metadata and D-Bus paths are isolated. It never supplies a
fully enabled valid order configuration.
Order tests MUST have a separate opt-in, exact dedicated-account and Simulation
endpoint guards, bounded cancellation/terminal-state cleanup, and tracking only
test-created orders. Never cancel all account orders or close existing positions.
