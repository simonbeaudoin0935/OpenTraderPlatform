#!/bin/bash
# Run L2Trader with LTTng kernel tracing for memory/thread diagnostics.
#
# Usage:
#   .sanitizers/lttng/run-with-lttng.sh
#
# Prerequisites:
#   sudo apt install lttng-tools lttng-modules-dkms babeltrace2
#
# What it traces (kernel-level, no code changes required):
#   - mmap / brk syscalls  → heap growth events
#   - sched_switch         → which thread is active at each instant
#   - sched_process_fork   → new thread spawns
#
# After the app exits (or Ctrl-C), run:
#   .sanitizers/lttng/analyze-lttng.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Allow caller to override the binary (e.g., for instrumented build/LTTng)
APP="${APP:-${SCRIPT_DIR}/../../build/GUI/Src/L2Trader}"
TRACE_BASE="${HOME}/.local/share/L2Trader/lttng-traces"
SESSION="l2trader-oom-hunt"
TRACE_DIR="${TRACE_BASE}/$(date +%Y-%m-%d_%H-%M-%S)"

if ! command -v lttng &>/dev/null; then
    echo "ERROR: lttng-tools not found."
    echo "Install with: sudo apt install lttng-tools lttng-modules-dkms babeltrace2"
    exit 1
fi

if [ ! -x "$APP" ]; then
    echo "ERROR: App not found at $APP — build first."
    exit 1
fi

mkdir -p "$TRACE_BASE"

cleanup() {
    echo ""
    echo "=== Stopping LTTng session... ==="
    sudo lttng stop "$SESSION" 2>/dev/null || true
    sudo lttng destroy "$SESSION" 2>/dev/null || true
    echo ""
    echo "Trace saved to: $TRACE_DIR"
    echo "Analyze with:   ${SCRIPT_DIR}/analyze-lttng.sh $TRACE_DIR"
}
trap cleanup EXIT INT TERM

echo "=== LTTng OOM Hunt: L2Trader ==="
echo "Trace output: $TRACE_DIR"
echo ""

# Destroy any stale session with the same name
sudo lttng destroy "$SESSION" 2>/dev/null || true

sudo lttng create "$SESSION" --output="$TRACE_DIR"

# Heap growth: mmap and brk are the two syscalls that grow the heap/address space
sudo lttng enable-event --kernel --syscall mmap,brk,mremap

# Thread scheduling: correlates TID → thread name and shows which thread is hot
sudo lttng enable-event --kernel 'sched_switch,sched_process_fork,sched_process_exec'

sudo lttng start

echo "LTTng recording started."
echo "  > App binary: $APP"
if [[ "$APP" == *"/LTTng/"* ]]; then
    echo "  > UST tracepoints: ACTIVE (instrumented build)"
    # Enable userspace tracepoints from the l2trader provider
    sudo lttng enable-event --userspace 'l2trader:*' 2>/dev/null || true
else
    echo "  > UST tracepoints: inactive (use build-with-lttng task for instrumented binary)"
fi
echo "  > Switch to 10s timescale, enter replay mode, hit Play to reproduce OOM"
echo "  > Close the app (or Ctrl-C here) to stop tracing"
echo ""

"$APP"
