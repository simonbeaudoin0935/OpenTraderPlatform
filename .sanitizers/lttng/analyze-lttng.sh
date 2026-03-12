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

# ------------------------------------------------------------------
# 1. Top mmap/brk callers by TID — shows the runaway allocator
# ------------------------------------------------------------------
echo "--- Top mmap/brk/mremap callers by TID (highest = likely culprit) ---"
babeltrace2 "$TRACE_DIR" 2>/dev/null \
    | grep -E "syscall_entry_mmap|syscall_entry_brk|syscall_entry_mremap" \
    | grep -oP 'tid = \d+' \
    | sort | uniq -c | sort -rn | head -20
echo ""

# ------------------------------------------------------------------
# 2. Thread name → TID mapping from scheduler events
# ------------------------------------------------------------------
echo "--- Thread name → TID mapping (from sched_switch) ---"
babeltrace2 "$TRACE_DIR" 2>/dev/null \
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
babeltrace2 "$TRACE_DIR" 2>/dev/null \
    | grep -E "syscall_entry_mmap|syscall_entry_brk" \
    | grep -oP '\[\d{2}:\d{2}:\d{2}' \
    | tr -d '[' \
    | sort | uniq -c | sort -rn | head -20
echo ""

# ------------------------------------------------------------------
# 4. Cross-reference: busy TIDs with their thread names
# ------------------------------------------------------------------
echo "--- Cross-reference: top allocator TIDs with thread names ---"
TOP_TIDS=$(babeltrace2 "$TRACE_DIR" 2>/dev/null \
    | grep -E "syscall_entry_mmap|syscall_entry_brk|syscall_entry_mremap" \
    | grep -oP 'tid = \d+' \
    | sort | uniq -c | sort -rn | head -5 \
    | awk '{print $2}' | grep -oP '\d+')

if [ -n "$TOP_TIDS" ]; then
    for TID in $TOP_TIDS; do
        COUNT=$(babeltrace2 "$TRACE_DIR" 2>/dev/null \
            | grep -E "syscall_entry_mmap|syscall_entry_brk|syscall_entry_mremap" \
            | grep "tid = ${TID}" | wc -l)
        NAME=$(babeltrace2 "$TRACE_DIR" 2>/dev/null \
            | grep "sched_switch" \
            | grep "next_tid = ${TID}" \
            | grep -oP 'next_comm = "[^"]*"' \
            | sort -u | head -1 \
            | sed 's/next_comm = //')
        echo "  TID ${TID} (${NAME:-unknown}): ${COUNT} alloc calls"
    done
fi
echo ""

echo "=== Analysis complete ==="
echo "For full trace: babeltrace2 $TRACE_DIR | less"
