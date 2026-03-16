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
if [[ "$APP" == *"/LTTng"* ]]; then
    IS_INSTRUMENTED=true
fi

# Detect flamegraph build: check if binary was compiled with -finstrument-functions
# by looking for the LTTNG_FLAMEGRAPH symbol in the binary
FLAMEGRAPH_ENABLED="${FLAMEGRAPH:-false}"
if $IS_INSTRUMENTED && nm "$APP" 2>/dev/null | grep -q "__cyg_profile_func_enter" ; then
    FLAMEGRAPH_ENABLED=true
fi

mkdir -p "$TRACE_DIR"

cleanup() {
    echo ""
    echo "=== Stopping LTTng sessions... ==="
    if [ -n "${APP_PID:-}" ] && kill -0 "$APP_PID" 2>/dev/null; then
        kill "$APP_PID" 2>/dev/null || true
        wait "$APP_PID" 2>/dev/null || true
    fi
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
        if $FLAMEGRAPH_ENABLED; then
            echo "            + function call-stack (flamegraph) data"
        fi
    fi
    echo ""
    echo "Analyze with: ${SCRIPT_DIR}/analyze-lttng.sh $TRACE_DIR"
}
APP_PID=""
trap cleanup EXIT INT TERM

echo "=== LTTng Trace: L2Trader ==="
echo "Trace output : $TRACE_DIR"
echo "App binary   : $APP"
echo "UST events   : $(${IS_INSTRUMENTED} && echo 'ACTIVE (instrumented build)' || echo 'inactive — use build-with-lttng task')"
echo "Flamegraph   : $(${FLAMEGRAPH_ENABLED} && echo 'ACTIVE (cyg-profile (standard) + -finstrument-functions)' || echo 'inactive — build with -DLTTNG_FLAMEGRAPH=ON')"
echo ""

# ------------------------------------------------------------------
# Kernel session (root) — comprehensive tracing for thread & system analysis
# ------------------------------------------------------------------
sudo lttng destroy "$KERNEL_SESSION" 2>/dev/null || true
sudo lttng create "$KERNEL_SESSION" --output="${TRACE_DIR}/kernel"

# ────────────────────────────────────────────────────────────────────────────
# THREAD & PROCESS LIFECYCLE
# ────────────────────────────────────────────────────────────────────────────
# sched_switch:   Context switch (CPU scheduling) — shows thread activity & blocking
# sched_process_fork: Process/thread creation — maps parent→child TIDs
# sched_process_exec: Process exec — identifies thread startup points
# sched_process_exit: Process/thread termination — shows lifecycle end
# sched_wakeup:    Thread wake events — shows inter-thread dependencies
sudo lttng enable-event --kernel 'sched_switch,sched_process_fork,sched_process_exec,sched_process_exit,sched_wakeup'

# ────────────────────────────────────────────────────────────────────────────
# MEMORY ALLOCATION & PAGE MANAGEMENT
# ────────────────────────────────────────────────────────────────────────────
# kmem_mm_page_alloc: Physical page allocations — populates TraceCompass "Memory Usage" view
# kmem_mm_page_free:  Page deallocations
# brk,mmap,mremap:   Heap & virtual memory syscalls — shows heap growth patterns
sudo lttng enable-event --kernel --syscall 'brk,mmap,mmap2,mremap,munmap,sbrk'
sudo lttng enable-event --kernel 'kmem_mm_page_alloc,kmem_mm_page_free'

# ────────────────────────────────────────────────────────────────────────────
# SYNCHRONIZATION & BLOCKING (mutex, futex, condvar)
# ────────────────────────────────────────────────────────────────────────────
# futex_wait/futex_wake: Qt's QMutex uses futex internally — shows lock contention
# pthread_mutex_*:       Explicit mutex events (if enabled)
sudo lttng enable-event --kernel 'syscalls:sys_enter_futex,syscalls:sys_exit_futex'

# ────────────────────────────────────────────────────────────────────────────
# TIMER & SIGNAL HANDLING
# ────────────────────────────────────────────────────────────────────────────
# timer_init/timer_start/timer_expire:  Kernel timer events
# signal_deliver:       Signal delivery (SIGSEGV, SIGABRT for crash handler)
sudo lttng enable-event --kernel 'timer_init,timer_start,timer_cancel,timer_expire' 2>/dev/null || true
sudo lttng enable-event --kernel 'signal_deliver' 2>/dev/null || true

# ────────────────────────────────────────────────────────────────────────────
# I/O OPERATIONS (file descriptors, network)
# ────────────────────────────────────────────────────────────────────────────
# syscalls:sys_enter/exit for read/write/poll — identifies I/O bottlenecks
sudo lttng enable-event --kernel --syscall 'read,write,pread64,pwrite64,readv,writev,poll,select,epoll_wait' 2>/dev/null || true
sudo lttng enable-event --kernel --syscall 'open,close,openat,socket,accept,connect' 2>/dev/null || true

