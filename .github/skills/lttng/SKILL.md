---
name: 'LTTng Trace Browser'
description: 'Skills for opening, querying, and interpreting LTTng kernel + UST traces captured from L2Trader to diagnose performance, memory, and threading issues.'
---

# Copilot Skills

This file teaches GitHub Copilot how to locate, open, and extract information from LTTng traces captured by the L2Trader project.

---

## Skill: Open the Latest LTTng Trace Session

### Where are the traces?

Every run of `run-with-lttng.sh` (or the VSCode tasks `run-lttng` / `run-lttng-instrumented`) creates a timestamped directory under:

```
~/.local/share/L2Trader/lttng-traces/<YYYY-MM-DD_hh-mm-ss>/
```

Each session directory contains two sub-directories:

| Sub-directory | Contents |
|---|---|
| `kernel/` | Kernel-space events (syscalls, scheduler, memory page alloc/free) |
| `ust/` | Userspace tracepoints emitted by L2Trader (`l2trader:*` events) |

```bash
# Find the latest trace session (always use two-step pattern — no nested $(...))
LATEST=$(ls -td ~/.local/share/L2Trader/lttng-traces/*/ | head -1) && echo "Latest: $LATEST"

KERNEL_DIR="${LATEST}kernel"
UST_DIR="${LATEST}ust"
```

> **AI agent note**: Always use the two-step pattern — assign `LATEST` first, then reference `"$LATEST"`. Never nest `$(...)` inside another `$(...)` (blocked by shell security policy).

---

## Skill: List All Events in a Trace

```bash
LATEST=$(ls -td ~/.local/share/L2Trader/lttng-traces/*/ | head -1)

# List all kernel event types present
babeltrace2 "${LATEST}kernel" 2>/dev/null | grep -oP 'event\.name = "[^"]*"' | sort -u

# List all UST event types present
babeltrace2 "${LATEST}ust" 2>/dev/null | grep -oP 'l2trader:\w+' | sort -u

# Full raw dump of all UST events (pipe to less for interactive reading)
babeltrace2 "${LATEST}ust" 2>/dev/null | head -100
```

---

## Skill: Identify Which Thread Is Allocating Memory (OOM / Memory Leak Diagnosis)

### Step 1 — Find the top allocator TIDs from kernel events

```bash
LATEST=$(ls -td ~/.local/share/L2Trader/lttng-traces/*/ | head -1)
KERNEL_DIR="${LATEST}kernel"

# Top TIDs by mmap/brk/mremap call count (highest = runaway allocator)
babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep -E "syscall_entry_mmap|syscall_entry_brk|syscall_entry_mremap" \
    | grep -oP 'tid = \d+' \
    | sort | uniq -c | sort -rn | head -20
```

### Step 2 — Map TIDs to thread names

```bash
# Scheduler events record the thread name (comm) alongside the TID
babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep "sched_switch" \
    | grep -oP 'next_comm = "[^"]*", next_tid = \d+' \
    | sort -u | head -40
```

### Step 3 — Cross-reference a specific TID

```bash
TID=12345   # replace with TID from Step 1

COUNT=$(babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep -E "syscall_entry_mmap|syscall_entry_brk" \
    | grep "tid = ${TID}" | wc -l)

NAME=$(babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep "sched_switch" \
    | grep "next_tid = ${TID}" \
    | grep -oP 'next_comm = "[^"]*"' \
    | sort -u | head -1)

echo "TID ${TID} (${NAME}): ${COUNT} alloc calls"
```

### Step 4 — Find *when* the explosion started (mmap rate per second)

```bash
babeltrace2 "$KERNEL_DIR" 2>/dev/null \
    | grep -E "syscall_entry_mmap|syscall_entry_brk" \
    | grep -oP '\[\d{2}:\d{2}:\d{2}' \
    | tr -d '[' \
    | sort | uniq -c | sort -rn | head -20
```

---

## Skill: Query UST Tracepoints (Instrumented Build Only)

UST events are only present when the app was built with `LTTNG_ENABLED=ON` (the `build-with-lttng` VSCode task) and launched via the `run-lttng-instrumented` task.

All events are in the `l2trader` provider. Check presence first:

```bash
LATEST=$(ls -td ~/.local/share/L2Trader/lttng-traces/*/ | head -1)
UST_DIR="${LATEST}ust"

UST_EVENTS=$(babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:" || true)
echo "UST events found: $UST_EVENTS"
```

### Available `l2trader:*` events

