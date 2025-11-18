# L2Trader Utils

This directory contains utility scripts for analyzing and working with L2Trader data.

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