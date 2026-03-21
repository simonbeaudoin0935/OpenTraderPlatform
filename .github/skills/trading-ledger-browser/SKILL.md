---
name: 'Trading Ledger Browser'
description: 'Skills for locating and querying L2Trader order and position SQLite ledgers for live, paper, and replay sessions.'
---

# Copilot Skills

This file teaches GitHub Copilot how to locate and query L2Trader's order and position SQLite ledgers for live trading, paper trading, and replay sessions.

---

## Where are the ledgers?

L2Trader stores trading ledgers under:

```
~/.local/share/L2Trader/
```

There are two top-level ledger folders:

| Folder | Contents |
|--------|----------|
| `~/.local/share/L2Trader/Orders/` | Order ledger |
| `~/.local/share/L2Trader/Positions/` | Position ledger |

Each folder contains the same three mode-specific subfolders:

| Subfolder | Meaning | Orders file | Positions file |
|-----------|---------|-------------|----------------|
| `Live/` | Real-money trading | `Orders.db` | `Positions.db` |
| `Simulation/` | Paper trading | `Orders.db` | `Positions.db` |
| `Replay/` | Replay sessions | `Orders_YYYY-MM-DD_HHMMSS.db` | `Positions_YYYY-MM-DD_HHMMSS.db` |

### Important storage rule

- `Live/` and `Simulation/` each use one SQLite file that accumulates entries across many days.
- `Replay/` uses one orders DB and one positions DB per platform invocation session.
- Matching replay order and position files share the same timestamp suffix, for example:
  - `Orders_2026-03-07_032509.db`
  - `Positions_2026-03-07_032509.db`

---

## AI agent shell note

> **Important for AI agents**: assign DB paths to variables first, then reuse them. Never nest `$(...)` inside another `$(...)`.

Examples:

```bash
ORDERS_DB=~/.local/share/L2Trader/Orders/Simulation/Orders.db
POSITIONS_DB=~/.local/share/L2Trader/Positions/Simulation/Positions.db
```

```bash
SESSION=2026-03-07_032509
ORDERS_DB=~/.local/share/L2Trader/Orders/Replay/Orders_${SESSION}.db
POSITIONS_DB=~/.local/share/L2Trader/Positions/Replay/Positions_${SESSION}.db
```

---

## Schemas that matter when querying

### Orders ledger

The `orders` table stores one row per order. Useful columns:

- `order_id`
- `account_id`
- `symbol`
- `quantity`
- `trade_action`
- `order_type`
- `status`
- `filled_price`
- `opened_datetime`
- `closed_datetime`
- `latency_ms`
- `json_data`

The same DB also contains a `strategy_logs` table with:

- `strategy_id`
- `symbol`
- `timestamp`
- `message`

### Positions ledger

The `positions` table stores one persisted row per position ID and updates it when the position structurally changes. Useful columns:

- `position_id`
- `account_id`
- `symbol`
- `quantity`
- `average_price`
- `last`
- `unrealized_profit_loss`
- `deleted`
- `timestamp`
- `opened_datetime`
- `closed_datetime`
- `json_data`

### Important positions caveat

- The positions DB is **not** a full mark-to-market tick history.
- Pure mark-to-market updates with the same quantity and average price are skipped.
- The DB is updated on structural changes such as open, add, reduce, and close.
- For the most current live position or P&L view, the in-memory app state is more authoritative than the DB alone.

---

## Skill: Inspect live or paper ledgers by day

Set the mode first. Replace `Simulation` with `Live` for real-money trading.

```bash
ORDERS_DB=~/.local/share/L2Trader/Orders/Simulation/Orders.db
POSITIONS_DB=~/.local/share/L2Trader/Positions/Simulation/Positions.db
```

### List the trading days present in the orders ledger

```bash
sqlite3 -header -column "$ORDERS_DB" "SELECT substr(opened_datetime, 1, 10) AS day, COUNT(*) AS orders FROM orders WHERE opened_datetime IS NOT NULL GROUP BY substr(opened_datetime, 1, 10) ORDER BY day DESC;"
```

### Show all orders for a specific day

```bash
DAY=2026-03-07
sqlite3 -header -column "$ORDERS_DB" "SELECT order_id, symbol, trade_action, order_type, status, quantity, filled_price, opened_datetime, closed_datetime FROM orders WHERE opened_datetime IS NOT NULL AND substr(opened_datetime, 1, 10) = '$DAY' ORDER BY opened_datetime;"
```

