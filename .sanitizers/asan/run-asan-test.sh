#!/bin/bash
# ASan (Address Sanitizer) test runner with clean output
# Detects memory errors like buffer overflows, use-after-free, memory leaks, etc.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="$REPO_ROOT/build/ASan"
APP="$BUILD_DIR/Src/L2Trader"
ASAN_LOG="$SCRIPT_DIR/asan-report.txt"

# ASan environment options for better output
export ASAN_OPTIONS="log_path=$ASAN_LOG:halt_on_error=0:detect_leaks=1:suppressions=$SCRIPT_DIR/asan.supp"

echo "═══════════════════════════════════════════════════════"
echo "  ASan Test - L2Trader"
echo "═══════════════════════════════════════════════════════"
echo ""

# Check if app exists
if [ ! -f "$APP" ]; then
    echo "❌ Error: Application not found at $APP"
    echo "Please build first with the 'build-asan' task"
    exit 1
fi

echo "Running application with AddressSanitizer..."
echo "App: $APP"
echo "Log: $ASAN_LOG.*"
echo ""
echo "NOTE: Application will start normally but ASan will:"
echo "  1. Detect memory errors in real-time"
echo "  2. Print warnings to stderr"
echo "  3. Continue execution (non-fatal by default)"
echo "  4. Log all issues to $ASAN_LOG.*"
echo ""
echo "Usage:"
echo "  1. Let app initialize"
echo "  2. Use normally (trigger features you want to test)"
echo "  3. Press Ctrl+Q to exit gracefully"
echo "  4. Review ASan report"
echo ""
echo "Press Enter to continue..."
read

# Clean old logs
rm -f "$ASAN_LOG".*

echo ""
echo "Starting application..."
echo "════════════════════════════════════════════════════════════"

# Run with ASan - output goes to both terminal and log file
"$APP" 2>&1 | tee "$SCRIPT_DIR/asan-console.txt"

APP_EXIT_CODE=${PIPESTATUS[0]}

echo ""
echo "════════════════════════════════════════════════════════════"
echo "Application exited with code: $APP_EXIT_CODE"
echo ""

# Analyze results
"$SCRIPT_DIR/analyze-asan.sh"
