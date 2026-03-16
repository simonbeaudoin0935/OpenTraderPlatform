---
name: 'LTTng Trace Browser'
description: 'Skills for opening, querying, and interpreting LTTng kernel + UST traces captured from L2Trader to diagnose performance, memory, and threading issues.'
---

# Copilot Skills

This file teaches GitHub Copilot how to locate, open, and extract information from LTTng traces captured by the L2Trader project.

---

> ### ⚠️ Shell Security Policy — Mandatory Pattern Rules
>
> The shell security policy **blocks nested command substitution**: any `VAR=$(command | pipeline)` after the first `LATEST=` assignment will be rejected with *"contains dangerous shell expansion patterns"*.
>
> **Rule**: Never assign `$(...)` pipeline output to a variable. Run pipelines directly and let them print.
>
> ```bash
> # ✗ BLOCKED — nested $() assigned to variable
> HITS=$(babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:barcache_l1_hit" || true)
> echo "Hits: $HITS"
>
> # ✓ CORRECT — pipeline runs directly, output goes to stdout
> echo -n "Cache hits: " && babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:barcache_l1_hit" || true
> ```
>
> The **only** safe use of `$(...)` is the initial session path assignment:
> ```bash
> LATEST=$(ls -td ~/.local/state/L2Trader/lttng-traces/*/ | head -1)
> ```
> Everything after that must use direct pipelines.

---

## Skill: Open the Latest LTTng Trace Session

### Where are the traces?

Every run of `run-with-lttng.sh` (or the VSCode tasks `run-lttng` / `run-lttng-instrumented`) creates a timestamped directory under:

```
~/.local/state/L2Trader/lttng-traces/<YYYY-MM-DD_hh-mm-ss>/
```

Each session directory contains two sub-directories:

| Sub-directory | Contents |
|---|---|
| `kernel/` | Kernel-space events (syscalls, scheduler, memory page alloc/free) |
| `ust/` | Userspace tracepoints emitted by L2Trader (`l2trader:*` events) |

```bash
# Find the latest trace session (always use two-step pattern — no nested $(...))
LATEST=$(ls -td ~/.local/state/L2Trader/lttng-traces/*/ | head -1) && echo "Latest: $LATEST"

KERNEL_DIR="${LATEST}kernel"
UST_DIR="${LATEST}ust"
```

> **AI agent note**: Always use the two-step pattern — assign `LATEST` first, then reference `"$LATEST"`. Never nest `$(...)` inside another `$(...)` (blocked by shell security policy).

---

## Skill: Discover What Is in a Trace

Before querying, always start by discovering what event types are present:

```bash
LATEST=$(ls -td ~/.local/state/L2Trader/lttng-traces/*/ | head -1)

# All distinct kernel event types recorded
babeltrace2 "${LATEST}kernel" 2>/dev/null \
    | grep -oP 'name = "[^"]*"' | sort -u

# All distinct UST event types recorded
babeltrace2 "${LATEST}ust" 2>/dev/null \
    | grep -oP 'l2trader:\w+' | sort -u

# Total event counts per type (kernel)
babeltrace2 "${LATEST}kernel" 2>/dev/null \
    | grep -oP 'name = "[^"]*"' | sort | uniq -c | sort -rn | head -30

# Total event counts per type (UST)
babeltrace2 "${LATEST}ust" 2>/dev/null \
    | grep -oP 'l2trader:\w+' | sort | uniq -c | sort -rn

# Raw dump of the first 50 UST events (understand field names and structure)
babeltrace2 "${LATEST}ust" 2>/dev/null | head -100

# Time range of the trace
babeltrace2 "${LATEST}kernel" 2>/dev/null | head -3
babeltrace2 "${LATEST}kernel" 2>/dev/null | tail -3
```

---

## Skill: Query Kernel Events

### Thread scheduling

