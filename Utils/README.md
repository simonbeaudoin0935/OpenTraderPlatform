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