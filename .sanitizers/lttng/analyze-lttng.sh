#!/bin/bash
# Analyze an LTTng trace captured with run-with-lttng.sh.
#
# Usage:
#   .sanitizers/lttng/analyze-lttng.sh [trace-dir]
#
# If no trace-dir is given, uses the most recent capture under
#   ~/.local/share/L2Trader/lttng-traces/

set -euo pipefail

TRACE_DIR="${1:-}"
if [ -z "$TRACE_DIR" ]; then
    TRACE_DIR="$(ls -td "${HOME}/.local/share/L2Trader/lttng-traces"/*/ 2>/dev/null | head -1)"
fi

if [ -z "$TRACE_DIR" ] || [ ! -d "$TRACE_DIR" ]; then
    echo "ERROR: No trace directory found."
    echo "Usage: $0 <trace-dir>"
    exit 1
fi

if ! command -v babeltrace2 &>/dev/null; then
    echo "ERROR: babeltrace2 not found."
    echo "Install with: sudo apt install babeltrace2"
    exit 1
fi

echo "=== LTTng Trace Analysis ==="
echo "Trace: $TRACE_DIR"
echo ""

# Support both old flat layout and new kernel/ subdirectory layout
KERNEL_DIR="${TRACE_DIR}/kernel"
if [ ! -d "$KERNEL_DIR" ]; then
    KERNEL_DIR="$TRACE_DIR"
fi
UST_DIR="${TRACE_DIR}/ust"

# ------------------------------------------------------------------
# 1. Top mmap/brk callers by TID — shows the runaway allocator
# ------------------------------------------------------------------
echo "--- Top mmap/brk/mremap callers by TID (highest = likely culprit) ---"
babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep -E "syscall_entry_mmap|syscall_entry_brk|syscall_entry_mremap" \
    | grep -oP 'tid = \d+' \
    | sort | uniq -c | sort -rn | head -20
echo ""

# ------------------------------------------------------------------
# 2. Thread name → TID mapping from scheduler events
# ------------------------------------------------------------------
echo "--- Thread name → TID mapping (from sched_switch) ---"
babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep "sched_switch" \
    | grep -oP 'next_comm = "[^"]*", next_tid = \d+' \
    | sort -u \
    | sed 's/next_comm = //;s/next_tid = /  TID: /' \
    | head -40
echo ""

# ------------------------------------------------------------------
# 3. mmap call rate per second — shows when the explosion starts
# ------------------------------------------------------------------
echo "--- mmap+brk call rate per second (top 20 busiest seconds) ---"
babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep -E "syscall_entry_mmap|syscall_entry_brk" \
    | grep -oP '\[\d{2}:\d{2}:\d{2}' \
    | tr -d '[' \
    | sort | uniq -c | sort -rn | head -20
echo ""

# ------------------------------------------------------------------
# 4. Cross-reference: busy TIDs with their thread names
# ------------------------------------------------------------------
echo "--- Cross-reference: top allocator TIDs with thread names ---"
TOP_TIDS=$(babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep -E "syscall_entry_mmap|syscall_entry_brk|syscall_entry_mremap" \
    | grep -oP 'tid = \d+' \
    | sort | uniq -c | sort -rn | head -5 \
    | awk '{print $2}' | grep -oP '\d+')

if [ -n "$TOP_TIDS" ]; then
    for TID in $TOP_TIDS; do
        COUNT=$(babeltrace2 "$KERNEL_DIR" 2>/dev/null \
            | grep -E "syscall_entry_mmap|syscall_entry_brk|syscall_entry_mremap" \
            | grep "tid = ${TID}" | wc -l)
        NAME=$(babeltrace2 "$KERNEL_DIR" 2>/dev/null \
            | grep "sched_switch" \
            | grep "next_tid = ${TID}" \
            | grep -oP 'next_comm = "[^"]*"' \
            | sort -u | head -1 \
            | sed 's/next_comm = //')
        echo "  TID ${TID} (${NAME:-unknown}): ${COUNT} alloc calls"
    done
fi
echo ""

# ------------------------------------------------------------------
# 5. LTTng UST tracepoints (only present in instrumented build)
# ------------------------------------------------------------------
UST_EVENTS=0
if [ -d "$UST_DIR" ]; then
    UST_EVENTS=$(babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:" || true)
fi
if [ "${UST_EVENTS}" -gt 0 ] 2>/dev/null; then
    echo "--- UST: BarCache API fetches (barcache_api_fetch_start) ---"
    babeltrace2 "$UST_DIR" 2>/dev/null \
        | grep "l2trader:barcache_api_fetch_start" \
        | grep -oP 'symbol = "[^"]*", tf_seconds = \d+, date = "[^"]*"' \
        | sort | uniq -c | sort -rn | head -20
    echo ""

    echo "--- UST: BarCache day-vector allocations (barcache_day_alloc) ---"
    babeltrace2 "$UST_DIR" 2>/dev/null \
        | grep "l2trader:barcache_day_alloc" \
        | grep -oP 'symbol = "[^"]*", tf_seconds = \d+, date = "[^"]*", num_slots = \d+' \
        | sort | uniq -c | sort -rn | head -20
    echo ""

    echo "--- UST: DB completeness misses (db_completeness_miss) ---"
    babeltrace2 "$UST_DIR" 2>/dev/null \
        | grep "l2trader:db_completeness_miss" \
        | grep -oP 'symbol = "[^"]*".*expected = \d+, got = \d+' \
        | sort | uniq -c | sort -rn | head -20
    echo ""

    echo "--- UST: live bar store rate (barcache_store_bar_live) ---"
    babeltrace2 "$UST_DIR" 2>/dev/null \
        | grep "l2trader:barcache_store_bar_live" \
        | grep -oP 'tf_seconds = \d+' \
        | sort | uniq -c | sort -rn | head -10
    echo ""
else
    echo "--- UST tracepoints: not present (rerun with run-lttng-instrumented task) ---"
    echo ""
fi

echo "=== Analysis complete ==="
echo "TraceCompass: open $TRACE_DIR/kernel  (kernel events)"
if [ -d "$UST_DIR" ]; then
    echo "              open $UST_DIR  (UST tracepoints)"
fi
echo "CLI full dump: babeltrace2 $KERNEL_DIR | less"
