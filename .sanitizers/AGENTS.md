# L2Trader Sanitizers and Memory Analysis Tools

## Directory Organization

This directory contains runtime sanitizers and memory analysis tools organized by tool type:

```
.sanitizers/
├── AGENTS.md           # This file - AI agent instructions
├── ubsan/              # UndefinedBehaviorSanitizer
│   ├── analyze-ubsan.sh
│   ├── run-ubsan-test.sh
│   └── ubsan.supp
├── asan/               # AddressSanitizer
│   ├── analyze-asan.sh
│   ├── run-asan-test.sh
│   └── asan.supp
├── valgrind/           # Valgrind memory leak detection
│   ├── analyze-leaks.sh
│   ├── run-valgrind-test.sh
│   └── valgrind-qt.supp
└── lttng/              # LTTng kernel tracing (thread/memory diagnostics)
    └── run-with-lttng.sh
```

## Tool Capabilities

### UBSan (Undefined Behavior Sanitizer) - `ubsan/`
**Purpose**: Detects undefined behavior at compile-time and runtime
**When to use**: Checking for code correctness and standards compliance
**Detects**:
- Null pointer dereferences
- Signed integer overflows
- Out-of-bounds array access
- Use of uninitialized variables
- Invalid bit shifts
- Misaligned pointer access

**Build flag**: `-DENABLE_UBSAN=ON`
**Performance**: 20-50% slowdown
**Memory**: Minimal overhead

### ASan (Address Sanitizer) - `asan/`
**Purpose**: Detects memory access errors and memory leaks
**When to use**: Debugging memory corruption and heap issues
**Detects**:
- Heap buffer overflows
- Stack buffer overflows
- Use-after-free
- Double-free
- Memory leaks (via LeakSanitizer)
- Global buffer overflows

**Build flag**: `-DENABLE_ASAN=ON`
**Performance**: 50-100% slowdown
**Memory**: 2-3x memory usage

### Valgrind - `valgrind/`
**Purpose**: Comprehensive memory error and leak detection
**When to use**: Deep memory leak analysis, complement to ASan
**Detects**:
- All memory leaks (definite, indirect, possible)
- Invalid memory access
- Use of uninitialized memory
- Double-free and invalid-free

**Build flag**: Normal Debug build (no special flags)
**Performance**: 10-50x slowdown
**Memory**: Moderate overhead

**Important**: UBSan and ASan cannot be used simultaneously (enforced by CMake). Valgrind can be used separately with any build.

### LTTng - `lttng/`
**Purpose**: Kernel-level event tracing with nanosecond timestamps — no code changes required
**When to use**: Identifying runaway memory growth, pinpointing which thread is causing OOM, correlating heap allocation bursts to thread names
**Traces**:
- `mmap` / `brk` / `mremap` syscalls → heap growth events
- `sched_switch` → active thread at every context switch (maps TID → Qt thread name)
- `sched_process_fork` → new thread spawns

**Prerequisites**: `sudo apt install lttng-tools lttng-modules-dkms babeltrace2`
**Requires sudo**: Yes (kernel module needs elevated privileges)
**Performance**: Negligible overhead (ring-buffer, asynchronous)
**Trace output**: `~/.local/state/L2Trader/lttng-traces/<timestamp>/`

**VSCode tasks**: `run-lttng`, `run-lttng-instrumented`, `build-with-lttng`

## Quick Start for AI Agents

### Running Tests Locally

**UBSan**:
```bash
./.sanitizers/ubsan/run-ubsan-test.sh
```

**ASan**:
```bash
./.sanitizers/asan/run-asan-test.sh
```

**Valgrind**:
```bash
./.sanitizers/valgrind/run-valgrind-test.sh
```

**LTTng** (kernel tracing):
```bash
./.sanitizers/lttng/run-with-lttng.sh
```

For trace queries and event reference, see `.github/skills/lttng/SKILL.md`.

### Analyzing Reports

**UBSan**:
```bash
./.sanitizers/ubsan/analyze-ubsan.sh
```

**ASan**:
```bash
./.sanitizers/asan/analyze-asan.sh
```

