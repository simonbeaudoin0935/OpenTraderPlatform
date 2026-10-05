# Risk Management Engine

## Purpose

The risk-management feature adds a platform-owned guardrail layer that sits in front of all order placement paths (GUI and strategy). It enforces account-scoped limits, tracks runtime risk state across the session, and exposes status/configuration in dedicated GUI surfaces.

This document covers:

- risk configuration model and formulas;
- runtime enforcement flow;
- live/sim/replay persistence behavior;
- GUI integration (`Risk Management` tab + compact status widget);
- strategy manual-confirmation integration (preview brackets, mute behavior).

## Components

| Area | Main files | Role |
| --- | --- | --- |
| Core risk engine | `Src/Algo/RiskManager.h/.cpp` | Config/state loading, day rollover, order evaluation, lock/cooldown, persistence |
| Risk data model | `Src/Core/RiskTypes.h` | `RiskConfig`, `RiskRuntimeState`, `RiskStatusSnapshot`, `RiskDecision`, drawdown basis enum |
| Algo integration | `Src/Algo/MainAlgo.h/.cpp` | Calls risk gate before placement, updates runtime state from balances/orders/positions, emits GUI refresh signal |
| GUI risk config | `Src/FrontEnd/GUI/Tabs/RiskTab.h/.cpp` | Account-scoped config editing and runtime controls (`Reset Day`, `Unlock Trading`) |
| GUI risk status | `Src/FrontEnd/GUI/Widgets/RiskStatus/RiskStatusWidget.h/.cpp` | Compact headroom bar widget above Time & Sales |
| SQL schema | `Src/SQL/RiskStateStoreQueries.h` | `risk_state`, `risk_events`, `risk_config_snapshot` tables in current ledger DB |
| Defaults/settings | `Src/Misc/CONSTANTS.h` (`RiskManagementConstants`) | Bounds, defaults, and per-account settings keys |

## Risk Configuration

`RiskConfig` fields:

- `enabled`
- `dailyDrawdownLimitUsd`
- `dailyDrawdownBasis`
- `maxPlannedLossPerTradeUsd`
- `maxPositionShares`
- `maxPositionNotionalUsd`
- `maxDailyEntryTrades`
- `maxOpenPositions`
- `cooldownEnabled`
- `cooldownLossTriggerUsd`
- `cooldownDurationSec`
- `warningAmberUsedPercent`
- `warningRedUsedPercent`

All values are clamped by `RiskManager::clampConfig(...)` against `RiskManagementConstants`.

## Drawdown Basis Modes

`RiskDrawdownBasis` modes:

1. `EquityPeak` (trailing)
2. `TodaysProfitLoss` (trailing, realized + unrealized)
3. `RealizedProfitLoss` (trailing, closed trades only)
4. `TodaysProfitLossFromBaseline` (non-trailing baseline mode)

Core formulas:

- `reference = basisPeak` for trailing modes, `basisBaseline` for baseline mode.
- `drawdownUsed = max(0, reference - currentMetric)`.
- `drawdownRemainingUsd = max(0, dailyDrawdownLimitUsd - drawdownUsed)`.
- `drawdownRemainingPercent = clamp(drawdownRemainingUsd / dailyDrawdownLimitUsd * 100, 0, 100)`.

Headroom interpretation:

- `remaining` is **not** account cash and not total day PnL.
- `remaining` means how much allowed drawdown budget is still available before lock.
- Example (trailing basis): limit = `$1000`, peak metric reached `+$700`, current metric is `+$200`.
  - drawdown used = `700 - 200 = $500`
  - remaining = `1000 - 500 = $500`
  - day PnL can still be positive while drawdown headroom declines due to pullback from peak.

Important behavior:

- In trailing modes, a profitable peak can still consume budget later if PnL/equity pulls back from that peak.
- In baseline mode, pullbacks above baseline do not consume budget; budget is consumed only when metric drops below baseline.
- Changing drawdown basis rebaselines runtime state immediately (new baseline/peak/current = current metric, lock cleared).

## Runtime State and Daily Lifecycle

`RiskRuntimeState` tracks:

- `riskDay` (market-timezone day, rolls at first early pre-market candle boundary);
- basis fields (`drawdownBasis`, `basisPeak`, `basisBaseline`, `currentMetric`);
- lock/cooldown (`tradingLocked`, `lockReason`, `cooldownUntil`);
- counters (`entryTradesCount`, `openPositionsCount`);
- latest balance-derived metrics (`lastEquity`, `lastTodaysPnl`, `lastRealizedPnl`).

Daily rollover:

- `RiskManager::rolloverIfNeeded(...)` auto-resets day state when resolved risk day changes.
- Manual reset is available via GUI (`Reset Day`) and calls `MainAlgo::resetRiskDayForAccount(...)`.

## Enforcement Pipeline

All order sources go through `MainAlgo::processPlaceOrder(...)`, which invokes:

`RiskManager::evaluateOrder(request, context, now)`

