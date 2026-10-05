# Managed Bracket Engine

## Purpose

Managed brackets are now a **platform-owned** risk mechanism in `MainAlgo`, not strategy-owned trigger loops.
A strategy provides desired stop/take levels, and the platform decides how to enforce them depending on market session and policy.

This covers:
- pre-market / after-hours virtual bracket protection
- regular-hours native broker bracket legs (stop-market + take-profit limit)
- live chart overlay rendering and drag-adjust updates

## Public Strategy API

Strategies use the external SDK methods:

- `upsertManagedBracket(symbol, accountId, side, stopPrice, takePrice, policy, requestId?)`
- `upsertManagedBracket(symbol, accountId, side, stopPrice, takePrice, policy, referenceEntryPrice, requestId?)`
- `cancelManagedBracket(symbol, accountId, requestId?)`

Protocol intents:

- `UpsertBracketIntent`
- `CancelBracketIntent`
- `BracketSide`
- `BracketExecutionPolicy`

`BracketExecutionPolicy` values:

- `Auto`: virtual outside regular hours, native during regular hours
- `VirtualOnly`: always virtual
- `NativeOnly`: attempt native immediately

## Host-Side Flow

1. Strategy sends upsert/cancel intent.
2. `ProcessStrategyRuntimeBackend` validates and forwards to host `StrategySDK`.
3. `StrategySDK` queues into `MainAlgo`:
   - `processUpsertManagedBracket(...)`
   - `processCancelManagedBracket(...)`
4. `MainAlgo` owns runtime state in `m_managedBrackets` keyed by `account+symbol`.

One active bracket is supported per `(account, symbol)` pair. New upserts replace prior state.

`referenceEntryPrice` is optional on upsert. If omitted, host fallback uses current trigger reference (trade/BBO) or stop/take midpoint.

## Enforcement Model

### Virtual mode

When virtual mode is active (`Auto` outside regular or `VirtualOnly`):

- trigger price is derived from recent trade, else BBO midpoint, else best bid/ask fallback
- when stop/take triggers, bracket is marked triggered and a protective exit order is submitted
- virtual exits are single-flight: one tracked in-flight exit order per bracket symbol
- no second virtual exit is submitted until that tracked order reaches a terminal state (fill/reject/cancel/expire)
- in regular session: market exit
- in extended session: aggressive marketable limit exit (`DAY+`)
- for extended-hours stop-loss triggers, non-marketable/stale tracked exits are cancel-replaced until flat

### Native mode

When native mode is active (`Auto` during regular or `NativeOnly`):

- platform submits two broker legs:
  - take-profit limit
  - stop-market stop-loss
- legs share OCO grouping fields in `PlaceOrderRequest` JSON:
  - `OCOGroupID`
  - `OCORoute`
- fill/reject/cancel updates are tracked from order stream; sibling leg is cancelled on fill.
- in replay mode, stop-market fills are triggered from top-of-book (`bid <= stop` for sell stops, `ask >= stop` for buy stops) and the `StopPrice` is propagated through stream updates.

If native leg placement fails or gets rejected:

- regular-session live/sim: drop managed bracket protection immediately (no virtual fallback), clear overlay, and raise
  a blocking GUI warning
- replay mode and non-regular sessions: keep fallback-to-virtual behavior

## Session Transition Behavior

`Auto` policy transitions by session:

- entering regular session: activate native bracket legs
- leaving regular session: cancel native legs and continue virtual monitoring

This preserves protection continuity across pre-market -> regular -> after-hours.

## Quantity Synchronization

Protected quantity is continuously synchronized to the current net position for the same `(account, symbol)`.
If position size changes after entry (scale-in/out), managed bracket quantity follows.
If quantity reaches zero, bracket is cleared automatically.

## Chart Integration

`MainAlgo` emits `managedBracketOverlayEmitted(StrategyBracketOverlayEntry)`.
GUI charts consume this event for stop/take band and labels.
Overlay events now include `referenceEntryPrice` so chart-side bracket wheel adjustments can preserve R-multiple math.

### Drag Adjust

In `StockPriceChart`:

- drag stop or take horizontal line vertically to preview new level
- release mouse to commit (`adjustManagedBracketRequested` signal)
- right-click during drag cancels and restores original levels
- price snapping rounds to 1-cent increments
- middle-click cycles wheel mode: `NORMAL -> RATIO -> STOP -> NORMAL`
- `RATIO` mode: wheel adjusts take-profit R multiple (`R = |take-entry| / |entry-stop|`), stop fixed
- `STOP` mode: wheel adjusts stop-loss percent-from-entry, and take is recomputed to preserve current R
- in `NORMAL` mode, wheel keeps default chart zoom/pan behavior and the mode badge is hidden

Wheel tuning is configurable in Config tab:
- ratio step (`R` per notch)
- stop-percent step (percent points per notch)
- ratio min/max clamps
- stop-percent min/max clamps

`GUIFrontend` and detached `ChartPanel` route this signal back to:

- `MainAlgo::processAdjustManagedBracketLevels(account, symbol, stop?, take?, reason)`

## Current Limitations

- Overlay payload does not currently include account ID (single-account workflows are the intended path).
- Persisted `strategy_bracket_overlays` DB rows are legacy data; active managed brackets are live-state in `MainAlgo`.