```bash
LATEST=$(ls -td ~/.local/state/L2Trader/lttng-traces/*/ | head -1)
KERNEL_DIR="${LATEST}kernel"

# All threads that were scheduled (unique TID → name mappings)
babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep "sched_switch" \
    | grep -oP 'next_comm = "[^"]*", next_tid = \d+' \
    | sort -u

# How much CPU time each thread got (rough — count of sched_switch to it)
babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep "sched_switch" \
    | grep -oP 'next_comm = "[^"]*"' \
    | sort | uniq -c | sort -rn | head -20

# Thread wakeup events (sched_process_fork = new thread spawned)
babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep "sched_process_fork" | head -20
```

### Memory (heap syscalls)

```bash
# Total mmap/brk/mremap calls per thread (higher = more allocations)
babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep -E "syscall_entry_mmap|syscall_entry_brk|syscall_entry_mremap" \
    | grep -oP 'tid = \d+' \
    | sort | uniq -c | sort -rn | head -20

# Resolve those TIDs to thread names
babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep "sched_switch" \
    | grep -oP 'next_comm = "[^"]*", next_tid = \d+' \
    | sort -u

# Allocation call rate per second (find *when* growth happened)
babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep -E "syscall_entry_mmap|syscall_entry_brk" \
    | grep -oP '\[\d{2}:\d{2}:\d{2}' \
    | tr -d '[' \
    | sort | uniq -c | sort -rn | head -20

# Physical page alloc/free balance (requires kmem events)
echo -n "Pages allocated: " && babeltrace2 "$KERNEL_DIR" 2>/dev/null | grep -c "kmem_mm_page_alloc" || true
echo -n "Pages freed:     " && babeltrace2 "$KERNEL_DIR" 2>/dev/null | grep -c "kmem_mm_page_free" || true
```

### System calls — general

```bash
# All syscall types observed (entry events only)
babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep "syscall_entry" \
    | grep -oP 'name = "[^"]*"' | sort | uniq -c | sort -rn | head -20

# Activity by a specific TID (replace 12345)
TID=12345
babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep "tid = ${TID}" | head -50
```

---

## Skill: Query UST Tracepoints

UST events are only present when the app was built with `LTTNG_ENABLED=ON` (VSCode task `build-with-lttng`) and launched via `run-lttng-instrumented`.

```bash
LATEST=$(ls -td ~/.local/state/L2Trader/lttng-traces/*/ | head -1)
UST_DIR="${LATEST}ust"

# Confirm UST events are present
babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:" || echo "0 — not instrumented build"
```

### Available `l2trader:*` events

The cache is **3-level**. Event names reflect which level fired:

| Level | Event | Meaning | Key fields |
|---|---|---|---|
| **L1 (RAM)** | `barcache_l1_hit` | Day found in `m_barCacheByTimeFrame` — no I/O | `symbol`, `tf_seconds`, `date`, `bars_returned` |
| **L1 (RAM)** | `barcache_l1_miss` | Day not in RAM — L2 (SQLite) will be checked next. Does **not** imply an API fetch. | `symbol`, `tf_seconds`, `date` |
| **L2 (SQLite)** | `barcache_l2_hit` | DB returned ≥90% of expected bars — loaded into L1 | `symbol`, `tf_seconds`, `date`, `bars_returned`, `expected` |
| **L2 (SQLite)** | `barcache_l2_miss` | DB returned 0 bars for that day — L3 (API) fetch will follow | `symbol`, `tf_seconds`, `date` |
| **L2 (SQLite)** | `barcache_l2_partial` | DB has some bars but below 90% threshold — L3 fetch will follow | `symbol`, `tf_seconds`, `date`, `expected`, `got` |
| **L3 (API)** | `barcache_api_fetch_start` | Databento historical bars request dispatched | `symbol`, `tf_seconds`, `date` |
| **L3 (API)** | `barcache_api_fetch_done` | Fetch returned | `symbol`, `tf_seconds`, `date`, `bars_count` |
| **Store** | `barcache_day_alloc` | New day-vector allocated in RAM | `symbol`, `tf_seconds`, `date`, `num_slots` |
| **Store** | `barcache_store_bar_live` | Live/replay bar slotted into L1 | `symbol`, `tf_seconds`, `index` |
| **Store** | `barcache_store_full_day` | Full day written to L1 (then DB) | `symbol`, `tf_seconds`, `date`, `bars_count` |
| **Store** | `barcache_store_partial` | Partial day slotted into pre-allocated day vector | `symbol`, `tf_seconds`, `date`, `bars_count`, `expected_slots` |
| **Gap fill** | `fillholes_run` | Gap-fill pass started after an L3 fetch | `symbol`, `tf_seconds`, `step_secs`, `bars_in`, `expected_slots` |
| **Gap fill** | `fillholes_done` | Gap-fill pass finished | `symbol`, `tf_seconds`, `bars_out`, `void_bars` |
| **Chart** | `chart_missing_bars_request` | Chart fired `requestMissingBars` | `symbol`, `tf_seconds`, `from`, `to` |
| **Live** | `livebar_closed` | A live bar interval completed | `symbol`, `interval_seconds` |
| **Actor model** | `symbolctx_enqueue` | Event pushed to SymbolContext queue | `symbol`, `event_type`, `queue_depth` |
| **Actor model** | `symbolctx_pool_submit` | QRunnable submitted to QThreadPool | `symbol` |
| **Actor model** | `symbolctx_drain_start` | Pool thread begins draining a symbol's queue | `symbol` |
| **Actor model** | `symbolctx_drain_end` | Pool thread finished draining | `symbol`, `items_processed` |
| **Actor model** | `symbolctx_process_level2` | About to process a Level2 event in drain loop | `symbol` |
| **Actor model** | `symbolctx_process_trade` | About to process a Trade event in drain loop | `symbol` |
| **Actor model** | `symbolctx_shutdown_wait` | SymbolContext destructor waiting for drain to finish | `symbol` |
| **Replay** | `replay_tick` | DBClient replay timer tick completed | `events_emitted`, `current_epoch_ms` |
| **GUI lifecycle** | `gui_replay_entered` | GUIFrontend entered replay mode | (none) |
| **GUI lifecycle** | `gui_replay_exited` | GUIFrontend exited replay mode | (none) |
| **GUI data** | `gui_bar_received` | GUIFrontend dispatches a bar to chart | `symbol`, `tf_seconds` |
| **GUI data** | `gui_level2_received` | GUIFrontend dispatches L2 to widgets | `symbol` |
| **GUI data** | `gui_trade_received` | GUIFrontend dispatches a trade to TimeAndSales | `symbol` |
| **GUI data** | `gui_replay_time_updated` | Replay time label updated | `epoch_ms` |
| **GUI data** | `gui_order_received` | Order update received | `symbol`, `status` |
| **GUI data** | `gui_position_received` | Position update received | `symbol` |
| **GUI chart** | `gui_chart_add_bar` | StockPriceChart::addLiveBar() processes a bar | `symbol`, `chart_index` |
| **GUI chart** | `gui_chart_backfill_received` | Historical backfill bars arrived | `bars_count` |
| **GUI chart** | `gui_chart_timeline_update` | Replay/current time line repositioned | (none) |
| **GUI widget** | `gui_level2_widget_update` | Level2Widget rebuilds order book display | (none) |
| **GUI widget** | `gui_timesales_widget_update` | TimeAndSalesWidget inserts a trade row | (none) |
| **GUI widget** | `gui_order_widget_update` | OrderWidget processes an order update | (none) |
| **GUI widget** | `gui_position_widget_update` | PositionWidget processes a position update | (none) |
| **Pull model** | `gui_pull_tick` | 30 Hz display refresh timer fired (dirty data present) | `dirty_flags` (bitmask: L2=1, trade=2, bar=4, aggregator=8, replayTime=16) |
| **Pull model** | `snapshot_write` | MainAlgo wrote to DisplaySnapshot under write lock | `symbol`, `field` (bar/l2/trade/aggregator/aggregator10s/replayTime) |

