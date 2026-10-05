---
name: "coredump"
description: "Debug OpenTraderPlatform crashes with systemd coredumps: locate crash artifacts, symbolize stacks, inspect in gdb, and produce actionable root-cause reports."
---

Use this skill when the user asks to debug a crash, analyze `SIGABRT`/`SIGSEGV`, inspect a stacktrace, or mentions `core`, `coredumpctl`, `gdb`, or `addr2line`.

## Goal

Turn a crash into a concrete diagnosis by:

1. finding the exact crashing run (log + core);
2. symbolizing app frames back to source lines;
3. extracting the failing thread state and nearby context;
4. reporting likely root cause, confidence, and next fix/test steps.

## Known Environment (OpenTraderPlatform)

- Dev machine host: `simon@192.168.1.144`
- Remote repo: `~/Documents/OpenTraderPlatform`
- GUI binary: `~/Documents/OpenTraderPlatform/build/Src/OpenTraderPlatform`
- App logs: `~/.local/state/OpenTraderPlatform/AppLogs/`
- Coredump backend: `systemd-coredump` (`coredumpctl`)

## 1) Confirm Core Dumps Are Enabled

Run this first when no new core appears.

```bash
# Shell limit must be unlimited for new crashes from this shell/session
ulimit -c

# Must point to systemd-coredump handler
cat /proc/sys/kernel/core_pattern

# User manager limit should be infinity
systemctl --user show --property=DefaultLimitCORE --value
```

Expected:

- `ulimit -c` -> `unlimited`
- `core_pattern` contains `systemd-coredump`
- user default core limit is `infinity`

## 2) Find the Crashing Invocation

```bash
# Latest app log
LATEST=$(ls -t ~/.local/state/OpenTraderPlatform/AppLogs/ | head -1) && echo "$LATEST"

# Most recent log that actually contains crash marker
for f in $(ls -t ~/.local/state/OpenTraderPlatform/AppLogs/); do
  if grep -q "Received signal" ~/.local/state/OpenTraderPlatform/AppLogs/"$f"; then
    echo "$f"
    break
  fi
done
```

Then inspect:

```bash
LOG=~/.local/state/OpenTraderPlatform/AppLogs/<crash-log>.log
grep -n "Version:" "$LOG"
grep -n "Received signal\|Stack trace" "$LOG"
grep -n "INPT" "$LOG" | tail -n 40
```

## 3) Locate Matching Core

```bash
# List recent OpenTraderPlatform cores
coredumpctl --no-pager list OpenTraderPlatform | tail -n 20

# Inspect one candidate PID
coredumpctl --no-pager info <PID>
```

Use timestamp + executable path + signal to correlate with the log run.

## 4) Symbolize Stacktrace Addresses From Log

When app log contains lines like:

```text
/path/to/OpenTraderPlatform(+0xc53fe)
```

Symbolize with:

```bash
addr2line -Cfpe ~/Documents/OpenTraderPlatform/build/Src/OpenTraderPlatform 0xc53fe 0xc539d
```

If crash happened on another commit/build, use the binary that produced that log.

## 5) Deep Debug in gdb From Core

```bash
coredumpctl debug <PID>
```

Inside gdb, run:

```gdb
set pagination off
info threads
thread apply all bt full
frame 0
info locals
disassemble /m
```

Then inspect project-specific state where relevant:

```gdb
# Examples (adapt to frame scope)
p this
p m_symbol
p m_displayTimeFrame
p m_latestBarIndex
```

For Qt-heavy stacks, also capture:

- first non-Qt/non-libc frame;
- whether abort came from Qt fatal checks (`qFatal`, `Q_ASSERT`, allocator abort);
- any evidence of use-after-free/double-free (e.g., `__libc_free`, corrupted pointer paths).

## 6) Export Core For Offline Analysis (Optional)

```bash
# Writes raw core file in current directory
coredumpctl dump <PID> --output core.OpenTraderPlatform.<PID>

# Open manually with matching binary
gdb ~/Documents/OpenTraderPlatform/build/Src/OpenTraderPlatform core.OpenTraderPlatform.<PID>
```

## 7) Remote Dev Machine Workflow

Use these from local workspace when user asks you to debug remote crashes:

```bash
ssh simon@192.168.1.144 'coredumpctl --no-pager list OpenTraderPlatform | tail -n 20'
ssh simon@192.168.1.144 'coredumpctl --no-pager info <PID>'
ssh simon@192.168.1.144 'addr2line -Cfpe ~/Documents/OpenTraderPlatform/build/Src/OpenTraderPlatform <addr1> <addr2>'
ssh simon@192.168.1.144 'coredumpctl debug <PID>'
```

## 8) Reporting Template

Always report findings in this structure:

1. **Crash identity**: signal, timestamp, commit/version, PID.
2. **Load-bearing frames**: first app frames with file:line.
3. **Execution context**: recent `INPT` actions and nearby WARN/CRIT lines.
4. **Root cause hypothesis**: concise, technical, with confidence level.
5. **Next action**: exact code area/tests to validate fix.

## 9) Common Failure Cases

- `coredumpctl list` is empty:
  - `ulimit -c` not unlimited for launch shell/session.
  - user/system `DefaultLimitCORE` not set.
  - crash happened in a different host/session/user.
- Stack has only offsets / poor symbols:
  - wrong binary for that crash.
  - non-debug build or stripped symbols.
  - rebuild with `-DCMAKE_BUILD_TYPE=Debug` or `RelWithDebInfo`.
- Core exists but gdb backtrace is shallow:
  - install debug symbol packages for libc/Qt on host.

## Notes

- App-level crash logger writes its own stacktrace to app logs; always correlate it with the core.
- For allocator crashes, the visible frame is often where corruption is detected, not where it originated.
- Favor reproducibility: record exact symbol, timeframe, replay/live mode, and last user actions before crash.
