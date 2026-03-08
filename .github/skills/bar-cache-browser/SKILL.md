---
name: 'BarCache Browser'
description: 'Skills for browsing and analyzing the BarCache data structure used in L2Trader for storing historical market data.'
---

# BarCache Browser

This file teaches GitHub Copilot how to inspect and query the L2Trader BarCache SQLite databases to diagnose bar data issues.

---

## Database Location

BarCache stores **one SQLite file per symbol** in:

```
~/.cache/L2Trader/Bars/{SYMBOL}.db
```

Examples:
- `~/.cache/L2Trader/Bars/NVDA.db`
- `~/.cache/L2Trader/Bars/AAPL.db`

---

## Schema

```sql
CREATE TABLE IF NOT EXISTS bars (
    date    TEXT,       -- 'yyyy-MM-dd'  e.g. '2026-03-02'
    [index] INTEGER,    -- minute offset: 0 = 04:00, 1 = 04:01, ..., 899 = 18:59
    open    REAL,
    high    REAL,
    low     REAL,
    close   REAL,
    volume  INTEGER,
    status  INTEGER DEFAULT 0,  -- 0=Uninitialized, 1=Null, 2=Open, 3=Closed
    PRIMARY KEY (date, [index])
)
```

### Index ↔ Time conversion

```
time  → index : (hour - 4) * 60 + minute       (e.g. 07:00 → 180, 10:00 → 360)
index → time  : hour = 4 + index/60, min = index%60
```

Trading day spans indices 0–899 (4:00 AM – 6:59 PM ET, 900 bars total).

### Bar status values

| status | meaning |
|--------|---------|
| 0 | Uninitialized (cache miss / never written) |
| 1 | Null (minute with no trading activity — hole-fill) |
| 2 | Open (live bar, still updating) |
| 3 | Closed (complete historical bar) |

---

## Common sqlite3 Queries

> **Important for AI agents**: Always assign filenames to variables first — never nest `$(...)` inside another `$(...)` (blocked by shell security policy).

### List available symbols

```bash
ls ~/.cache/L2Trader/Bars/
```

### List available dates for a symbol

```bash
sqlite3 ~/.cache/L2Trader/Bars/NVDA.db "SELECT DISTINCT date FROM bars ORDER BY date;"
```

### Count bars per date (sanity check — should be 900 for complete days)

```bash
sqlite3 ~/.cache/L2Trader/Bars/NVDA.db "SELECT date, COUNT(*) as total, SUM(CASE WHEN status=1 THEN 1 ELSE 0 END) as null_bars, SUM(CASE WHEN status=3 THEN 1 ELSE 0 END) as closed_bars FROM bars GROUP BY date ORDER BY date;"
```

### Inspect bars for a specific date and time range

```bash
# Show bars for 2026-03-02 between 07:00 (index 180) and 10:05 (index 365)
sqlite3 ~/.cache/L2Trader/Bars/NVDA.db "SELECT [index], 4 + [index]/60 || ':' || printf('%02d', [index]%60) as time, open, high, low, close, volume, status FROM bars WHERE date='2026-03-02' AND [index] >= 180 AND [index] <= 365 ORDER BY [index];"
```

### Find null-bar ranges for a date (to spot gaps)

```bash
sqlite3 ~/.cache/L2Trader/Bars/NVDA.db "SELECT [index], 4 + [index]/60 || ':' || printf('%02d', [index]%60) as time, status FROM bars WHERE date='2026-03-02' AND status != 3 ORDER BY [index];"
```

### Check a specific time (e.g. 07:00 = index 180)

```bash
sqlite3 ~/.cache/L2Trader/Bars/NVDA.db "SELECT [index], open, high, low, close, volume, status FROM bars WHERE date='2026-03-02' AND [index]=180;"
```

### Show first and last real bar for a date

```bash
sqlite3 ~/.cache/L2Trader/Bars/NVDA.db "SELECT MIN([index]), MAX([index]) FROM bars WHERE date='2026-03-02' AND status=3;"
```

### Dump a full day as CSV

```bash
sqlite3 -csv ~/.cache/L2Trader/Bars/NVDA.db "SELECT [index], 4 + [index]/60 || ':' || printf('%02d', [index]%60), open, high, low, close, volume, status FROM bars WHERE date='2026-03-02' ORDER BY [index];" | head -50
```

---

## Diagnostic Workflow: Diagnosing a Chart Bar Gap

If a chart shows missing bars between time A and time B on a given date:

1. **Check the date exists in cache:**
   ```bash
   sqlite3 ~/.cache/L2Trader/Bars/NVDA.db "SELECT date, COUNT(*) FROM bars WHERE date='2026-03-02' GROUP BY date;"
   ```

2. **Check bar statuses in the suspect range** (e.g. 7:00–10:00 = indices 180–360):
   ```bash
   sqlite3 ~/.cache/L2Trader/Bars/NVDA.db "SELECT [index], 4+[index]/60||':'||printf('%02d',[index]%60) as t, status, close FROM bars WHERE date='2026-03-02' AND [index] BETWEEN 180 AND 360 ORDER BY [index];"
   ```
   - `status=1` rows are **null bars** — no trading occurred (or data was not downloaded)
   - `status=3` rows are **real closed bars**

3. **Check if the full day was downloaded** (should have 900 rows):
   ```bash
   sqlite3 ~/.cache/L2Trader/Bars/NVDA.db "SELECT COUNT(*) FROM bars WHERE date='2026-03-02';"
   ```

4. **If null bars exist where real bars are expected**, the data was not downloaded from Databento for that range. The fix is to clear the stale day from cache and let the app re-fetch, or to trigger a fresh download from the Downloads tab.

---

## Notes

- The bar cache is designed with a **"complete day or nothing" rule**: if a date is present, all 900 bars must exist (either real or null). Partial days should never appear in a healthy cache.
- Null bars (status=1) are **intentional** for non-trading minutes (e.g. outside market hours where no trades occurred). Real gaps appear as null bars with all-zero OHLCV values.
- The cache is a **read-through write-back cache**: on miss it fetches from Databento API, fills holes with null bars, writes the full day to DB, then returns the requested range.
- In replay mode, `MainApp::getCurrentAppTime()` returns the replay time, which affects the `isCurrentDay` logic in `BarCache::getBars()`. Only past dates (relative to replay time) are treated as "complete".
