# L2Trader Utils

This directory contains utility scripts for analyzing and working with L2Trader data, and tools for developers.

## install-git-hooks.sh

Installs Git hooks to enforce code quality standards.

### Usage

```bash
./install-git-hooks.sh
```

This script will:
1. Check if Uncrustify is installed
2. Install the pre-commit hook to `.git/hooks/`
3. Make the hook executable

The pre-commit hook automatically checks:
- Code formatting using Uncrustify
- Trailing whitespace in C++ files

See [Git Pre-Commit Hook documentation](../Doc/Git_Pre_Commit_Hook.md) for more information.

### Requirements

- `uncrustify` (install with `sudo apt-get install uncrustify` on Ubuntu/Debian)

## git-hooks/

Contains Git hook scripts that can be installed using `install-git-hooks.sh`.

- **pre-commit**: Validates code formatting before commits

## analyze_db.sh

A shell script to analyze SQLite database files created by the L2Trader recorder.

### Usage

```bash
./analyze_db.sh <database_file.db>
```

### Supported Database Types

- **LiveBarsDB**: Files containing bar data (table: `bars`)
- **LiveMarketDepthQuoteDB**: Files containing market depth quote data (table: `market_depth_quotes`)

### Output

The script provides:

1. **Database Type**: Identifies whether it's bars or market depth data
2. **Total Stocks**: Number of unique stock tickers in the database
3. **Per-Stock Statistics**:
   - **Entries**: Total number of records for each stock
   - **Data Size**: Total characters in JSON data for each stock
   - **Sequence Numbers**: Count of unique sequence numbers per stock
4. **Summary**: Overall statistics for the entire database

### Example Output

```
=== Database Analysis: RecordedLiveBars_2025-11-17.db ===
Database Type: LiveBarsDB
Table Name: bars

Total number of stocks: 99

Per-stock statistics:
Stock Ticker | Entries | Data Size (chars) | Sequence Numbers
-------------|---------|-------------------|------------------
AAPL         | 6715    | 4412398           | 6714
...
```

### Database Schema

Both database types use the same schema:
- `id`: Primary key (auto-increment)
- `stockTicker`: Stock symbol (e.g., "AAPL")
- `stockTickerSeq`: Sequence number per stock ticker
- `epochMs`: Timestamp in milliseconds since epoch
- `jsonRawData`: Raw JSON data from the API

### Requirements

- `sqlite3` command-line tool
- Bash shell

## migrate_db_add_objecttype.sh

Migrates existing bars and market depth quote databases recorded before the
`objectType` column was introduced. Each row is classified as the primary data
type (`Bar` / `MarketDepthQuote`), `Heartbeat`, or `Error` by inspecting the
stored JSON. The script is **idempotent** — running it on an already-migrated
database is a no-op.

### Usage

```bash
# Migrate a single file
./migrate_db_add_objecttype.sh 2025-11-17.db

# Migrate all databases in a directory
./migrate_db_add_objecttype.sh ~/.cache/L2Trader/RecordedLiveData/Bars/
./migrate_db_add_objecttype.sh ~/.cache/L2Trader/RecordedLiveData/MarketDepthQuotes/
```

### Example Output

```
=== Migrating: 2025-11-17.db ===
  Table:       bars
  Total rows:  8730
  Adding objectType column...
  Classifying rows...
  Classification results:
    Bar                  8612 rows
    Heartbeat              116 rows
    Error                    2 rows
  Migration complete.
```

### Requirements

- `sqlite3` command-line tool (SQLite 3.38+ for `json_valid` / `json_extract`)
- Bash shell

## migrate_split_ledgers_to_combined.sh

Migrates the legacy split trading ledgers:

- `Orders/...`
- `Positions/...`

into the new combined layout:

- `Ledgers/Live/Ledger.db`
- `Ledgers/Simulation/Ledger.db`
- `Ledgers/Replay/Ledger_<timestamp>.db`

The script performs basic verification by comparing source and destination row
counts for `orders`, `positions`, and `strategy_logs`, then deletes the legacy
pair only after the combined ledger has been created successfully.

### Usage

```bash
# Migrate the default L2Trader data directory
./migrate_split_ledgers_to_combined.sh

# Migrate a custom data root
./migrate_split_ledgers_to_combined.sh ~/.local/share/L2Trader
```

### Requirements

- `sqlite3` command-line tool
- Bash shell
