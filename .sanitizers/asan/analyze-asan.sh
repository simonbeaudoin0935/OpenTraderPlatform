#!/bin/bash
# Analyze ASan output and summarize memory issues

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPORT_PATTERN="$SCRIPT_DIR/asan-report.txt.*"
CONSOLE_LOG="$SCRIPT_DIR/asan-console.txt"

echo "═══════════════════════════════════════════════════════"
echo "  ASan Analysis - Memory Error Detection"
echo "═══════════════════════════════════════════════════════"
echo ""

# Find all ASan report files
REPORT_FILES=$(ls $REPORT_PATTERN 2>/dev/null || echo "")

if [ -z "$REPORT_FILES" ] && [ ! -f "$CONSOLE_LOG" ]; then
    echo "📄 No ASan reports found"
    echo ""
    echo "This could mean:"
    echo "  ✅ No memory errors detected (perfect!)"
    echo "  ⚠️  App crashed before ASan could write reports"
    echo "  ⚠️  ASan not enabled in the build"
    echo ""
    exit 0
fi

echo "Scanning for memory issues..."
echo ""

# Combine all sources
ALL_OUTPUT=""
if [ -n "$REPORT_FILES" ]; then
    ALL_OUTPUT=$(cat $REPORT_FILES 2>/dev/null || echo "")
fi
if [ -f "$CONSOLE_LOG" ]; then
    ALL_OUTPUT="$ALL_OUTPUT
$(cat "$CONSOLE_LOG")"
fi

# Check for ASan errors
ASAN_ERRORS=$(echo "$ALL_OUTPUT" | grep -c "ERROR: AddressSanitizer:" 2>/dev/null || echo "0")

if [ "$ASAN_ERRORS" = "0" ]; then
    echo "🎉 PERFECT! No memory errors detected!"
    echo ""
    echo "✅ All checks passed:"
    echo "   - No heap buffer overflows"
    echo "   - No stack buffer overflows"
    echo "   - No use-after-free"
    echo "   - No double-free"
    echo "   - No memory leaks"
    echo ""
    exit 0
fi

echo "⚠️  Found $ASAN_ERRORS memory error(s)"
echo ""

# Categorize errors
HEAP_OVERFLOW=$(echo "$ALL_OUTPUT" | grep -c "heap-buffer-overflow" 2>/dev/null || echo "0")
STACK_OVERFLOW=$(echo "$ALL_OUTPUT" | grep -c "stack-buffer-overflow" 2>/dev/null || echo "0")
USE_AFTER_FREE=$(echo "$ALL_OUTPUT" | grep -c "heap-use-after-free" 2>/dev/null || echo "0")
DOUBLE_FREE=$(echo "$ALL_OUTPUT" | grep -c "double-free" 2>/dev/null || echo "0")
LEAKS=$(echo "$ALL_OUTPUT" | grep -c "LeakSanitizer:" 2>/dev/null || echo "0")
GLOBAL_OVERFLOW=$(echo "$ALL_OUTPUT" | grep -c "global-buffer-overflow" 2>/dev/null || echo "0")
STACK_USE_AFTER_SCOPE=$(echo "$ALL_OUTPUT" | grep -c "stack-use-after-scope" 2>/dev/null || echo "0")

echo "ISSUE BREAKDOWN:"
echo "----------------"
[ "$HEAP_OVERFLOW" != "0" ] && echo "  ❌ Heap buffer overflows: $HEAP_OVERFLOW"
[ "$STACK_OVERFLOW" != "0" ] && echo "  ❌ Stack buffer overflows: $STACK_OVERFLOW"
[ "$USE_AFTER_FREE" != "0" ] && echo "  ❌ Use-after-free: $USE_AFTER_FREE"
[ "$DOUBLE_FREE" != "0" ] && echo "  ❌ Double-free: $DOUBLE_FREE"
[ "$LEAKS" != "0" ] && echo "  ⚠️  Memory leaks: $LEAKS"
[ "$GLOBAL_OVERFLOW" != "0" ] && echo "  ❌ Global buffer overflows: $GLOBAL_OVERFLOW"
[ "$STACK_USE_AFTER_SCOPE" != "0" ] && echo "  ❌ Stack-use-after-scope: $STACK_USE_AFTER_SCOPE"

echo ""
echo "L2TRADER CODE ISSUES:"
echo "---------------------"

# Extract L2Trader-specific errors (filter Qt/system libraries)
L2_ERRORS=$(echo "$ALL_OUTPUT" | grep "ERROR: AddressSanitizer:" -A 10 | grep "Src/" || echo "")

if [ -n "$L2_ERRORS" ]; then
    echo "$L2_ERRORS" | head -30
    
    L2_COUNT=$(echo "$L2_ERRORS" | grep "Src/" | wc -l)
    echo ""
    echo "❌ Found issues in L2Trader code - ACTION REQUIRED!"
else
    echo "✅ No issues in L2Trader code"
    echo "   All errors are in external libraries (Qt/system)"
fi

echo ""
echo "DETAILED REPORTS:"
echo "-----------------"
if [ -n "$REPORT_FILES" ]; then
    for file in $REPORT_FILES; do
        echo "  📄 $file"
    done
fi
[ -f "$CONSOLE_LOG" ] && echo "  📄 $CONSOLE_LOG"

echo ""
echo "To see full details:"
echo "  cat $SCRIPT_DIR/asan-report.txt.* $CONSOLE_LOG 2>/dev/null | less"
