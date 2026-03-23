#!/bin/bash

set -euo pipefail
shopt -s nullglob

DATA_ROOT="${1:-$HOME/.local/share/L2Trader}"
ORDERS_ROOT="$DATA_ROOT/Orders"
POSITIONS_ROOT="$DATA_ROOT/Positions"
LEDGERS_ROOT="$DATA_ROOT/Ledgers"
FAILURES=0

sqlite_escape() {
    printf "%s" "$1" | sed "s/'/''/g"
}

table_exists() {
    local db_file="$1"
    local table_name="$2"
    [ "$(sqlite3 "$db_file" "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='$table_name';")" -gt 0 ]
}

require_sqlite_db() {
    local db_file="$1"
    sqlite3 "$db_file" ".schema" >/dev/null 2>&1
}

delete_legacy_pair() {
    local orders_db="$1"
    local positions_db="$2"

    rm -f "$orders_db" "$positions_db"
}

migrate_pair() {
    local label="$1"
    local orders_db="$2"
    local positions_db="$3"
    local output_db="$4"

    echo "=== Migrating: $label ==="
    echo "  Orders DB:    $orders_db"
    echo "  Positions DB: $positions_db"
    echo "  Output DB:    $output_db"

    if [ ! -f "$orders_db" ] || [ ! -f "$positions_db" ]; then
        echo "  ERROR: Missing legacy orders/positions pair."
        echo ""
        return 1
    fi

    if ! require_sqlite_db "$orders_db" || ! require_sqlite_db "$positions_db"; then
        echo "  ERROR: One of the input files is not a valid SQLite database."
        echo ""
        return 1
    fi

    if ! table_exists "$orders_db" "orders"; then
        echo "  ERROR: Legacy orders DB is missing the orders table."
        echo ""
        return 1
    fi

    if ! table_exists "$positions_db" "positions"; then
        echo "  ERROR: Legacy positions DB is missing the positions table."
        echo ""
        return 1
    fi

    local orders_count
    local positions_count
    local strategy_logs_count=0
    orders_count="$(sqlite3 "$orders_db" "SELECT COUNT(*) FROM orders;")"
    positions_count="$(sqlite3 "$positions_db" "SELECT COUNT(*) FROM positions;")"
    if table_exists "$orders_db" "strategy_logs"; then
        strategy_logs_count="$(sqlite3 "$orders_db" "SELECT COUNT(*) FROM strategy_logs;")"
    fi

    mkdir -p "$(dirname "$output_db")"
    rm -f "$output_db"

    local orders_db_sql
    local positions_db_sql
    orders_db_sql="$(sqlite_escape "$orders_db")"
    positions_db_sql="$(sqlite_escape "$positions_db")"

    local migration_sql="
        ATTACH '${orders_db_sql}' AS orders_src;
        ATTACH '${positions_db_sql}' AS positions_src;
        CREATE TABLE orders AS SELECT * FROM orders_src.orders;
        CREATE TABLE positions AS SELECT * FROM positions_src.positions;
    "

    if table_exists "$orders_db" "strategy_logs"; then
        migration_sql+="CREATE TABLE strategy_logs AS SELECT * FROM orders_src.strategy_logs;"
    else
        migration_sql+="
            CREATE TABLE strategy_logs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                strategy_id TEXT NOT NULL,
                symbol TEXT NOT NULL,
                timestamp TEXT NOT NULL,
                message TEXT NOT NULL
            );
        "
    fi

    migration_sql+="DETACH orders_src; DETACH positions_src;"

    if ! sqlite3 "$output_db" "$migration_sql"; then
        echo "  ERROR: Failed to create combined ledger."
        rm -f "$output_db"
        echo ""
        return 1
    fi

    local migrated_orders_count
    local migrated_positions_count
    local migrated_strategy_logs_count
    migrated_orders_count="$(sqlite3 "$output_db" "SELECT COUNT(*) FROM orders;")"
    migrated_positions_count="$(sqlite3 "$output_db" "SELECT COUNT(*) FROM positions;")"
    migrated_strategy_logs_count="$(sqlite3 "$output_db" "SELECT COUNT(*) FROM strategy_logs;")"

    echo "  Verification:"
    echo "    orders:        $orders_count -> $migrated_orders_count"
    echo "    positions:     $positions_count -> $migrated_positions_count"
    echo "    strategy_logs: $strategy_logs_count -> $migrated_strategy_logs_count"

    if [ "$orders_count" != "$migrated_orders_count" ] ||
       [ "$positions_count" != "$migrated_positions_count" ] ||
       [ "$strategy_logs_count" != "$migrated_strategy_logs_count" ]; then
        echo "  ERROR: Row-count verification failed. Keeping legacy files."
        rm -f "$output_db"
        echo ""
        return 1
    fi

    delete_legacy_pair "$orders_db" "$positions_db"
    echo "  Legacy files deleted."
    echo ""
    return 0
}