**Valgrind**:
```bash
./.sanitizers/valgrind/analyze-leaks.sh [report-file]
```

**LTTng**:
```bash
# View the latest trace
LATEST=$(ls -td ~/.local/state/L2Trader/lttng-traces/*/ | head -1)
babeltrace2 "${LATEST}ust" 2>/dev/null | head -50
# See .github/skills/lttng/SKILL.md for full query reference
```

### CI Integration

The `.github/workflows/build.yml` includes automated tests:
- `build-ubsan` + `test-ubsan`: Builds with UBSan and runs test
- `build-asan` + `test-asan`: Builds with ASan and runs test

Each test job:
1. Downloads sanitizer-enabled build
2. Runs app in Xvfb (virtual X display)
3. Waits 5 seconds for initialization
4. Sends Ctrl+Q for graceful exit
5. Analyzes output with respective analyze script
6. Uploads reports as artifacts (7-day retention)

## File Locations and Paths

### Report Files (gitignored)
- UBSan: `.sanitizers/ubsan/ubsan-report.txt.*`, `.sanitizers/ubsan/ubsan-console.txt`
- ASan: `.sanitizers/asan/asan-report.txt.*`, `.sanitizers/asan/asan-console.txt`
- Valgrind: `.sanitizers/valgrind/valgrind-report.txt`

### Suppression Files
- UBSan: `.sanitizers/ubsan/ubsan.supp`
- ASan: `.sanitizers/asan/asan.supp`
- Valgrind: `.sanitizers/valgrind/valgrind-qt.supp`

## VSCode Integration

Tasks are configured in `.vscode/tasks.json`:

**UBSan**:
- `run-ubsan-with-analysis` - Run + analyze
- `analyze-ubsan-report` - Analyze existing report
- `build-ubsan` - Build with UBSan

**ASan**:
- `run-asan-with-analysis` - Run + analyze
- `analyze-asan-report` - Analyze existing report
- `build-asan` - Build with ASan

**Valgrind**:
- `run-valgrind-with-analysis` - Run + analyze
- `analyze-valgrind-report` - Analyze existing report
- `run-valgrind` - Run with full options

**LTTng**:
- `run-lttng` - Launch app under kernel LTTng tracing
- `run-lttng-instrumented` - Launch instrumented build (kernel + UST)
- `build-with-lttng` - Build the LTTNG_ENABLED binary

## How It Works

### Build Configuration

**UBSan:**
```bash
cmake -DENABLE_UBSAN=ON -DCMAKE_BUILD_TYPE=Debug ...
```
This adds compiler flags:
- `-fsanitize=undefined` - Enable UBSan
- `-fno-sanitize-recover=all` - Abort on first error

**ASan:**
```bash
cmake -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug ...
```
This adds compiler flags:
- `-fsanitize=address` - Enable ASan
- `-fno-omit-frame-pointer` - Better stack traces

**Valgrind:**
```bash
valgrind --leak-check=full --show-leak-kinds=definite,possible \
  --suppressions=.sanitizers/valgrind/valgrind-qt.supp ./build/GUI/Src/L2Trader
```

### Environment Variables

**UBSan:**
```bash
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0:log_path=./ubsan/ubsan-report.txt:suppressions=./ubsan/ubsan.supp"
```

Options:
- `print_stacktrace=1` - Show full stack traces
- `halt_on_error=0` - Continue after errors (default)
- `halt_on_error=1` - Abort on first error (testing)
- `log_path=FILE` - Write reports to file
- `suppressions=FILE` - Suppress known false positives

**ASan:**
```bash
ASAN_OPTIONS="log_path=./asan/asan-report.txt:halt_on_error=0:detect_leaks=1:suppressions=./asan/asan.supp"
```

Options:
- `halt_on_error=0` - Continue after errors
- `detect_leaks=1` - Enable leak detection
- `log_path=FILE` - Write reports to file
- `suppressions=FILE` - Suppress known false positives

### Suppressions

