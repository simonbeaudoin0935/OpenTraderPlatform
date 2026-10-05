# Order Emulator - Agent Instructions

## Overview

The OrderEmulator provides realistic order execution simulation during replay mode. It enables strategies and manual trading to place orders that go through realistic lifecycles (reception delay, execution delay, fill logic) without connecting to any real brokerage API.

**When Used**: Only active when `TSClient::Mode::Replay` is set. In live/simulation modes, orders go through the real TradeStation API.

**Purpose**: Allow strategy testing and backtesting during replay playback with realistic order behavior.

## Architecture

```
┌───────────────────────────────────────────────────────────────────────────────┐
│                         ORDER EMULATION ARCHITECTURE                          │
├───────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│   Strategy/GUI ──► MainAlgo ──► TSClient::placeOrder()                       │
│                                       │                                       │
│                       ┌───────────────┼───────────────┐                       │
│                       │               │               │                       │
│                       ▼               ▼               ▼                       │
│               Mode::Live      Mode::Replay     Mode::Sim                     │
│                   │               │               │                          │
│                   │               ▼               │                          │
│                   │    MockNetworkAccessManager   │                          │
│                   │               │               │                          │
│                   │               ▼               │                          │
│                   │        OrderEmulator          │                          │
│                   │               │               │                          │
│                   │      ┌────────┴────────┐      │                          │
│                   │      ▼                 ▼      │                          │
│                   │   m_receptionTimer  m_executionTimer                     │
│                   │      │                 │      │                          │
│                   │      ▼                 ▼      │                          │
│                   │   OPN status       FLL status │                          │
│                   │      │                 │      │                          │
│                   │      └────────┬────────┘      │                          │
│                   │               ▼               │                          │
│                   │    emit orderStatusUpdate()   │                          │
│                   │    emit positionUpdate()      │                          │
│                   │               │               │                          │
│                   │               ▼               │                          │
│                   │      MockNetworkReply         │                          │
│                   │        (orders stream)        │                          │
│                   │               │               │                          │
│                   ▼               ▼               ▼                          │
│           Real QNetworkAccessManager ◄──────────────                         │
│                   │                                                           │
│                   ▼                                                           │
│               StreamOrders ──► OrdersReceiver ──► GUI                        │
│                                                                               │
│   ReplayEngine ─────────────────────────────────────────────────────────┐    │
│       │                                                                  │    │
│       │  injectDepthData signal                                         │    │
│       ▼                                                                  │    │
│   TSClient::onInjectDepthData() ──► OrderEmulator::updateMarketDepth()  │    │
│                                                                          │    │
│   (Depth updates monitored to fill pending limit orders)                │    │
│                                                                          │    │
└──────────────────────────────────────────────────────────────────────────┘    │
```

## Order Lifecycle

### Market Orders

```
1. placeOrder() called
   │
2. Validation (account ID, symbol, balance)
   │
3. Add to m_pendingOrders queue
   │
4. Start m_receptionTimer (100-500ms random delay, scaled by speed)
   │
   └──► Timer fires: onReceptionDelayElapsed()
         │
5. Emit OPN (Open) status order
   │
6. Calculate fill price using current market depth
   │
7. Add to m_executingOrders queue
   │
8. Start m_executionTimer (10-50ms random delay, scaled by speed)
   │
   └──► Timer fires: onExecutionDelayElapsed()
         │
9. Emit FLL (Filled) status order
   │
10. Call updatePosition() - emits position update
```

### Limit Orders (Immediate Fill)

When limit price is at or better than market:
- BUY limit: fills if ask ≤ limit price
- SELL limit: fills if bid ≥ limit price

**Fill price is always the current market price (ask for buys, bid for sells) — NOT the limit price.**
The limit price is a ceiling (buy) or floor (sell) that controls whether the order is marketable.
A buy limit at $15.94 when the ask is $15.70 crosses the spread and fills at $15.70, exactly as a
market order would. This matches real exchange behaviour (price improvement).

### Limit Orders (Delayed Fill)

When limit price cannot be filled immediately:

```
1-5. Same as market order through OPN emission
   │
6. Cannot fill: add to m_openOrders map
   │
7. On each depth update for symbol:
   │   │
   │   └── updateMarketDepth() called
   │         │
   │         └── Check if order can now fill
   │               │
   │               └── If yes: move to m_executingOrders
   │
8. m_executionTimer fires → FLL + position update
```

### Stop Orders

- Stop-market orders trigger from top-of-book:
  - BUY-side stops trigger when `best ask >= stop`
  - SELL-side stops trigger when `best bid <= stop`
- Stop-limit orders require stop trigger first, then normal limit marketability checks.
- `StopPrice` is emitted on replay order-stream updates so GUI and ledgers can display the stop level.