### Querying individual events

```bash
# All fetches — what was requested, how many bars came back
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:barcache_api_fetch_done" \
    | grep -oP 'symbol = "[^"]*", tf_seconds = \d+, date = "[^"]*", bars_count = \d+'

# L1 (memory) hit/miss
echo -n "L1 hits:  " && babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:barcache_l1_hit" || true
echo -n "L1 misses:" && babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:barcache_l1_miss" || true

# L2 (SQLite) hit/miss/partial
echo -n "L2 hits:    " && babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:barcache_l2_hit" || true
echo -n "L2 misses:  " && babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:barcache_l2_miss" || true
echo -n "L2 partial: " && babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:barcache_l2_partial" || true

# L2 partial detail (what data was present vs expected)
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:barcache_l2_partial" \
    | grep -oP 'symbol = "[^"]*".*expected = \d+, got = \d+'

# Gap-filling pass summary (bars_in → bars_out after null-bar injection)
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:fillholes_done" \
    | grep -oP 'tf_seconds = \d+, bars_out = \d+, void_bars = \d+'

# Full vs partial day stores (run each line independently)
echo -n "Full day stores: " && babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:barcache_store_full_day" || true
echo -n "Partial stores:  " && babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:barcache_store_partial" || true

# Chart-driven backfill requests (symbol + timeframe + time range)
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:chart_missing_bars_request" \
    | grep -oP 'symbol = "[^"]*", tf_seconds = \d+, start = "[^"]*", end = "[^"]*"'

# Live bar throughput per timeframe
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:livebar_closed" \
    | grep -oP 'interval_seconds = \d+' \
    | sort | uniq -c | sort -rn
```

### Querying GUI events

```bash
# All GUI event types and counts
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep -oP 'l2trader:gui_\w+' | sort | uniq -c | sort -rn

# GUI pull-tick effective refresh rate
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:gui_pull_tick" \
    | grep -oP '\d{2}:\d{2}:\d{2}\.\d{9}' \
    | head -3
# Then use python or manual subtraction to compute inter-tick delta (target: ~33ms)

# Dirty flags distribution (what data types are being refreshed)
# Bitmask: L2=1, trade=2, bar=4, aggregator=8, replayTime=16
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:gui_pull_tick" \
    | grep -oP 'dirty_flags = \d+' | sort | uniq -c | sort -rn

# Snapshot write counts by field (writer-side frequency)
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:snapshot_write" \
    | grep -oP 'field = "[^"]*"' | sort | uniq -c | sort -rn

# Chart add-bar events (each triggers a queued replot)
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:gui_chart_add_bar" \
    | grep -oP 'symbol = "[^"]*", chart_index = \d+'

# Trade widget insert rate (each call inserts one row — high counts indicate batching issue)
echo -n "Trade widget inserts: " && babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:gui_timesales_widget_update" || true

# GUI events in a specific time window (replace HH:MM:SS)
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:gui_" | grep "HH:MM:SS"

# Full tick breakdown — see all GUI events within one tick cycle
# (tick fires, then L2/trade/bar/aggregator/replayTime sub-events follow)
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:gui_" | grep "19:05:29.09" | head -30
```

