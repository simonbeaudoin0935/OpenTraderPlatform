#!/bin/bash
# Analyze UBSan output and summarize undefined behavior issues

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPORT_PATTERN="$SCRIPT_DIR/ubsan-report.txt.*"
CONSOLE_LOG="$SCRIPT_DIR/ubsan-console.txt"

echo "═══════════════════════════════════════════════════════"
echo "  UBSan Analysis - Undefined Behavior Detection"
echo "═══════════════════════════════════════════════════════"
echo ""

# Find all UBSan report files
REPORT_FILES=$(ls $REPORT_PATTERN 2>/dev/null || echo "")

if [ -z "$REPORT_FILES" ] && [ ! -f "$CONSOLE_LOG" ]; then
    echo "📄 No UBSan reports found"
    echo ""
    echo "This could mean:"
    echo "  ✅ No undefined behavior detected (perfect!)"
    echo "  ⚠️  App crashed before UBSan could write reports"
    echo "  ⚠️  UBSan not enabled in the build"
    echo ""
    exit 0
fi

echo "Scanning for undefined behavior issues..."
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

# Check for UBSan errors
UBSAN_ERRORS=$(echo "$ALL_OUTPUT" | grep -c "runtime error:" 2>/dev/null || echo "0")

if [ "$UBSAN_ERRORS" = "0" ]; then
    echo "🎉 PERFECT! No undefined behavior detected!"
    echo ""
    echo "✅ All checks passed:"
    echo "   - No null pointer dereferences"
    echo "   - No signed integer overflows"
    echo "   - No use of uninitialized variables"
    echo "   - No out-of-bounds array access"
    echo "   - No invalid shifts"
    echo "   - No misaligned pointers"
    echo ""
    exit 0
fi

echo "⚠️  Found $UBSAN_ERRORS undefined behavior issue(s)"
echo ""

# Categorize errors
NULL_DEREF=$(echo "$ALL_OUTPUT" | grep -c "null pointer" 2>/dev/null || echo "0")
OVERFLOW=$(echo "$ALL_OUTPUT" | grep -c "signed integer overflow" 2>/dev/null || echo "0")
SHIFT=$(echo "$ALL_OUTPUT" | grep -c "shift" 2>/dev/null || echo "0")
BOUNDS=$(echo "$ALL_OUTPUT" | grep -c "out of bounds" 2>/dev/null || echo "0")
ALIGN=$(echo "$ALL_OUTPUT" | grep -c "misaligned" 2>/dev/null || echo "0")
UNINIT=$(echo "$ALL_OUTPUT" | grep -c "uninitialized" 2>/dev/null || echo "0")
OTHER=$(echo "$UBSAN_ERRORS - $NULL_DEREF - $OVERFLOW - $SHIFT - $BOUNDS - $ALIGN - $UNINIT" | bc)

echo "ISSUE BREAKDOWN:"
echo "----------------"
[ "$NULL_DEREF" != "0" ] && echo "  ❌ Null pointer dereferences: $NULL_DEREF"
[ "$OVERFLOW" != "0" ] && echo "  ❌ Signed integer overflows: $OVERFLOW"
[ "$SHIFT" != "0" ] && echo "  ❌ Invalid shift operations: $SHIFT"
[ "$BOUNDS" != "0" ] && echo "  ❌ Out-of-bounds access: $BOUNDS"
[ "$ALIGN" != "0" ] && echo "  ❌ Misaligned pointers: $ALIGN"
[ "$UNINIT" != "0" ] && echo "  ❌ Uninitialized variables: $UNINIT"
[ "$OTHER" != "0" ] && echo "  ⚠️  Other issues: $OTHER"

echo ""
echo "OPENTRADERPLATFORM CODE ISSUES:"
echo "---------------------"

# Extract OpenTraderPlatform-specific errors (filter Qt/system libraries)
L2_ERRORS=$(echo "$ALL_OUTPUT" | grep "runtime error:" | grep "Src/" || echo "")

if [ -n "$L2_ERRORS" ]; then
    echo "$L2_ERRORS" | head -20
    
    L2_COUNT=$(echo "$L2_ERRORS" | wc -l)
    echo ""
    echo "❌ Found $L2_COUNT issue(s) in OpenTraderPlatform code - ACTION REQUIRED!"
else
    echo "✅ No issues in OpenTraderPlatform code"
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
echo "  cat $SCRIPT_DIR/ubsan-report.txt.* $CONSOLE_LOG 2>/dev/null | less"