### Order Cancellation

```
cancelOrder(orderID)
   │
   └── Find in m_openOrders
         │
         ├── Not found: log warning, return
         │
         └── Found: emit CAN status, remove from m_openOrders
```

## Order Status Codes

Status codes and descriptions **exactly match** TradeStation API. Status descriptions are centralized in `Order::getStatusDescriptionForStatus()` (see `Src/Clients/TSClient/Brokerage/GetOrders/Order.h`).

**Commonly Used in Replay**:

| Status | StatusDescription | When Used |
|--------|-------------------|-----------|
| `ACK` | "Received" | Order received by emulator |
| `OPN` | "Sent" | Order acknowledged, waiting to fill |
| `FLL` | "Filled" | Order completely filled |
| `REJ` | "Rejected" | Validation failed (balance, symbol, etc.) |
| `CAN` | "Canceled" | User cancelled open order |
| `DON` | "Queued" | Order queued (currently unused) |

**Order ID Format**: 9-digit numeric IDs starting at 900000000, matching real TradeStation API format (e.g., "900000001", "900000002"). Previously used "EMU-1000" prefix which caused assertion failures during cancellation.

## Data Structures

### PendingOrder
Orders waiting for reception delay to elapse.

```cpp
struct PendingOrder {
    PlaceOrderRequest request;  // Original order request
    QString requestID;          // Correlation ID
    qint64 submitTimeMs;        // Wall-clock submission time
    qint64 remainingDelayMs;    // For pause/resume support
};
```

### ExecutingOrder
Orders that have been acknowledged (OPN) and are waiting for execution delay.

```cpp
struct ExecutingOrder {
    QJsonObject orderJson;      // Order data (no default ctor workaround)
    double fillPrice;           // Calculated fill price
    qint64 remainingDelayMs;    // For pause/resume support
};
```

### PositionData
Internal position tracking separate from Position objects.

```cpp
struct PositionData {
    QString symbol;
    int quantity;           // Positive = long, negative = short
    double averagePrice;    // Weighted average entry price
};
```

## Key Methods

| Method | Purpose |
|--------|---------|
| `placeOrder()` | Entry point for new orders; validates and queues |
| `cancelOrder()` | Cancels open limit orders |
| `updateMarketDepth()` | Receives Level 2 depth snapshots; checks pending fills |
| `updateQuote()` | Receives Level 1 quote data; enables fills for symbols without L2 |
| `onReceptionDelayElapsed()` | Processes pending orders after delay |
| `onExecutionDelayElapsed()` | Fills orders after execution delay |
| `canFillLimitOrder()` | Checks if limit order conditions are met |
| `canFillStopOrder()` | Checks if stop order trigger/fill conditions are met |
| `calculateMarketOrderFillPrice()` | Determines fill price for market orders |
| `fillOrder()` | Marks order as filled, emits update |
| `updatePosition()` | Creates/updates position, calculates P&L |
| `recalculatePositionPnL()` | Updates unrealized P&L when market data changes |
| `validateOrder()` | Full order validation before acceptance |
| `pause()/resume()` | Freezes/resumes timers for replay pause |
| `clear()` | Resets all state for new replay session |

## Market Data Sources

The OrderEmulator supports two data sources for order fills and position P&L:

### Level 2 (Market Depth) - Primary
- Full order book with multiple price levels
- More accurate fill prices using actual depth
- Limited to 10 symbols due to API stream limits
- `updateMarketDepth()` receives depth snapshots

### Level 1 (Quote) - Fallback
- Best bid/ask only
- Enables fills for all 100 symbols in replay
- Used when Level 2 stream is not available for a symbol
- `updateQuote()` receives quote data

> **⚠️ Note**: The `updateQuote()` method is a legacy interface from the TradeStation era.
> The `Quote` type was removed in Phase 3 of the Databento migration. In the current codebase,
> `OrderEmulator` uses `Level2` (from `Src/Core/Models/Level2.h`) for market depth data.
> The fill logic uses `Level2.m_bids[0]`/`Level2.m_asks[0]` for BBO.

**Fill Price Logic** (market and limit orders use identical price calculation):
```cpp
if (hasMarketDepthData(symbol)) {
    // Use L2: walk the book for accurate fill price (ask for buy, bid for sell)
    fillPrice = calculateMarketOrderFillPrice(order, depth);
} else if (hasQuoteData(symbol)) {
    // Use L1: bid/ask crossing (ask for buy, bid for sell)
    fillPrice = calculateMarketOrderFillPriceFromQuote(order, quote);
} else {
    // Reject: no market data
    rejectOrder("No market data available");
}
// For limit orders: fill price = market price, not limit price.
// The limit price only determines whether the order is marketable.
```

