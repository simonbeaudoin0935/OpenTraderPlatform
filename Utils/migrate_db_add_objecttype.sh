#!/bin/bash

# Migration script: Add objectType column to bars and market_depth_quotes tables
#
# Upgrades existing L2Trader recording databases from the old schema
# (id, stockTicker, stockTickerSeq, epochMs, jsonRawData) to the new schema
# that adds an objectType column classifying each row as the primary data type
# ('Bar' or 'MarketDepthQuote'), 'Heartbeat', or 'Error'.
#
# The classification is done by inspecting the stored JSON:
#   - Valid JSON with a 'Heartbeat' field → 'Heartbeat'
#   - Valid JSON with an 'Error' field     → 'Error'
#   - Anything else (normal data or unparseable fragment) → 'Bar'/'MarketDepthQuote'
#
# Usage:
#   ./migrate_db_add_objecttype.sh <database_file.db>
#   ./migrate_db_add_objecttype.sh /path/to/RecordedLiveData/Bars/
#
# When given a directory, all *.db files inside are migrated.
#
# The script is safe to run multiple times (idempotent).

if [ $# -ne 1 ]; then
    echo "Usage: $0 <database_file.db | directory>"
    echo ""
    echo "Examples:"
    echo "  $0 2025-11-17.db"
    echo "  $0 ~/.cache/L2Trader/RecordedLiveData/Bars/"
    echo "  $0 ~/.cache/L2Trader/RecordedLiveData/MarketDepthQuotes/"
    exit 1
fi

TARGET="$1"
FAILURES=0

# ─── Helpers ─────────────────────────────────────────────────────────────────

migrate_file() {
    local DB_FILE="$1"

    echo "=== Migrating: $DB_FILE ==="

    if [ ! -f "$DB_FILE" ]; then
        echo "  ERROR: File does not exist — skipping."
        echo ""
        return 1
    fi

    if ! sqlite3 "$DB_FILE" ".schema" >/dev/null 2>&1; then
        echo "  ERROR: Not a valid SQLite database — skipping."
        echo ""
        return 1
    fi

    # Determine which table this database contains
    local TABLE_NAME=""
    local DEFAULT_TYPE=""

    if sqlite3 "$DB_FILE" "SELECT name FROM sqlite_master WHERE type='table' AND name='bars';" | grep -q bars; then
        TABLE_NAME="bars"
        DEFAULT_TYPE="Bar"
    elif sqlite3 "$DB_FILE" "SELECT name FROM sqlite_master WHERE type='table' AND name='market_depth_quotes';" | grep -q market_depth_quotes; then
        TABLE_NAME="market_depth_quotes"
        DEFAULT_TYPE="MarketDepthQuote"
    else
        echo "  ERROR: No recognized table (bars / market_depth_quotes) found — skipping."
        echo ""
        return 1
    fi

    # Sanity-check that it's a recorder DB (must have stockTicker column)
    local HAS_TICKER
    HAS_TICKER=$(sqlite3 "$DB_FILE" \
        "SELECT COUNT(*) FROM pragma_table_info('$TABLE_NAME') WHERE name='stockTicker';")
    if [ "$HAS_TICKER" -eq 0 ]; then
        echo "  ERROR: '$TABLE_NAME' is missing the stockTicker column — not a recorder database, skipping."
        echo ""
        return 1
    fi

    # Check if the migration was already applied
    local HAS_COL
    HAS_COL=$(sqlite3 "$DB_FILE" \
        "SELECT COUNT(*) FROM pragma_table_info('$TABLE_NAME') WHERE name='objectType';")
    if [ "$HAS_COL" -gt 0 ]; then
        echo "  Already migrated (objectType column exists) — skipping."
        echo ""
        return 0
    fi

    local TOTAL_ROWS
    TOTAL_ROWS=$(sqlite3 "$DB_FILE" "SELECT COUNT(*) FROM $TABLE_NAME;")
    echo "  Table:       $TABLE_NAME"
    echo "  Total rows:  $TOTAL_ROWS"

    echo "  Adding objectType column..."
    if ! sqlite3 "$DB_FILE" "ALTER TABLE $TABLE_NAME ADD COLUMN objectType TEXT;"; then
        echo "  ERROR: Failed to add objectType column."
        echo ""
        return 1
    fi

    echo "  Classifying rows..."
    # Use SQLite's json_valid() + json_extract() to determine type:
    #   - Valid JSON with a 'Heartbeat' field → 'Heartbeat'
    #   - Valid JSON with an 'Error' field     → 'Error'
    #   - Everything else (normal data or unparseable fragment) → DEFAULT_TYPE
    if ! sqlite3 "$DB_FILE" "
        UPDATE $TABLE_NAME SET objectType =
            CASE
                WHEN json_valid(jsonRawData) AND json_extract(jsonRawData, '$.Heartbeat') IS NOT NULL THEN 'Heartbeat'
                WHEN json_valid(jsonRawData) AND json_extract(jsonRawData, '$.Error')     IS NOT NULL THEN 'Error'
                ELSE '$DEFAULT_TYPE'
            END;
    "; then
        echo "  ERROR: Failed to classify rows."
        echo ""
        return 1
    fi

    # Report breakdown
    echo "  Classification results:"
    sqlite3 "$DB_FILE" \
        "SELECT objectType, COUNT(*) FROM $TABLE_NAME GROUP BY objectType ORDER BY COUNT(*) DESC;" \
        | while IFS='|' read -r TYPE COUNT; do
            printf "    %-20s %s rows\n" "$TYPE" "$COUNT"
        done

    echo "  Migration complete."
    echo ""
    return 0
}

# ─── Main ─────────────────────────────────────────────────────────────────────

if [ -d "$TARGET" ]; then
    # Collect .db files; avoid glob expansion when no files match
    DB_FILES=()
    while IFS= read -r -d '' f; do
        DB_FILES+=("$f")
    done < <(find "$TARGET" -maxdepth 1 -name "*.db" -print0 | sort -z)

    if [ ${#DB_FILES[@]} -eq 0 ]; then
        echo "No .db files found in '$TARGET'"
        exit 1
    fi

    echo "Found ${#DB_FILES[@]} database(s) in '$TARGET'"
    echo ""

    for DB_FILE in "${DB_FILES[@]}"; do
        if ! migrate_file "$DB_FILE"; then
            FAILURES=$((FAILURES + 1))
        fi
    done
else
    if ! migrate_file "$TARGET"; then
        FAILURES=$((FAILURES + 1))
    fi
fi

if [ "$FAILURES" -gt 0 ]; then
    echo "Completed with $FAILURES failure(s)."
    exit 1
else
    echo "All done."
fi
