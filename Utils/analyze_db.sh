#!/bin/bash

# Database Analysis Script for L2Trader
# Analyzes LiveBarsDB and LiveMarketDepthQuoteDB files

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 <database_file.db>"
    echo "Example: $0 RecordedLiveBars_2025-11-17.db"
    exit 1
fi

DB_FILE="$1"

if [ ! -f "$DB_FILE" ]; then
    echo "Error: Database file '$DB_FILE' does not exist"
    exit 1
fi

# Check if it's a valid SQLite database
if ! sqlite3 "$DB_FILE" ".schema" >/dev/null 2>&1; then
    echo "Error: '$DB_FILE' is not a valid SQLite database"
    exit 1
fi

echo "=== Database Analysis: $DB_FILE ==="
echo

# Determine database type and table name
if sqlite3 "$DB_FILE" "SELECT name FROM sqlite_master WHERE type='table' AND name='bars';" | grep -q bars; then
    TABLE_NAME="bars"
    DB_TYPE="LiveBarsDB"
elif sqlite3 "$DB_FILE" "SELECT name FROM sqlite_master WHERE type='table' AND name='market_depth_quotes';" | grep -q market_depth_quotes; then
    TABLE_NAME="market_depth_quotes"
    DB_TYPE="LiveMarketDepthQuoteDB"
else
    echo "Error: Database does not contain expected tables (bars or market_depth_quotes)"
    exit 1
fi

echo "Database Type: $DB_TYPE"
echo "Table Name: $TABLE_NAME"
echo

# Check if the table has the expected columns
COLUMNS=$(sqlite3 "$DB_FILE" "PRAGMA table_info($TABLE_NAME);" | awk -F'|' '{print $2}')

if ! echo "$COLUMNS" | grep -q stockTicker; then
    echo "Error: This database does not appear to be a $DB_TYPE database. The '$TABLE_NAME' table is missing the 'stockTicker' column."
    echo "This might be a BarCache database instead. The script is designed for LiveBarsDB and LiveMarketDepthQuoteDB."
    exit 1
fi

# Get total number of stocks
TOTAL_STOCKS=$(sqlite3 "$DB_FILE" "SELECT COUNT(DISTINCT stockTicker) FROM $TABLE_NAME;")
echo "Total number of stocks: $TOTAL_STOCKS"
echo

# Get statistics for each stock
echo "Per-stock statistics:"
echo "Stock Ticker | Entries | Data Size (chars) | Sequence Numbers"
echo "-------------|---------|-------------------|------------------"

sqlite3 "$DB_FILE" "SELECT stockTicker, COUNT(*) as entries, SUM(LENGTH(jsonRawData)) as data_size, COUNT(DISTINCT stockTickerSeq) as seq_numbers FROM $TABLE_NAME GROUP BY stockTicker ORDER BY stockTicker;" | while IFS='|' read -r TICKER COUNT SIZE SEQ_COUNT; do
    printf "%-12s | %-7s | %-17s | %-16s\n" "$TICKER" "$COUNT" "$SIZE" "$SEQ_COUNT"
done

echo
echo "=== Summary ==="

# Overall statistics
TOTAL_ENTRIES=$(sqlite3 "$DB_FILE" "SELECT COUNT(*) FROM $TABLE_NAME;")
TOTAL_DATA_SIZE=$(sqlite3 "$DB_FILE" "SELECT SUM(LENGTH(jsonRawData)) FROM $TABLE_NAME;")
TOTAL_SEQ_NUMBERS=$(sqlite3 "$DB_FILE" "SELECT COUNT(DISTINCT stockTickerSeq) FROM $TABLE_NAME;")

echo "Total entries: $TOTAL_ENTRIES"
echo "Total data size: $TOTAL_DATA_SIZE characters"
echo "Total unique sequence numbers: $TOTAL_SEQ_NUMBERS"

# Database file size
DB_SIZE=$(stat -f%z "$DB_FILE" 2>/dev/null || stat -c%s "$DB_FILE")
DB_SIZE_KB=$((DB_SIZE / 1024))
echo "Database file size: $DB_SIZE_KB KB"

echo
echo "Analysis complete."