```bash
# Per-symbol drain throughput (how many items processed per drain call)
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:symbolctx_drain_end" \
    | grep -oP 'symbol = "[^"]*", items_processed = \d+'

# Queue depth at enqueue time (detect backpressure — high values mean the drain can't keep up)
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:symbolctx_enqueue" \
    | grep -oP 'symbol = "[^"]*", event_type = "[^"]*", queue_depth = \d+'

# Pool submit frequency (how often new QRunnables are spawned)
echo -n "Pool submits: " && babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:symbolctx_pool_submit" || true

# Replay tick event batch sizes
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:replay_tick" \
    | grep -oP 'events_emitted = \d+, current_epoch_ms = \d+'

# Shutdown wait events (should be brief — if not, drain is stuck)
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:symbolctx_shutdown_wait"
```

---

## Skill: Correlate Kernel and UST Events on a Timeline

`babeltrace2` merges multiple trace directories by timestamp automatically:

```bash
LATEST=$(ls -td ~/.local/state/L2Trader/lttng-traces/*/ | head -1)

# Merged timeline — filter to a narrow time window (replace with timestamp of interest)
babeltrace2 "${LATEST}kernel" "${LATEST}ust" 2>/dev/null \
    | grep "HH:MM:SS" | head -50

# Find the timestamp of a specific UST event, then look at kernel events around it
babeltrace2 "${LATEST}ust" 2>/dev/null \
    | grep "l2trader:barcache_api_fetch_done" | head -3
# Copy the timestamp, then:
# babeltrace2 "${LATEST}kernel" 2>/dev/null | grep "HH:MM:SS" | head -30
```

This is useful for correlating any application-level event (fetch complete, bar stored, chart request) with the kernel activity (syscalls, context switches, page allocs) that occurred at the same moment.

---

## Skill: Inspect a Specific Time Window

```bash
LATEST=$(ls -td ~/.local/state/L2Trader/lttng-traces/*/ | head -1)

# Events in a specific second from both streams
TIME="21:57:14"
babeltrace2 "${LATEST}kernel" "${LATEST}ust" 2>/dev/null \
    | grep "$TIME"

# Events in a range (use begin/end options for precision)
babeltrace2 "${LATEST}kernel" \
    --begin "2026-03-11T21:57:14.000000000" \
    --end   "2026-03-11T21:57:20.000000000" 2>/dev/null | head -50
```

---

## Skill: Open in TraceCompass

TraceCompass provides visual analysis with timeline, Memory Usage, and Control Flow views.

1. **File → Open Trace…** → select `~/.local/state/L2Trader/lttng-traces/<session>/kernel/`
2. **File → Open Trace…** → select `~/.local/state/L2Trader/lttng-traces/<session>/ust/`
3. Most useful views:
   - **Memory Usage** — RSS growth over time per process (needs `kmem_mm_page_alloc/free`)
   - **Control Flow** — thread scheduling as swimlanes; spot runaway threads visually
   - **LTTng-UST** — filterable event list for all `l2trader:*` tracepoints
   - **System Calls** — syscall density per thread

> Kernel traces are written as root and then `chown`'d back to the user by the run script. If TraceCompass shows a permissions error: `sudo chown -R $USER:$USER ~/.local/state/L2Trader/lttng-traces/`

---

## Skill: List and Manage Trace Sessions

```bash
# List all sessions, newest first, with sizes
du -sh ~/.local/state/L2Trader/lttng-traces/*/ 2>/dev/null | sort -rh

# Delete old sessions (keeps the 5 most recent)
ls -td ~/.local/state/L2Trader/lttng-traces/*/ | tail -n +6 | xargs rm -rf
```

---

## Skill: Run a New Capture

Use VSCode tasks (Ctrl+Shift+P → "Tasks: Run Task"):

| Task | What it does |
|---|---|
| `run-lttng` | Kernel-only tracing, normal binary |
| `run-lttng-instrumented` | Kernel + UST tracing, LTTng-enabled binary |
| `build-with-lttng` | Builds the instrumented binary (`build/LTTng/`) |
| `build-with-lttng-flamegraph` | Builds with `-finstrument-functions` (`build/LTTng-Flamegraph/`) |
| `run-lttng-flamegraph` | Kernel + UST + function call-stack (flamegraph) tracing |

