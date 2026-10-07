# Testing

Contributor and AI-session guidance starts at [Tests/AGENTS.md](../Tests/AGENTS.md).
Each suite/domain has a local guide with scenario coverage, isolation rules,
and remaining test gaps.

Use the VS Code `build-then-run-tests` task for the unit and local integration
suites. External TradeStation API tests are excluded from this task.

## TradeStation Simulation smoke test

The `build-then-run-tradestation-api-tests` task explicitly opts into
`TradeStationApiSmokeTests`. It reads the existing OS-keyring TradeStation client
credentials and tokens, uses the platform's startup/refresh flow, and verifies
that the dedicated Simulation account is returned. It also checks balances,
quotes, historical bar contracts and ranges, invalid-symbol errors, positions
snapshot completion, and quote stream delivery/cleanup. It does not place orders.
The process timeout is 240 seconds; each authentication/request wait is bounded.

Prerequisites:
- A working, unlocked native keyring and a previous platform login using OS keyring.
- Original OS-keyring token metadata must still exist.
- The dedicated paper account must be accessible to that login.
- Do not run another process refreshing the same OS-keyring login concurrently.
  CTest serializes this test only within its own run.

The task sets `OTP_TEST_TRADESTATION_API=1` and
`OTP_TEST_TRADESTATION_ACCOUNT` to the configured paper account. Without explicit
opt-in the executable returns CTest's skip code before accessing credentials.
Missing credentials, refresh rejection, persistence failure, request failure,
missing account, and bounded wait timeouts fail the test.

The test forces `TradingMode::Sim` before creating TSClient. Its native backend
preference and app state are temporary; the running application's backend
preference is unchanged. OS-keyring token metadata remains in its original
location so TSClient persists refreshed/rotated tokens and metadata through the
normal platform code. Therefore this test can update the existing OS-keyring
login. The YubiKey vault is not unlocked or modified.

Never include tokens, client secrets, authorization headers, or raw OAuth
responses in test assertions, reports, or committed files.

## Separately opted-in paper-order lifecycle

`build-then-run-tradestation-order-tests` is the only task that enables paper
orders. It prompts for an explicit equity symbol and buy limit price, then submits
**one share** to **SIM2956555M** with Day duration. The buy limit must be below the
current bid, but **a fill is still possible**. Normal completion requires
acknowledgment, cancellation, and a terminal cancellation update.

This task sets `OTP_TEST_TRADESTATION_API=1`,
`OTP_TEST_TRADESTATION_ORDERS=1`, `OTP_TEST_TRADESTATION_ACCOUNT=SIM2956555M`,
`OTP_TEST_TRADESTATION_ORDER_SYMBOL`, and
`OTP_TEST_TRADESTATION_ORDER_LIMIT_PRICE`.
The `tradestation-orders` label is separate from read-only `tradestation-api`.
Routine tests use a local-label allowlist and exclude all `tradestation-` labels.

Cleanup cancels only IDs returned by this test's placement. It never cancels all
account orders or liquidates any position. An unknown placement outcome, cleanup
failure, unexpected fill, timeout, or crash requires **manual paper-account
inspection**. Do not repeat placement automatically. Credential-refresh
concurrency restrictions apply to this task too.
