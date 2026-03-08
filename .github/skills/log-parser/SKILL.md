---
name: 'Log Parser'
description: 'Skills for parsing and analyzing L2Trader log files to diagnose issues and understand app behavior.'
---

# Copilot Skills

This file teaches GitHub Copilot how to perform common debugging and diagnostic tasks specific to the L2Trader project.

---

## Skill: Debug the Last App Invocation

### Where are the logs?

L2Trader writes a timestamped log file for every invocation. There are two log directories under the XDG state folder:

| Directory | Contents |
|-----------|----------|
| `~/.local/state/L2Trader/AppLogs/` | Platform app logs (one file per invocation) |
| `~/.local/state/L2Trader/StrategiesLogs/` | Strategy logs (one file per strategy per run) |

**Platform log** filenames:
```
L2Trader_YYYY-MM-DD_hh-mm-ss.log.ansi
```

**Strategy log** filenames:
```
strategy_{StrategyName}_YYYY-MM-DD_hh-mm-ss.log
```

The `.ansi` extension indicates the file contains ANSI color escape codes. To find and read the **most recent platform log**:

```bash
# Find the latest log file (assign to variable first to avoid nested command substitution)
LATEST=$(ls -t ~/.local/state/L2Trader/AppLogs/ | head -1) && echo "$LATEST"

# Read it (plain text, stripping ANSI escape codes)
LATEST=$(ls -t ~/.local/state/L2Trader/AppLogs/ | head -1) && sed 's/\x1b\[[0-9;]*m//g' ~/.local/state/L2Trader/AppLogs/"$LATEST"

# Grep for errors/warnings in the latest log
LATEST=$(ls -t ~/.local/state/L2Trader/AppLogs/ | head -1) && sed 's/\x1b\[[0-9;]*m//g' ~/.local/state/L2Trader/AppLogs/"$LATEST" | grep -E "WARN|CRIT|FATAL|error"
```

To find and read the **most recent strategy log**:

```bash
# Find the latest strategy log
LATEST=$(ls -t ~/.local/state/L2Trader/StrategiesLogs/ | head -1) && echo "$LATEST"

# Read it
LATEST=$(ls -t ~/.local/state/L2Trader/StrategiesLogs/ | head -1) && cat ~/.local/state/L2Trader/StrategiesLogs/"$LATEST"
```

> **Important for AI agents**: Always use the two-step pattern above — assign `LATEST` first, then reference `"$LATEST"` — never use `$(...)` inside another `$(...)` (nested command substitution is blocked by the shell security policy).

### Notes

- The first lines of every platform log list which logging categories are enabled/disabled — useful context when diagnosing missing output.
- In TUI mode, platform logs are written to **stderr** in addition to the file (stdout is the ncurses UI).
- If the app crashed, look for `FATAL`, `SIGABRT`, or `SIGSEGV` near the end of the platform log.
- Strategy logs do not use ANSI codes (plain text, no `.ansi` extension).