| Event | Fired when | Key fields |
|---|---|---|
| `barcache_api_fetch_start` | Before a Databento historical bar fetch | `symbol`, `tf_seconds`, `date` |
| `barcache_api_fetch_done` | After fetch completes | `symbol`, `tf_seconds`, `date`, `bars_count` |
| `barcache_day_alloc` | A day-vector is allocated in memory | `symbol`, `tf_seconds`, `date`, `num_slots` |
| `barcache_store_bar_live` | A live/replay bar is stored in cache | `symbol`, `tf_seconds`, `index` |
| `barcache_store_full_day` | A complete day written to DB | `symbol`, `tf_seconds`, `date`, `bars_count` |
| `barcache_store_partial` | A partial day written to DB | `symbol`, `tf_seconds`, `date`, `bars_count`, `expected` |
| `barcache_cache_hit` | DB lookup succeeded | `symbol`, `tf_seconds`, `date`, `bars_count` |
| `barcache_cache_miss` | DB lookup returned nothing → triggers fetch | `symbol`, `tf_seconds`, `date` |
| `db_completeness_miss` | DB has data but below 90% completeness threshold | `symbol`, `tf_seconds`, `date`, `expected`, `got` |
| `fillholes_run` | Start of gap-filling after fetch | `tf_seconds`, `step_secs`, `bars_in`, `expected_slots` |
| `fillholes_done` | End of gap-filling | `tf_seconds`, `bars_out`, `void_bars` |
| `livebar_closed` | A live bar interval completed | `symbol`, `interval_seconds` |
| `chart_missing_bars_request` | Chart requested a backfill from BarCache | `symbol`, `tf_seconds`, `start`, `end` |

### Common queries

```bash
LATEST=$(ls -td ~/.local/share/L2Trader/lttng-traces/*/ | head -1)
UST_DIR="${LATEST}ust"

# How many bars were fetched per (symbol, timeframe, date)?
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:barcache_api_fetch_done" \
    | grep -oP 'symbol = "[^"]*", tf_seconds = \d+, date = "[^"]*", bars_count = \d+'

# Cache hit rate
HITS=$(babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:barcache_cache_hit" || true)
MISSES=$(babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:barcache_cache_miss" || true)
echo "Cache hits: $HITS  misses: $MISSES"

# DB completeness misses (why a re-fetch was triggered)
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:db_completeness_miss" \
    | grep -oP 'symbol = "[^"]*".*expected = \d+, got = \d+'

# Detect OOM regression: step_secs=0 in fillholes_run means infinite loop
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:fillholes_run" \
    | grep "step_secs = 0" \
    | head -5

# Full vs partial day stores
FULL=$(babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:barcache_store_full_day" || true)
PARTIAL=$(babeltrace2 "$UST_DIR" 2>/dev/null | grep -c "l2trader:barcache_store_partial" || true)
echo "Full day stores: $FULL  Partial: $PARTIAL"

# How often does the chart trigger a backfill?
babeltrace2 "$UST_DIR" 2>/dev/null \
    | grep "l2trader:chart_missing_bars_request" \
    | grep -oP 'symbol = "[^"]*", tf_seconds = \d+, start = "[^"]*", end = "[^"]*"'
```

---

## Skill: Correlate Kernel and UST Events on a Timeline

Combine both streams (babeltrace2 merges by timestamp automatically):

```bash
LATEST=$(ls -td ~/.local/share/L2Trader/lttng-traces/*/ | head -1)

# Interleaved kernel + UST timeline (filter to a narrow time window)
babeltrace2 "${LATEST}kernel" "${LATEST}ust" 2>/dev/null \
    | grep -E "21:57:14|21:57:15|21:57:16" | head -50
```

This is useful to see exactly which kernel allocations happen in the seconds following a `barcache_api_fetch_done` event.

---

## Skill: Open in TraceCompass

TraceCompass can open the trace directories directly for visual analysis:

1. **File → Open Trace…** → select `~/.local/share/L2Trader/lttng-traces/<session>/kernel/`
2. **File → Open Trace…** → select `~/.local/share/L2Trader/lttng-traces/<session>/ust/`
3. Useful views:
   - **LTTng-UST CallStack** — shows C++ call stacks per thread over time
   - **Memory Usage** — shows RSS growth (requires `kmem_mm_page_alloc/free` kernel events)
   - **Control Flow** — visualizes thread scheduling / wakeup patterns
   - **LTTng-UST** — raw event list, searchable and filterable

> Kernel traces are written as root and then `chown`'d back to the user by the run script. If TraceCompass shows a permissions error, run: `sudo chown -R $USER:$USER ~/.local/share/L2Trader/lttng-traces/`

---

## Skill: List All Trace Sessions and Their Sizes

```bash
# List all sessions, newest first, with sizes
du -sh ~/.local/share/L2Trader/lttng-traces/*/ 2>/dev/null | sort -rh | head -10
```

---

## Skill: Run a New Capture

Use the VSCode tasks (Ctrl+Shift+P → "Tasks: Run Task"):

| Task | What it does |
|---|---|
| `run-lttng` | Kernel-only tracing, normal binary |
| `run-lttng-instrumented` | Kernel + UST tracing, LTTng-enabled binary |
| `run-lttng-with-analysis` | Kernel-only + runs `analyze-lttng.sh` after exit |
| `run-lttng-instrumented-with-analysis` | Full kernel+UST + analysis after exit |
| `build-with-lttng` | Builds the instrumented binary (`build/LTTng/`) |

Or run the script directly:
```bash
# Kernel-only (normal build)
.sanitizers/lttng/run-with-lttng.sh

# Kernel + UST (instrumented build)
APP=./build/LTTng/Src/L2Trader .sanitizers/lttng/run-with-lttng.sh
```

No sudo password required — `/usr/bin/lttng` and `/usr/bin/chown` are configured with `NOPASSWD` in `/etc/sudoers.d/l2trader-lttng`.
