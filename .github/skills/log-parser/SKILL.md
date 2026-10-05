---
name: 'Log Parser'
description: 'Skills for parsing and analyzing OpenTraderPlatform log files to diagnose issues and understand app behavior.'
---

# Copilot Skills

This file teaches GitHub Copilot how to perform common debugging and diagnostic tasks specific to the OpenTraderPlatform project.

---

## Skill: Debug the Last App Invocation

### Where are the logs?

OpenTraderPlatform writes a timestamped log file for every invocation. There are two log directories under the XDG state folder:

| Directory | Contents |
|-----------|----------|
| `~/.local/state/OpenTraderPlatform/AppLogs/` | Platform app logs (one file per invocation) |
| `~/.local/state/OpenTraderPlatform/StrategiesLogs/` | Strategy logs (one file per strategy per run) |

**Platform log** filenames:
```
OpenTraderPlatform_YYYY-MM-DD_hh-mm-ss.log
```

**Strategy log** filenames:
```
strategy_{StrategyName}_YYYY-MM-DD_hh-mm-ss.log
```

```bash
# Find the latest log file (assign to variable first to avoid nested command substitution)
LATEST=$(ls -t ~/.local/state/OpenTraderPlatform/AppLogs/ | head -1) && echo "$LATEST"

# Read it
LATEST=$(ls -t ~/.local/state/OpenTraderPlatform/AppLogs/ | head -1) && cat ~/.local/state/OpenTraderPlatform/AppLogs/"$LATEST"

# Grep for errors/warnings in the latest log
LATEST=$(ls -t ~/.local/state/OpenTraderPlatform/AppLogs/ | head -1) && grep -E "WARN|CRIT|FATAL|error" ~/.local/state/OpenTraderPlatform/AppLogs/"$LATEST"

# Grep for user input actions in the latest log
LATEST=$(ls -t ~/.local/state/OpenTraderPlatform/AppLogs/ | head -1) && grep -E "INPT| Input:" ~/.local/state/OpenTraderPlatform/AppLogs/"$LATEST"
```

To find and read the **most recent strategy log**:

```bash
# Find the latest strategy log
LATEST=$(ls -t ~/.local/state/OpenTraderPlatform/StrategiesLogs/ | head -1) && echo "$LATEST"

# Read it
LATEST=$(ls -t ~/.local/state/OpenTraderPlatform/StrategiesLogs/ | head -1) && cat ~/.local/state/OpenTraderPlatform/StrategiesLogs/"$LATEST"
```

> **Important for AI agents**: Always use the two-step pattern above — assign `LATEST` first, then reference `"$LATEST"` — never use `$(...)` inside another `$(...)` (nested command substitution is blocked by the shell security policy).

### Log line format

Every platform log line follows this format:
```
[hh:mm:ss.zzz] LEVL Category: message
```

Where `LEVL` is one of `DEBG`, `INFO`, `INPT`, `WARN`, `CRIT`, or `FATAL`.

- `INPT` is the dedicated user-input stream.
- Input logs currently use the `Input` category and describe GUI actions such as button presses, mode changes, replay controls, symbol changes, and order-entry actions.

#### Replay mode timestamps

When the app was running in **replay mode**, log lines include a second timestamp for the simulated market time:
```
[hh:mm:ss.zzz]-[hh:mm:ss.zzz] LEVL Category: message
```
- **Left bracket** — real wall-clock time the log was issued
- **Right bracket** — simulated replay market time at that moment

Example:
```
[20:19:39.112]-[13:23:21.678] DEBG TSClient: TSClient shutting down
```

### Notes

- The first lines of every platform log list which logging categories are enabled/disabled — useful context when diagnosing missing output.
- When the user asks what actions they performed before a bug, grep the platform log for `INPT` first, then correlate nearby `WARN`/`CRIT` lines.
- Platform logs are plain text — no ANSI escape codes — readable directly with `cat` or `grep`.
- If the app crashed, look for `FATAL`, `SIGABRT`, or `SIGSEGV` near the end of the platform log.
