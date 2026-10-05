#!/bin/bash
# Analyze valgrind report and show only OpenTraderPlatform-specific leaks
# Filters out system library leaks to focus on our code

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPORT="${1:-$SCRIPT_DIR/valgrind-report.txt}"

if [ ! -f "$REPORT" ]; then
    echo "❌ Error: Report file not found: $REPORT"
    echo ""
    echo "Usage: $0 [report-file]"
    echo "Default: $SCRIPT_DIR/valgrind-report.txt"
    exit 1
fi

echo "═══════════════════════════════════════════════════════"
echo "  OpenTraderPlatform Memory Leak Analysis"
echo "═══════════════════════════════════════════════════════"
echo "Report: $REPORT"
echo ""

# Extract summary
echo "LEAK SUMMARY:"
echo "-------------"
grep "LEAK SUMMARY" "$REPORT" -A 6 | tail -7
echo ""

# Get totals
DEFINITE=$(grep "definitely lost:" "$REPORT" | grep -oP '\d+(?= bytes)' || echo "0")
INDIRECT=$(grep "indirectly lost:" "$REPORT" | grep -oP '\d+(?= bytes)' || echo "0")
POSSIBLE=$(grep "possibly lost:" "$REPORT" | grep -oP '\d+(?= bytes)' || echo "0")
SUPPRESSED=$(grep "suppressed:" "$REPORT" | grep -oP '[\d,]+(?= bytes)' | tr -d ',' || echo "0")

# Check for OpenTraderPlatform code in leaks
echo "CHECKING FOR OPENTRADERPLATFORM CODE LEAKS:"
echo "----------------------------------"

# Look for our source files in actual leak stack traces
# Exclude header lines (Command:, Reading syms, /path/to/gdb)
L2_LEAK_LINES=$(grep "Src/" "$REPORT" 2>/dev/null | \
    grep -v "Command:" | \
    grep -v "Reading syms" | \
    grep -v "/path/to/gdb" | \
    grep -v "^--" || echo "")

if [ -n "$L2_LEAK_LINES" ]; then
    L2_LEAK_COUNT=$(echo "$L2_LEAK_LINES" | wc -l)
    echo "⚠️  Found OpenTraderPlatform code in leak traces ($L2_LEAK_COUNT lines)!"
    echo ""
    echo "OpenTraderPlatform leak locations:"
    echo "$L2_LEAK_LINES" | head -20
    echo ""
    HAS_L2_LEAKS=true
else
    echo "✅ No OpenTraderPlatform code found in leak traces"
    echo "   All leaks are in external libraries (expected)"
    HAS_L2_LEAKS=false
fi

echo ""
echo "RESULTS:"
echo "--------"

if [ "$DEFINITE" = "0" ] && [ "$INDIRECT" = "0" ]; then
    echo "🎉 PERFECT! No definite or indirect memory leaks!"
    
    if [ "$POSSIBLE" != "0" ]; then
        echo ""
        echo "ℹ️  Note: $(printf "%'d" $POSSIBLE) bytes possibly lost"
        echo "   (Often false positives from Qt internal allocations)"
    fi
    
    if [ "$HAS_L2_LEAKS" = true ]; then
        echo ""
        echo "⚠️  However, OpenTraderPlatform code appears in some leak traces."
        echo "   Review the full report to investigate."
    fi
elif [ "$HAS_L2_LEAKS" = false ]; then
    echo "✅ Clean! All leaks are in external libraries:"
    echo "   - Definitely lost: $(printf "%'d" $DEFINITE) bytes (fontconfig, GTK, etc.)"
    echo "   - Indirectly lost: $(printf "%'d" $INDIRECT) bytes"
    if [ "$SUPPRESSED" != "0" ]; then
        echo "   - Suppressed: $(printf "%'d" $SUPPRESSED) bytes"
    fi
    echo ""
    echo "   These are NOT bugs in OpenTraderPlatform code."
else
    echo "❌ ACTION REQUIRED - Memory leaks in OpenTraderPlatform code:"
    echo "   - Definitely lost: $(printf "%'d" $DEFINITE) bytes"
    echo "   - Indirectly lost: $(printf "%'d" $INDIRECT) bytes"
    if [ "$POSSIBLE" != "0" ]; then
        echo "   - Possibly lost: $(printf "%'d" $POSSIBLE) bytes"
    fi
    echo ""
    echo "   Review the full report for details."
fi

echo ""
echo "ERROR SUMMARY:"
echo "--------------"
grep "ERROR SUMMARY" "$REPORT" | tail -1

echo ""
echo "For full details: less $REPORT"