**UBSan suppressions** (`.sanitizers/ubsan/ubsan.supp`):
```
# Suppress specific function
signed-integer-overflow:*MyFunction*

# Suppress specific file
src:*/external/library.cpp

# Suppress Qt internals (if needed)
signed-integer-overflow:*QHash*
```

**ASan suppressions** (`.sanitizers/asan/asan.supp`):
```
# Suppress memory leaks from Qt
leak:*QApplication*

# Suppress system library leaks
leak:*libfontconfig*
```

**Valgrind suppressions** (`.sanitizers/valgrind/valgrind-qt.supp`):
- Comprehensive Qt6, GTK, GLib, fontconfig suppressions
- System library suppressions
- See file for full list of patterns

## Output Analysis

### UBSan Clean Run
```
🎉 PERFECT! No undefined behavior detected!

✅ All checks passed:
   - No null pointer dereferences
   - No signed integer overflows
   ...
```

### UBSan Issues Found
```
⚠️  Found 5 undefined behavior issue(s)

ISSUE BREAKDOWN:
----------------
  ❌ Signed integer overflows: 3
  ❌ Null pointer dereferences: 2

L2TRADER CODE ISSUES:
---------------------
❌ Found 2 issue(s) in L2Trader code - ACTION REQUIRED!
```

### ASan Clean Run
```
🎉 PERFECT! No memory errors detected!

✅ All checks passed:
   - No heap buffer overflows
   - No use-after-free
   - No memory leaks
```

### ASan Issues Found
```
⚠️  Found 3 memory error(s)

ISSUE BREAKDOWN:
----------------
  ❌ Heap buffer overflows: 1
  ❌ Use-after-free: 1
  ⚠️  Memory leaks: 1
```

## Common Issues and Fixes

### UBSan: Signed Integer Overflow
### UBSan: Signed Integer Overflow
```cpp
// BAD:
int a = INT_MAX;
int b = a + 1;  // ❌ Overflow!

// GOOD:
if (a > INT_MAX - 1) {
    // Handle overflow
}
```

### UBSan: Null Pointer Dereference
### UBSan: Null Pointer Dereference
```cpp
// BAD:
MyClass* obj = getObject();
obj->method();  // ❌ obj might be null

// GOOD:
MyClass* obj = getObject();
OBJ_ASSUME_DIFF(obj, nullptr);  // Assert or check
obj->method();
```

### UBSan: Out-of-Bounds Access
### UBSan: Out-of-Bounds Access
```cpp
// BAD:
int arr[5];
arr[5] = 10;  // ❌ Index out of bounds

// GOOD:
int arr[5];
if (index < 5) {
    arr[index] = 10;
}
```

### ASan: Heap Buffer Overflow
```cpp
// BAD:
char* buf = new char[10];
strcpy(buf, "This is too long");  // ❌ Buffer overflow

// GOOD:
char* buf = new char[20];
strncpy(buf, "This is safe", 19);
buf[19] = '\0';
```

### ASan: Use-After-Free
```cpp
// BAD:
auto* obj = new MyClass();
delete obj;
obj->method();  // ❌ Use after free

// GOOD:
auto* obj = new MyClass();
obj->method();
delete obj;
obj = nullptr;  // Prevent accidental reuse
```

### ASan: Memory Leak
```cpp
// BAD:
MyClass* obj = new MyClass();
// Never deleted  // ❌ Memory leak

// GOOD:
std::unique_ptr<MyClass> obj = std::make_unique<MyClass>();
// Automatically deleted
```

## Integration with CI/CD

Both sanitizers are now integrated into the CI pipeline:
Both sanitizers are now integrated into the CI pipeline:

### GitHub Actions Workflow
The `.github/workflows/build.yml` includes these jobs:

1. **build-ubsan** - Builds application with UBSan enabled
2. **test-ubsan** - Runs the app, sends Ctrl+Q to exit, analyzes UBSan output
3. **build-asan** - Builds application with ASan enabled
4. **test-asan** - Runs the app, sends Ctrl+Q to exit, analyzes ASan output