## Position Tracking

When an order fills, `updatePosition()` is called:

1. **Find or create** PositionData for the symbol
2. **Calculate new quantity** based on trade action:
   - BUY/BUYTOCOVER: `newQty = currentQty + fillQty`
   - SELL/SELLSHORT: `newQty = currentQty - fillQty`
3. **Calculate weighted average price**:
   - Adding to position: `avgPrice = (oldCost + newCost) / newQty`
   - Closing position: Keep existing avg price until position closes
4. **Update balance**: Deduct on buys, add on sells
5. **Calculate P&L**: `unrealizedPL = (fillPrice - avgPrice) * newQty`
6. **Emit position update** via `positionUpdate` signal

## Delay Scaling

Delays are scaled based on replay speed:

```cpp
int scaledDelay = (baseDelay * 100) / replaySpeedPercent;
```

| Speed | Reception Delay | Execution Delay |
|-------|-----------------|-----------------|
| 100% (1x) | 100-500ms | 10-50ms |
| 200% (2x) | 50-250ms | 5-25ms |
| 1000% (10x) | 10-50ms | 1-5ms |
| -1 (max) | 0ms | 0ms |

## Validation Rules

`validateOrder()` checks:
1. **Account ID**: Must be `SIM123456`
2. **Quantity**: Must be > 0
3. **Symbol**: Must not be empty
4. **Market Data**: Depth snapshot must exist for symbol
5. **Buying Power Reservation**:
   - New order impact is validated against account cash using a conservative reference price
   - Existing pending/open/executing orders reserve buying power first
   - Rejects if `reserved + new` would exceed available balance
6. **Boxing Prevention**:
   - Long position + SELLSHORT = rejected
   - Short position + BUY (not cover) = rejected
7. **Position Reservation**:
   - Pending/open/executing `SELL`/`SELLTOCLOSE` reserve long shares
   - Pending/open/executing `BUYTOCOVER`/`BUYTOCLOSE` reserve short shares
   - Prevents duplicate exits that would flip a position unintentionally

## Signals

| Signal | Description |
|--------|-------------|
| `orderStatusUpdate(QByteArray)` | JSON order status for StreamOrders |
| `positionUpdate(QByteArray)` | JSON position for StreamPositions |

Both signals connect to `MockNetworkReply::injectData()` in TSClientStreams.cpp.

## Threading Model

- **Lives in**: TSClient thread
- **Called from**:
  - TSClient thread: `placeOrder()`, `cancelOrder()` via MockNetworkAccessManager
  - ReplayEngine thread: `updateMarketDepth()` (cross-thread via signal)
- **Signal delivery**: Both signals use `Qt::QueuedConnection` to MockNetworkReply

## Configuration Constants

Defined in OrderEmulator.h:

```cpp
static constexpr int MIN_RECEPTION_DELAY_MS = 100;
static constexpr int MAX_RECEPTION_DELAY_MS = 500;
static constexpr int MIN_EXECUTION_DELAY_MS = 10;
static constexpr int MAX_EXECUTION_DELAY_MS = 50;
```

Simulated account:
```cpp
static QString getSimulatedAccountID() { return "SIM123456"; }
```

Starting balance: `$100,000` (stored in `m_balance`)

**Order ID Generation**: Starting value is 900000000, increments sequentially:
```cpp
QString m_nextOrderID = 900000000;  // Generates: "900000000", "900000001", ...
```

## Integration Points

| Component | Integration |
|-----------|-------------|
| `TSClient::setMode()` | Creates/destroys OrderEmulator instance |
| `MockNetworkAccessManager` | Routes placeOrder/cancelOrder calls |
| `TSClientStreams.cpp` | Creates mock streams, connects signals |
| `TSClient::onInjectDepthData()` | Forwards depth to OrderEmulator |
| `MainAlgo::startReplayOrderStreams()` | Emits simulated account to GUI |

## Error Handling

| Error | Behavior |
|-------|----------|
| Invalid account ID | REJ status, immediate response |
| Zero/negative quantity | REJ status |
| Missing symbol | REJ status |
| No depth data | REJ with "No market data available" |
| Insufficient buying power | REJ with reserved + required breakdown |
| Boxing position | REJ with EC803 error code |
| Over-committed sell/cover | REJ with EC401/EC602-style message |
| Cancel unknown order | Warning log, no response |

## Future Enhancements (Deferred)

- **Book Walking**: Walk multiple depth levels for large orders
- **Commission/Fees**: Configurable per-trade or per-share fees
- **Multiple Accounts**: Support multiple simulated accounts
- **Day Trading Rules**: PDT pattern detection
- **Partial Fills**: Split large orders across time