### Count orders per symbol for a specific day

```bash
DAY=2026-03-07
sqlite3 -header -column "$ORDERS_DB" "SELECT symbol, COUNT(*) AS orders FROM orders WHERE opened_datetime IS NOT NULL AND substr(opened_datetime, 1, 10) = '$DAY' GROUP BY symbol ORDER BY orders DESC, symbol;"
```

### Show positions whose last structural update happened on a specific day

```bash
DAY=2026-03-07
sqlite3 -header -column "$POSITIONS_DB" "SELECT position_id, symbol, quantity, average_price, last, unrealized_profit_loss, deleted, timestamp, opened_datetime, closed_datetime FROM positions WHERE substr(timestamp, 1, 10) = '$DAY' ORDER BY timestamp;"
```

### Show positions opened or closed on a specific day

```bash
DAY=2026-03-07
sqlite3 -header -column "$POSITIONS_DB" "SELECT position_id, symbol, quantity, deleted, opened_datetime, closed_datetime, timestamp FROM positions WHERE (opened_datetime IS NOT NULL AND substr(opened_datetime, 1, 10) = '$DAY') OR (closed_datetime IS NOT NULL AND substr(closed_datetime, 1, 10) = '$DAY') ORDER BY opened_datetime, closed_datetime, timestamp;"
```

---

## Skill: Inspect a replay session

### List available replay sessions

```bash
ls -1 ~/.local/share/L2Trader/Orders/Replay/Orders_*.db
ls -1 ~/.local/share/L2Trader/Positions/Replay/Positions_*.db
```

Copy the timestamp from the filenames you want, for example `2026-03-07_032509`.

### Open the matching replay ledgers

```bash
SESSION=2026-03-07_032509
ORDERS_DB=~/.local/share/L2Trader/Orders/Replay/Orders_${SESSION}.db
POSITIONS_DB=~/.local/share/L2Trader/Positions/Replay/Positions_${SESSION}.db
```

### Show all orders from that replay session

```bash
sqlite3 -header -column "$ORDERS_DB" "SELECT order_id, symbol, trade_action, order_type, status, quantity, filled_price, opened_datetime, closed_datetime FROM orders ORDER BY opened_datetime;"
```

### Show all persisted positions from that replay session

```bash
sqlite3 -header -column "$POSITIONS_DB" "SELECT position_id, symbol, quantity, average_price, last, unrealized_profit_loss, deleted, timestamp, opened_datetime, closed_datetime FROM positions ORDER BY symbol, position_id;"
```

### Show positions that were closed during that replay session

```bash
sqlite3 -header -column "$POSITIONS_DB" "SELECT position_id, symbol, quantity, deleted, opened_datetime, closed_datetime FROM positions WHERE closed_datetime IS NOT NULL ORDER BY closed_datetime;"
```

### Read strategy chart-log markers from the same replay orders DB

```bash
sqlite3 -header -column "$ORDERS_DB" "SELECT strategy_id, symbol, timestamp, message FROM strategy_logs ORDER BY timestamp;"
```

---

## Diagnostic workflow

If you need orders or positions for a specific trading day or replay session:

1. Decide whether you need the `Orders` ledger, the `Positions` ledger, or both.
2. Choose the mode:
   - `Live` for real-money trading
   - `Simulation` for paper trading
   - `Replay` for a specific replay session
3. For `Live` or `Simulation`, query the single ledger DB by date using `opened_datetime`, `closed_datetime`, or `timestamp`.
4. For `Replay`, first pick the session timestamp from the filename, then open the matching `Orders_...db` and `Positions_...db` files.
5. If position results look incomplete, remember that the DB records structural position changes, not every mark-to-market fluctuation.

---

## Notes

- `Orders` and `Positions` are stored in separate SQLite files; you often need to inspect both.
- Replay order and position files with the same timestamp belong to the same platform invocation.
- `Simulation` is paper trading; `Replay` is local session playback.
- The `json_data` column stores the full serialized object and is useful when the summary columns are not enough.
- A reliable day filter for these text datetime columns is `substr(column, 1, 10) = 'YYYY-MM-DD'`.
- To quickly discover the tables in a ledger:

```bash
sqlite3 "$ORDERS_DB" ".tables"
sqlite3 "$POSITIONS_DB" ".tables"
```