Order checks (entry orders only, in order):

1. trading lock (`risk_locked`)
2. cooldown active (`cooldown_active`)
3. max daily entry trades
4. max open positions (for new symbol entries)
5. max absolute position shares
6. reference price availability
7. max position notional
8. stop price availability (required for planned-loss checks)
9. max planned loss per trade
10. daily drawdown breach (locks trading and rejects)

If rejected, `MainAlgo` returns a rejected `PlaceOrderResult` with risk reason/message and emits `riskStatusChanged(accountId)`.

## Stop/Reference Sources for Risk Checks

For risk evaluation context:

- reference price resolves from:
  - explicit limit price, else
  - stop price (for stop-market), else
  - top-of-book side from latest Level2 snapshot.
- stop price resolves from:
  - explicit order stop price, else
  - active managed-bracket stop for `(account, symbol)` when available.

For manually armed GUI bracket entries:

- when a user submits an opening order with an armed bracket preview, GUI injects the preview stop into `PlaceOrderRequest.stopPrice` if missing so planned-loss checks evaluate the same stop shown on chart.

## Balance/Order/Position Driven Updates

Risk runtime updates are fed by existing streams:

- `onBalanceUpdate(...)` updates basis/current metrics and triggers drawdown lock when breached.
- `onEntryOrderFirstFill(...)` increments entry-trade count once per order ID.
- `updateOpenPositionsCount(...)` tracks open symbol count per account.
- `onPositionClosed(...)` can start cooldown when a closed position PnL is less than or equal to `-cooldownLossTriggerUsd`.

## Persistence Model (Live/Sim/Replay)

### Config persistence

Per-account config persists in `AppState` settings keys:

- `Risk/%1/...` (`%1` = normalized account ID).

### Runtime persistence

Runtime state/events/config snapshots persist in the **current ledger database**:

- `risk_state`
- `risk_events`
- `risk_config_snapshot`

Because the store binds to `LedgerPaths::currentLedgerDatabasePath()`:

- Live/Sim continue from their long-lived ledger files.
- Replay uses per-session ledgers; runtime state is session-scoped.

### Replay restart semantics

`MainApp::restartReplaySession(...)` rotates replay session timestamp, recreates replay ledgers, and restarts replay order streams before preload. This gives a fresh replay risk state on restart (coherent with fresh replay orders/positions).

## GUI Integration

### Risk Management tab

- Tab label: `Risk Management`
- Position in tab order: second tab (immediately after Trade)
- Widget: `RiskTab`
- Features:
  - account-scoped config controls
  - drawdown basis dropdown with per-mode formula tooltips
  - runtime summary/details
  - actions: `Reset Day`, `Unlock Trading`

### Compact risk widget

- Widget: `RiskStatusWidget`
- Placement: above `TimeAndSalesWidget` in a vertical right-side panel stack.
- Shows:
  - remaining/used drawdown headroom bar
  - remaining budget in USD
  - basis reference/current values
  - entries and open-position counters
  - lock/cooldown/ready status
- Color states:
  - green (normal),
  - amber (used >= amber threshold),
  - red (used >= red threshold or locked).

## Strategy Manual-Confirmation Integration

### Strategy confirmation preview bracket

When a strategy emits a user-confirm order request with a stop:

- GUI shows a chart bracket preview for that pending confirmation;
- stop can be adjusted on chart before `Y`;
- on accept, the user-adjusted stop is forwarded as override stop and used in final risk evaluation.

Accept is blocked if preview planned loss exceeds per-trade limit, with a risk warning dialog.

### Hard mute mode (`M`)

`M` toggles global manual-confirm mute mode:

- when enabled:
  - active confirmation is immediately rejected,
  - pending queued confirmations are rejected,
  - future strategy manual confirmations are auto-rejected in `MainAlgo`,
  - chart focus switching and flashing alert cues are suppressed.
- chart watermark `Muted` indicates mode is active.
- pressing `M` again un-mutes and allows new confirmations.

Related prompt keys while active confirmation exists:

- `Y`: accept
- `N`: reject
- `Shift+N`: reject and block future confirmations for that symbol
- `W`: authorize focus switch when switch authorization is required

## Related UX Updates in This Branch

- Replay speed options include `25x` (`Playback::Speed::Fast25x`) in GUI and platform-control protocol.
- Order Entry widget no longer displays arm stop/take percentage controls in the visible layout (controls remain internal for manual bracket defaults).
- Stock chart overlays were adjusted:
  - symbol watermark moved to bottom-right;
  - `Muted` watermark moved to bottom-center;
  - strategy status is rendered as top-left rich-text overlay instead of a dedicated status subpanel.

## Troubleshooting Notes

- If remaining budget appears to decrease after a green trade in trailing modes, verify selected drawdown basis; pullback from a prior peak can consume budget even while net day PnL remains positive.
- For replay confusion, confirm whether session was restarted: replay restart intentionally creates a fresh risk ledger session.
