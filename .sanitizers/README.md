# Sanitizers Testing for L2Trader

This directory contains scripts and configuration for runtime sanitizers that detect bugs during execution.

## Available Sanitizers

### UBSan (Undefined Behavior Sanitizer)
Detects undefined behavior at runtime:
- Null pointer dereferences
- Signed integer overflows
- Out-of-bounds array access
- Use of uninitialized variables
- Invalid bit shifts
- Misaligned pointer access

### ASan (Address Sanitizer)
Detects memory errors at runtime:
- Heap buffer overflows
- Stack buffer overflows
- Use-after-free
- Double-free
- Memory leaks
- Global buffer overflows

**Note**: UBSan and ASan cannot be used simultaneously. Choose one based on what you're testing.

## Quick Start

### VSCode Tasks (Recommended)
```
Ctrl+Shift+P → "Tasks: Run Task"
```

**UBSan Tasks:**
- `run-ubsan-with-analysis` ⭐ - Run app with UBSan + auto-analyze results
- `run-ubsan` - Run app with UBSan (raw output)
- `analyze-ubsan-report` - Analyze existing UBSan report
- `build-ubsan` - Build with UBSan enabled

**ASan Tasks:**
- `run-asan` - Run app with ASan
- `build-asan` - Build with ASan enabled

### Command Line

#### UBSan
```bash
# Automated test with analysis
./.sanitizers/run-ubsan-test.sh

# Or manual:
./build/UBSan/Src/L2Trader
./.sanitizers/analyze-ubsan.sh
```

#### ASan
```bash
# Automated test with analysis
./.sanitizers/run-asan-test.sh

# Or manual:
./build/ASan/Src/L2Trader
./.sanitizers/analyze-asan.sh
```

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

### Environment Variables

**UBSan:**
**UBSan:**
```bash
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0:log_path=./ubsan-report.txt:suppressions=ubsan.supp"
```

Options:
- `print_stacktrace=1` - Show full stack traces
- `halt_on_error=0` - Continue after errors (default)
- `halt_on_error=1` - Abort on first error (testing)
- `log_path=FILE` - Write reports to file
- `suppressions=FILE` - Suppress known false positives

**ASan:**
```bash
ASAN_OPTIONS="log_path=./asan-report.txt:halt_on_error=0:detect_leaks=1:suppressions=asan.supp"
```

Options:
- `halt_on_error=0` - Continue after errors
- `detect_leaks=1` - Enable leak detection
- `log_path=FILE` - Write reports to file
- `suppressions=FILE` - Suppress known false positives

### Suppressions

**UBSan suppressions** (`.sanitizers/ubsan.supp`):
**UBSan suppressions** (`.sanitizers/ubsan.supp`):
```
# Suppress specific function
signed-integer-overflow:*MyFunction*

# Suppress specific file
src:*/external/library.cpp

# Suppress Qt internals (if needed)
signed-integer-overflow:*QHash*
```

**ASan suppressions** (`.sanitizers/asan.supp`):
```
# Suppress memory leaks from Qt
leak:*QApplication*

# Suppress system library leaks
leak:*libfontconfig*
```

## Output Analysis

### UBSan Clean Run
### UBSan Clean Run
```
🎉 PERFECT! No undefined behavior detected!

✅ All checks passed:
   - No null pointer dereferences
   - No signed integer overflows
   ...
```

### UBSan Issues Found
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
  run: ./.sanitizers/analyze-ubsan.sh
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
- `.sanitizers/run-ubsan-test.sh` - UBSan test runner
- `.sanitizers/analyze-ubsan.sh` - UBSan report analyzer
- `.sanitizers/run-asan-test.sh` - ASan test runner
- `.sanitizers/analyze-asan.sh` - ASan report analyzer

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
- `Doc/Destructor_Guidelines.md` - Memory management best practices
- `.valgrind/README.md` - Complementary memory leak testing
