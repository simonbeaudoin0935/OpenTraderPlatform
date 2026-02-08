# UBSan (Undefined Behavior Sanitizer) Testing

## Overview
UBSan detects undefined behavior at runtime:
- Null pointer dereferences
- Signed integer overflows
- Out-of-bounds array access
- Use of uninitialized variables
- Invalid bit shifts
- Misaligned pointer access

## Quick Start

### VSCode Tasks (Recommended)
```
Ctrl+Shift+P → "Tasks: Run Task"
```

**Main Tasks:**
- `run-ubsan-with-analysis` ⭐ - Run app with UBSan + auto-analyze results
- `run-ubsan` - Run app with UBSan (raw output)
- `analyze-ubsan-report` - Analyze existing UBSan report

**Build Tasks:**
- `build-ubsan` - Build with UBSan enabled
- `configure-ubsan` - Configure build directory

### Command Line
```bash
# Automated test with analysis
./.sanitizers/run-ubsan-test.sh

# Or manual:
./build/UBSan/Src/L2Trader
./.sanitizers/analyze-ubsan.sh
```

## How It Works

### Build Configuration
UBSan is enabled via CMake option:
```bash
cmake -DENABLE_UBSAN=ON ...
```

This adds compiler flags:
- `-fsanitize=undefined` - Enable UBSan
- `-fno-sanitize-recover=all` - Abort on first error (optional)

### Environment Variables
```bash
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0:log_path=./ubsan-report.txt"
```

Options:
- `print_stacktrace=1` - Show full stack traces
- `halt_on_error=0` - Continue after errors (default)
- `halt_on_error=1` - Abort on first error (testing)
- `log_path=FILE` - Write reports to file

### Suppressions
Add false positives to `.sanitizers/ubsan.supp`:
```
# Suppress specific function
signed-integer-overflow:*MyFunction*

# Suppress specific file
src:*/external/library.cpp

# Suppress Qt internals (if needed)
signed-integer-overflow:*QHash*
```

## Output Analysis

### Clean Run
```
🎉 PERFECT! No undefined behavior detected!

✅ All checks passed:
   - No null pointer dereferences
   - No signed integer overflows
   ...
```

### Issues Found
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

## Common Issues and Fixes

### Signed Integer Overflow
```cpp
// BAD:
int a = INT_MAX;
int b = a + 1;  // ❌ Overflow!

// GOOD:
if (a > INT_MAX - 1) {
    // Handle overflow
}
```

### Null Pointer Dereference
```cpp
// BAD:
MyClass* obj = getObject();
obj->method();  // ❌ obj might be null

// GOOD:
MyClass* obj = getObject();
OBJ_ASSUME_DIFF(obj, nullptr);  // Assert or check
obj->method();
```

### Out-of-Bounds Access
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

## Integration with CI/CD

### GitHub Actions
```yaml
- name: Build with UBSan
  run: |
    cmake -B build -DENABLE_UBSAN=ON
    cmake --build build

- name: Run UBSan tests
  run: |
    ./.sanitizers/run-ubsan-test.sh
```

## Performance Impact
- **Slowdown**: 20-50% slower than normal build
- **Memory**: Slightly higher usage
- **Use case**: Development testing, not production

## Compatibility
- **Compilers**: GCC 4.9+, Clang 3.3+
- **Note**: Cannot be used simultaneously with ASan
- **CMake check**: Fatal error if both enabled

## Files

### Scripts
- `.sanitizers/run-ubsan-test.sh` - Automated test runner
- `.sanitizers/analyze-ubsan.sh` - Report analyzer
- `.sanitizers/ubsan.supp` - Suppressions file

### Generated Reports
- `.sanitizers/ubsan-report.txt.*` - UBSan log files (gitignored)
- `.sanitizers/ubsan-console.txt` - Console output (gitignored)

## Best Practices

1. **Run regularly** during development
2. **Test all features** - UBSan only catches executed code
3. **Fix immediately** - Undefined behavior is a bug, not a warning
4. **Don't suppress L2Trader code** - Only external libraries
5. **Use with valgrind** - Complementary tools (run separately)

## Debugging Tips

### Get more details
```bash
export UBSAN_OPTIONS="print_stacktrace=1:verbosity=1"
./build/UBSan/Src/L2Trader
```

### Abort on first error (for debugging)
```bash
export UBSAN_OPTIONS="halt_on_error=1"
gdb ./build/UBSan/Src/L2Trader
```

### Check specific sanitizers only
```bash
# Only check null pointer issues
cmake -DCMAKE_CXX_FLAGS="-fsanitize=null" ...
```

## See Also
- [UBSan Documentation](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- `Doc/Destructor_Guidelines.md` - Memory management best practices
- `.valgrind/README.md` - Complementary memory leak testing