# ────────────────────────────────────────────────────────────────────────────
# CPU TRACING (IRQ, softIRQ)
# ────────────────────────────────────────────────────────────────────────────
# irq_handler_entry/exit:  Hardware interrupt context switches
# softirq_entry/exit:      Software interrupt (networking, timers)
sudo lttng enable-event --kernel 'irq_handler_entry,irq_handler_exit' 2>/dev/null || true
sudo lttng enable-event --kernel 'softirq_entry,softirq_exit' 2>/dev/null || true

# Start the kernel session
sudo lttng start "$KERNEL_SESSION"
echo "[kernel session] started with comprehensive thread/memory/I/O tracing"

# ------------------------------------------------------------------
# UST session (user) — l2trader:* tracepoints + optional flamegraph
# Must be created and started BEFORE the app launches so the app can
# register its probe with the user-level sessiond on startup.
# ------------------------------------------------------------------
lttng destroy "$UST_SESSION" 2>/dev/null || true
lttng create "$UST_SESSION" --output="${TRACE_DIR}/ust"

if $IS_INSTRUMENTED; then
    lttng enable-event --userspace 'l2trader:*'

    if $FLAMEGRAPH_ENABLED; then
        # Use the standard (non-fast) library — provider lttng_ust_cyg_profile.
        # TraceCompass's built-in CallStack analysis only recognises this provider.
        # The -fast variant (lttng_ust_cyg_profile_fast) produces 0 stack frames in TraceCompass.
        lttng enable-event --userspace 'lttng_ust_cyg_profile:func_entry'
        lttng enable-event --userspace 'lttng_ust_cyg_profile:func_exit'
        lttng disable-event --userspace 'lttng_ust_cyg_profile:func_entry'
        lttng disable-event --userspace 'lttng_ust_cyg_profile:func_exit'
        echo "[ust session] started with l2trader:* events (flamegraph DEFERRED — press ENTER to activate)"
    else
        echo "[ust session] started with l2trader:* events"
    fi
else
    # Enable a benign no-op event so the session starts cleanly
    lttng enable-event --userspace --all 2>/dev/null || true
    echo "[ust session] started (no UST events — non-instrumented build)"
fi

# Add UST contexts needed for call-stack analysis and thread identification
lttng add-context --userspace --type vpid    2>/dev/null || true
lttng add-context --userspace --type vtid    2>/dev/null || true
lttng add-context --userspace --type procname 2>/dev/null || true

lttng start "$UST_SESSION"

echo ""
echo "Recording. Reproduce the issue:"
echo "  1. Enter replay mode"
echo "  2. Hit Play"
echo "  3. Close the app (or Ctrl-C here) when done"
if $FLAMEGRAPH_ENABLED; then
    echo ""
    echo "  Flamegraph: DEFERRED — press ENTER in this terminal to start recording"
    echo "              (wait until the app is loaded and replay is ready)"
    echo "              Press ENTER again to pause, and again to resume."
fi
echo ""

if $FLAMEGRAPH_ENABLED; then
    # Launch in background — redirect output so app logs don't block stdin.
    # Logs already go to ~/.local/state/L2Trader/AppLogs/ so nothing is lost.
    APP_LOG="${TRACE_DIR}/app_console.log"
    LD_PRELOAD=liblttng-ust-cyg-profile.so "$APP" >"$APP_LOG" 2>&1 &
    APP_PID=$!
    echo "[app] PID $APP_PID — console output redirected to $APP_LOG"

    # Toggle flamegraph on/off with ENTER keypresses until the app exits
    FLAMEGRAPH_ACTIVE=false
    while kill -0 "$APP_PID" 2>/dev/null; do
        if read -r -t 1; then
            if $FLAMEGRAPH_ACTIVE; then
                lttng disable-event --session "$UST_SESSION" --userspace 'lttng_ust_cyg_profile:func_entry'
                lttng disable-event --session "$UST_SESSION" --userspace 'lttng_ust_cyg_profile:func_exit'
                FLAMEGRAPH_ACTIVE=false
                echo "[flamegraph] Recording PAUSED — press ENTER to resume"
            else
                lttng enable-event --session "$UST_SESSION" --userspace 'lttng_ust_cyg_profile:func_entry'
                lttng enable-event --session "$UST_SESSION" --userspace 'lttng_ust_cyg_profile:func_exit'
                FLAMEGRAPH_ACTIVE=true
                echo "[flamegraph] Recording ENABLED — press ENTER to pause"
            fi
        fi
    done

    wait "$APP_PID" 2>/dev/null || true
else
    "$APP"
fi
