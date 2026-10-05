#!/bin/bash
# UBSan (Undefined Behavior Sanitizer) test runner with clean output
# Detects undefined behavior like null pointer dereference, signed integer overflow, etc.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="$REPO_ROOT/build/UBSan"
APP="$BUILD_DIR/Src/OpenTraderPlatform"
UBSAN_LOG="$SCRIPT_DIR/ubsan-report.txt"

# UBSan environment options for better output
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0:log_path=$UBSAN_LOG:suppressions=$SCRIPT_DIR/ubsan.supp"

echo "═══════════════════════════════════════════════════════"
echo "  UBSan Test - OpenTraderPlatform"
echo "═══════════════════════════════════════════════════════"
echo ""

# Check if app exists
if [ ! -f "$APP" ]; then
    echo "❌ Error: Application not found at $APP"
    echo "Please build first with the 'build-ubsan' task"
    exit 1
fi

echo "Running application with UndefinedBehaviorSanitizer..."
echo "App: $APP"
echo "Log: $UBSAN_LOG.*"
echo ""
echo "NOTE: Application will start normally but UBSan will:"
echo "  1. Detect undefined behavior in real-time"
echo "  2. Print warnings to stderr"
echo "  3. Continue execution (non-fatal by default)"
echo "  4. Log all issues to $UBSAN_LOG.*"
echo ""
echo "Usage:"
echo "  1. Let app initialize"
echo "  2. Use normally (trigger features you want to test)"
echo "  3. Press Ctrl+Q to exit gracefully"
echo "  4. Review UBSan report"
echo ""
echo "Press Enter to continue..."
read

# Clean old logs
rm -f "$UBSAN_LOG".*

echo ""
echo "Starting application..."
echo "════════════════════════════════════════════════════════════"

# Run with UBSan - output goes to both terminal and log file
"$APP" 2>&1 | tee "$SCRIPT_DIR/ubsan-console.txt"

APP_EXIT_CODE=${PIPESTATUS[0]}

echo ""
echo "════════════════════════════════════════════════════════════"
echo "Application exited with code: $APP_EXIT_CODE"
echo ""

# Analyze results
"$SCRIPT_DIR/analyze-ubsan.sh"
