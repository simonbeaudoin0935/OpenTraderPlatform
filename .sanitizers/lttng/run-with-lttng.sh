#!/bin/bash
# Run L2Trader with LTTng kernel + UST tracing for memory/thread diagnostics.
#
# Usage:
#   .sanitizers/lttng/run-with-lttng.sh
#
# Prerequisites:
#   sudo apt install lttng-tools lttng-modules-dkms babeltrace2
#
# Two parallel LTTng sessions are created:
#   [kernel]  - run as root  → syscalls, scheduler, memory page allocs
#   [ust]     - run as user  → l2trader:* UST tracepoints (instrumented build only)
#
# Trace output (CTF, open directly in TraceCompass):
#   ~/.local/state/L2Trader/lttng-traces/<timestamp>/kernel/
#   ~/.local/state/L2Trader/lttng-traces/<timestamp>/ust/
#
# After the app exits (or Ctrl-C), run:
#   .sanitizers/lttng/analyze-lttng.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Allow caller to override the binary (e.g., for instrumented build/LTTng)
APP="${APP:-${SCRIPT_DIR}/../../build/GUI/Src/L2Trader}"
TRACE_BASE="${HOME}/.local/state/L2Trader/lttng-traces"
TRACE_DIR="${TRACE_BASE}/$(date +%Y-%m-%d_%H-%M-%S)"
KERNEL_SESSION="l2trader-kernel"
UST_SESSION="l2trader-ust"

if ! command -v lttng &>/dev/null; then
    echo "ERROR: lttng-tools not found."
    echo "Install with: sudo apt install lttng-tools lttng-modules-dkms babeltrace2"
    exit 1
fi

if [ ! -x "$APP" ]; then
    echo "ERROR: App not found at $APP — build first."
    exit 1
fi

IS_INSTRUMENTED=false
if [[ "$APP" == *"/LTTng/"* ]]; then
    IS_INSTRUMENTED=true
fi

mkdir -p "$TRACE_DIR"

cleanup() {
    echo ""
    echo "=== Stopping LTTng sessions... ==="
    sudo lttng stop  "$KERNEL_SESSION" 2>/dev/null || true
    sudo lttng destroy "$KERNEL_SESSION" 2>/dev/null || true
    lttng stop  "$UST_SESSION" 2>/dev/null || true
    lttng destroy "$UST_SESSION" 2>/dev/null || true
    # Kernel session writes as root — fix ownership so TraceCompass can open it
    sudo chown -R "${USER}:${USER}" "$TRACE_DIR" 2>/dev/null || true
    echo ""
    echo "Trace saved to: $TRACE_DIR"
    echo "  kernel/ : kernel events (open in TraceCompass)"
    if $IS_INSTRUMENTED; then
        echo "  ust/    : l2trader UST tracepoints"
    fi
    echo ""
    echo "Analyze with: ${SCRIPT_DIR}/analyze-lttng.sh $TRACE_DIR"
}
trap cleanup EXIT INT TERM

echo "=== LTTng OOM Hunt: L2Trader ==="
echo "Trace output : $TRACE_DIR"
echo "App binary   : $APP"
echo "UST events   : $(${IS_INSTRUMENTED} && echo 'ACTIVE (instrumented build)' || echo 'inactive — use build-with-lttng task')"
echo ""

# ------------------------------------------------------------------
# Kernel session (root) — syscalls + scheduler + memory page allocs
# ------------------------------------------------------------------
sudo lttng destroy "$KERNEL_SESSION" 2>/dev/null || true
sudo lttng create "$KERNEL_SESSION" --output="${TRACE_DIR}/kernel"

# Heap growth syscalls
sudo lttng enable-event --kernel --syscall mmap,brk,mremap

# Thread scheduling — maps TID→name, shows hot threads in Control Flow view
sudo lttng enable-event --kernel 'sched_switch,sched_process_fork,sched_process_exec'

# Memory page allocs — populates TraceCompass "Memory Usage" view
sudo lttng enable-event --kernel 'kmem_mm_page_alloc,kmem_mm_page_free'

sudo lttng start "$KERNEL_SESSION"
echo "[kernel session] started"

# ------------------------------------------------------------------
# UST session (user) — l2trader:* tracepoints
# Must be created and started BEFORE the app launches so the app can
# register its probe with the user-level sessiond on startup.
# ------------------------------------------------------------------
lttng destroy "$UST_SESSION" 2>/dev/null || true
lttng create "$UST_SESSION" --output="${TRACE_DIR}/ust"

if $IS_INSTRUMENTED; then
    lttng enable-event --userspace 'l2trader:*'
    echo "[ust session] started with l2trader:* events"
else
    # Enable a benign no-op event so the session starts cleanly
    lttng enable-event --userspace --all 2>/dev/null || true
    echo "[ust session] started (no UST events — non-instrumented build)"
fi

lttng start "$UST_SESSION"

echo ""
echo "Recording. Reproduce the OOM:"
echo "  1. Switch chart to 10s timescale"
echo "  2. Enter replay mode"
echo "  3. Hit Play — watch memory climb"
echo "  4. Close the app (or Ctrl-C here) when done"
echo ""

"$APP"