Each test job:
- Downloads the sanitizer-enabled build
- Sets up test credentials
- Runs the app in Xvfb (virtual X display)
- Waits 5 seconds for initialization
- Sends Ctrl+Q for graceful exit
- Analyzes sanitizer output
- Uploads reports as artifacts (retained for 7 days)

### Viewing CI Results
After a CI run:
1. Go to the Actions tab
2. Click on the workflow run
3. Download the sanitizer reports:
   - `ubsan-reports-<sha>` - UBSan findings
   - `asan-reports-<sha>` - ASan findings

### Manual CI Testing
```yaml
- name: Build with UBSan
  run: |
    cmake -B build/UBSan -DENABLE_UBSAN=ON -DCMAKE_BUILD_TYPE=Debug
    cmake --build build/UBSan

- name: Run UBSan test
  run: |
    export UBSAN_OPTIONS="log_path=ubsan-report.txt"
    ./build/UBSan/Src/L2Trader
    
- name: Analyze
  run: ./.sanitizers/ubsan/analyze-ubsan.sh
```

## Performance Impact

### UBSan
- **Slowdown**: 20-50% slower than normal build
- **Memory**: Slightly higher usage
- **Use case**: Development testing, not production

### ASan
- **Slowdown**: 50-100% slower than normal build  
- **Memory**: 2-3x memory usage
- **Use case**: Development testing, not production

## Compatibility

### UBSan
- **Compilers**: GCC 4.9+, Clang 3.3+
- **Cannot** be used with ASan simultaneously
- CMake check will error if both enabled

### ASan
- **Compilers**: GCC 4.8+, Clang 3.1+
- **Cannot** be used with UBSan simultaneously
- CMake check will error if both enabled

## Files

### Scripts
- `.sanitizers/ubsan/run-ubsan-test.sh` - UBSan test runner
- `.sanitizers/ubsan/analyze-ubsan.sh` - UBSan report analyzer
- `.sanitizers/asan/run-asan-test.sh` - ASan test runner
- `.sanitizers/asan/analyze-asan.sh` - ASan report analyzer

### Configuration
- `.sanitizers/ubsan.supp` - UBSan suppressions
- `.sanitizers/asan.supp` - ASan suppressions

### Generated Reports (gitignored)
- `.sanitizers/ubsan-report.txt.*` - UBSan log files
- `.sanitizers/ubsan-console.txt` - UBSan console output
- `.sanitizers/asan-report.txt.*` - ASan log files
- `.sanitizers/asan-console.txt` - ASan console output

## Best Practices

1. **Run regularly** during development
2. **Test all features** - Sanitizers only catch executed code
3. **Fix immediately** - Don't ignore sanitizer warnings
4. **Don't suppress L2Trader code** - Only external libraries
5. **Use both sanitizers** - Run separately, they catch different bugs
6. **CI integration** - Sanitizer tests run automatically on every push

## Debugging Tips

### UBSan: Get more details
```bash
export UBSAN_OPTIONS="print_stacktrace=1:verbosity=1"
./build/UBSan/Src/L2Trader
```

### UBSan: Abort on first error (for debugging)
```bash
export UBSAN_OPTIONS="halt_on_error=1"
gdb ./build/UBSan/Src/L2Trader
```

### ASan: Get more details
```bash
export ASAN_OPTIONS="verbosity=1:log_path=asan.log"
./build/ASan/Src/L2Trader
```

### ASan: Abort on first error
```bash
export ASAN_OPTIONS="halt_on_error=1"
gdb ./build/ASan/Src/L2Trader
```

### Check specific sanitizers only
```bash
# UBSan: Only check null pointer issues
cmake -DCMAKE_CXX_FLAGS="-fsanitize=null" ...

# ASan: Disable leak checking
export ASAN_OPTIONS="detect_leaks=0"
```

## See Also
- [UBSan Documentation](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- [ASan Documentation](https://clang.llvm.org/docs/AddressSanitizer.html)
- [Valgrind Documentation](https://valgrind.org/docs/manual/quick-start.html)
- `Doc/Destructor_Guidelines.md` - Memory management best practices
- `.sanitizers/valgrind/` - Valgrind scripts and suppressions for memory leak testing
