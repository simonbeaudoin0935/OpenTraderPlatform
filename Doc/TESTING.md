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
that the dedicated Simulation account is returned. It does not place orders.

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