migrate_mode_if_present() {
    local label="$1"
    local orders_db="$2"
    local positions_db="$3"
    local output_db="$4"

    if [ ! -f "$orders_db" ] && [ ! -f "$positions_db" ]; then
        echo "=== Skipping: $label (no legacy files found) ==="
        echo ""
        return 0
    fi

    migrate_pair "$label" "$orders_db" "$positions_db" "$output_db"
}

migrate_replay_sessions() {
    local orders_files=("$ORDERS_ROOT/Replay"/Orders_*.db)
    local positions_files=("$POSITIONS_ROOT/Replay"/Positions_*.db)

    if [ "${#orders_files[@]}" -eq 0 ] && [ "${#positions_files[@]}" -eq 0 ]; then
        echo "=== Skipping: Replay sessions (no legacy replay ledgers found) ==="
        echo ""
        return 0
    fi

    for orders_db in "${orders_files[@]}"; do
        local session_id
        session_id="$(basename "$orders_db" .db)"
        session_id="${session_id#Orders_}"
        local positions_db="$POSITIONS_ROOT/Replay/Positions_${session_id}.db"
        if [ ! -f "$positions_db" ]; then
            echo "ERROR: Replay session $session_id is missing $positions_db"
            return 1
        fi
    done

    for positions_db in "${positions_files[@]}"; do
        local session_id
        session_id="$(basename "$positions_db" .db)"
        session_id="${session_id#Positions_}"
        local orders_db="$ORDERS_ROOT/Replay/Orders_${session_id}.db"
        if [ ! -f "$orders_db" ]; then
            echo "ERROR: Replay session $session_id is missing $orders_db"
            return 1
        fi
    done

    for orders_db in "${orders_files[@]}"; do
        local session_id
        session_id="$(basename "$orders_db" .db)"
        session_id="${session_id#Orders_}"
        local positions_db="$POSITIONS_ROOT/Replay/Positions_${session_id}.db"
        local output_db="$LEDGERS_ROOT/Replay/Ledger_${session_id}.db"
        migrate_pair "Replay ${session_id}" "$orders_db" "$positions_db" "$output_db" || return 1
    done
}

cleanup_empty_legacy_dirs() {
    find "$ORDERS_ROOT" "$POSITIONS_ROOT" -type d -empty -delete 2>/dev/null || true
}

echo "L2Trader ledger migration"
echo "  Data root: $DATA_ROOT"
echo ""

mkdir -p "$LEDGERS_ROOT"

if ! migrate_mode_if_present "Live" \
    "$ORDERS_ROOT/Live/Orders.db" \
    "$POSITIONS_ROOT/Live/Positions.db" \
    "$LEDGERS_ROOT/Live/Ledger.db"; then
    FAILURES=$((FAILURES + 1))
fi

if ! migrate_mode_if_present "Simulation" \
    "$ORDERS_ROOT/Simulation/Orders.db" \
    "$POSITIONS_ROOT/Simulation/Positions.db" \
    "$LEDGERS_ROOT/Simulation/Ledger.db"; then
    FAILURES=$((FAILURES + 1))
fi

if ! migrate_replay_sessions; then
    FAILURES=$((FAILURES + 1))
fi

cleanup_empty_legacy_dirs

if [ "$FAILURES" -gt 0 ]; then
    echo "Migration completed with $FAILURES failure(s)."
    exit 1
fi

echo "Migration complete."
