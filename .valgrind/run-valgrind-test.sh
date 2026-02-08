#!/bin/bash
# Valgrind memory leak test with automated shutdown
# Tests for memory leaks during normal operation and graceful shutdown

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$REPO_ROOT/build/GUI"
APP="$BUILD_DIR/Src/L2Trader"
VALGRIND_LOG="$SCRIPT_DIR/valgrind-shutdown-test.txt"
SUPPRESSIONS="$SCRIPT_DIR/valgrind-qt.supp"

echo "═══════════════════════════════════════════════════════"
echo "  Valgrind Memory Leak Test - L2Trader"
echo "═══════════════════════════════════════════════════════"
echo ""

# Check if app exists
if [ ! -f "$APP" ]; then
    echo "❌ Error: Application not found at $APP"
    echo "Please build first with: cmake --build ./build/GUI"
    exit 1
fi

# Check if suppressions file exists
if [ ! -f "$SUPPRESSIONS" ]; then
    echo "⚠️  Warning: Suppressions file not found at $SUPPRESSIONS"
    echo "Continuing without suppressions..."
    SUPP_ARG=""
else
    SUPP_ARG="--suppressions=$SUPPRESSIONS"
fi

echo "Starting valgrind memory leak test..."
echo "App: $APP"
echo "Log: $VALGRIND_LOG"
echo ""
echo "⏳ Running application under valgrind..."
echo "   (This will be SLOW - valgrind adds 10-50x overhead)"
echo ""
echo "NOTE: Application will start, you should:"
echo "  1. Wait a few seconds for initialization"
echo "  2. Press Ctrl+Q to trigger graceful shutdown"
echo "  3. Valgrind will analyze memory and exit"
echo ""
echo "Press Enter to continue..."
read

# Run valgrind with comprehensive leak checking
valgrind \
    --leak-check=full \
    --show-leak-kinds=definite,possible \
    --track-origins=yes \
    --show-reachable=no \
    --num-callers=20 \
    --log-file="$VALGRIND_LOG" \
    $SUPP_ARG \
    "$APP" 2>&1

echo ""
echo "═══════════════════════════════════════════════════════"
echo "  Valgrind Test Complete"
echo "═══════════════════════════════════════════════════════"
echo ""

# Parse results
if [ -f "$VALGRIND_LOG" ]; then
    echo "📄 Full report: $VALGRIND_LOG"
    echo ""
    
    # Extract summary
    echo "Summary:"
    echo "--------"
    grep "LEAK SUMMARY" "$VALGRIND_LOG" -A 5 || echo "No leak summary found"
    echo ""
    
    # Check for definite leaks
    DEFINITE_LEAKS=$(grep "definitely lost:" "$VALGRIND_LOG" | grep -oP '\d+(?= bytes)' || echo "0")
    POSSIBLE_LEAKS=$(grep "possibly lost:" "$VALGRIND_LOG" | grep -oP '\d+(?= bytes)' || echo "0")
    
    echo "Results:"
    echo "--------"
    if [ "$DEFINITE_LEAKS" = "0" ]; then
        echo "✅ No definite memory leaks detected!"
    else
        echo "⚠️  Definite leaks: $DEFINITE_LEAKS bytes"
    fi
    
    if [ "$POSSIBLE_LEAKS" = "0" ]; then
        echo "✅ No possible memory leaks detected!"
    else
        echo "⚠️  Possible leaks: $POSSIBLE_LEAKS bytes (may be false positives)"
    fi
    
    echo ""
    echo "To see full details: cat $VALGRIND_LOG"
else
    echo "❌ Error: Valgrind log file not created"
    exit 1
fi