Or run the script directly:
```bash
# Kernel-only (normal build)
.sanitizers/lttng/run-with-lttng.sh

# Kernel + UST (instrumented build)
APP=./build/LTTng/Src/L2Trader .sanitizers/lttng/run-with-lttng.sh

# Kernel + UST + flamegraph (function entry/exit via cyg-profile)
FLAMEGRAPH=true APP=./build/LTTng-Flamegraph/Src/L2Trader .sanitizers/lttng/run-with-lttng.sh
```

No sudo password required — `/usr/bin/lttng` and `/usr/bin/chown` are configured with `NOPASSWD` in `/etc/sudoers.d/l2trader-lttng`.

---

## Skill: Flamegraph Profiling (Function Call-Stack)

The flamegraph build adds `-finstrument-functions` to the compiler flags and uses `LD_PRELOAD=liblttng-ust-cyg-profile-fast.so` at runtime. This emits `lttng_ust_cyg_profile_fast:func_entry` and `lttng_ust_cyg_profile_fast:func_exit` UST events for every function call.

> **Note**: The `-fast` variant uses provider `lttng_ust_cyg_profile_fast` (not `lttng_ust_cyg_profile`). Using the wrong name results in 0 captured events.

### Deferred Recording

Flamegraph recording is **deferred by default**. The cyg-profile library is preloaded at launch but its LTTng events are disabled. This keeps the startup fast (no SSD bandwidth competition with BarCache SQLite reads). When you're ready (e.g., replay is loaded), **press ENTER** in the trace terminal to activate function call recording. **Press ENTER again** to pause it. You can toggle on/off as many times as needed during the session.

While events are disabled, the instrumentation hooks still fire but LTTng's fast-path check returns immediately (~0 overhead — just a cache-line read).

### Build and Run

```bash
# Build (one-time)
mkdir -p build/LTTng-Flamegraph
cmake -S . -B build/LTTng-Flamegraph -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=ON -DBUILD_TESTS=OFF \
    -DLTTNG_ENABLED=ON -DLTTNG_FLAMEGRAPH=ON
cmake --build build/LTTng-Flamegraph -j4

# Run with flamegraph capture (deferred — press ENTER when ready)
FLAMEGRAPH=true APP=./build/LTTng-Flamegraph/Src/L2Trader .sanitizers/lttng/run-with-lttng.sh
```

### Viewing in TraceCompass

1. Open the `ust/` trace directory in TraceCompass
2. TraceCompass auto-detects the `lttng_ust_cyg_profile` events and activates the **LTTng-UST CallStack Analysis**
3. Open these views:
   - **Flame Chart** — call stack over time per thread (shows what each thread was doing at every moment)
   - **Flame Graph** — aggregated icicle chart (width = total time in each function path)
4. Filter to the GUI thread to see exactly what functions consume time during chart updates

### Performance Impact

> ⚠️ Flamegraph mode has **significant overhead** (~5-20× slower) when active. Every function call emits two UST events. The deferred start avoids this during app startup. Use it only for targeted profiling windows, not the entire session.

The `liblttng-ust-cyg-profile-fast.so` variant is used (instead of the regular one) to minimize overhead — it skips `dladdr()` symbol resolution at trace time.

### Querying Call-Stack Events

```bash
LATEST=$(ls -td ~/.local/state/L2Trader/lttng-traces/*/ | head -1)

# Count function entry/exit events
echo -n "func_entry: " && babeltrace2 "${LATEST}ust" 2>/dev/null | grep -c "func_entry" || true
echo -n "func_exit:  " && babeltrace2 "${LATEST}ust" 2>/dev/null | grep -c "func_exit" || true

# Sample of function calls (shows instruction pointer addresses — use addr2line to resolve)
babeltrace2 "${LATEST}ust" 2>/dev/null | grep "lttng_ust_cyg_profile_fast:func_entry" | head -10
```
