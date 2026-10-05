#!/bin/bash
# Test script for GUI application with Xvfb and xdotool
# This script runs a local GUI launch and shutdown smoke test.

set -e

echo "=== GUI Test Script ==="
echo "This script tests the OpenTraderPlatform GUI application using Xvfb and xdotool"
echo ""

# Check if required tools are installed
command -v Xvfb >/dev/null 2>&1 || { echo "Error: Xvfb is not installed. Install with: sudo apt-get install xvfb"; exit 1; }
command -v xdotool >/dev/null 2>&1 || { echo "Error: xdotool is not installed. Install with: sudo apt-get install xdotool"; exit 1; }
command -v xdpyinfo >/dev/null 2>&1 || { echo "Error: xdpyinfo is not installed. Install with: sudo apt-get install x11-utils"; exit 1; }
command -v import >/dev/null 2>&1 || { echo "Warning: imagemagick is not installed. Screenshots will be skipped. Install with: sudo apt-get install imagemagick"; }

# Check if OpenTraderPlatform executable exists
if [ ! -f "./build/Src/OpenTraderPlatform" ]; then
    echo "Error: OpenTraderPlatform executable not found at ./build/Src/OpenTraderPlatform"
    echo "Please build the GUI version first."
    exit 1
fi

echo "Starting Xvfb on display :99..."
Xvfb :99 -screen 0 1024x768x24 &
XVFB_PID=$!
export DISPLAY=:99

# Wait for Xvfb to be ready
sleep 2

# Verify display is available
if ! xdpyinfo -display :99 > /dev/null 2>&1; then
    echo "Error: Display :99 not available"
    kill $XVFB_PID 2>/dev/null || true
    exit 1
fi

echo "Display :99 is ready"

# Start the GUI application in background
echo "Starting OpenTraderPlatform application..."
./build/Src/OpenTraderPlatform &
APP_PID=$!

# Wait for application to start
echo "Waiting 5 seconds for application to start..."
sleep 5

# Find the window and send Ctrl+Q
echo "Looking for OpenTraderPlatform window..."
WINDOW_ID=$(xdotool search --onlyvisible --class "OpenTraderPlatform" 2>/dev/null | head -1)

if [ -n "$WINDOW_ID" ]; then
    echo "Found window ID: $WINDOW_ID"
    
    # Take a screenshot before sending Ctrl+Q (if imagemagick is available)
    if command -v import >/dev/null 2>&1; then
        mkdir -p build/gui-test-screenshots
        import -window root build/gui-test-screenshots/gui-test-before-quit.png
        echo "Screenshot saved to build/gui-test-screenshots/gui-test-before-quit.png"
    fi
    
    # Send Ctrl+Q directly to the window (no activation needed in Xvfb)
    echo "Sending Ctrl+Q to window..."
    xdotool key --window "$WINDOW_ID" ctrl+q
    
else
    echo "Window not found, application may have crashed or failed to start"
fi

# Poll for a bounded time so a stuck app cannot hang the smoke test. A timeout
# is always a failure.
echo "Waiting for application to exit..."
TIMEOUT=10
ELAPSED=0
APP_STATE=""
while [ "$ELAPSED" -lt "$TIMEOUT" ]; do
    APP_STATE=$(ps -o stat= -p "$APP_PID" 2>/dev/null || true)
    if [ -z "$APP_STATE" ] || [[ "$APP_STATE" == Z* ]]; then
        break
    fi
    sleep 1
    ELAPSED=$((ELAPSED + 1))
done

APP_STATE=$(ps -o stat= -p "$APP_PID" 2>/dev/null || true)
if [ -z "$APP_STATE" ] || [[ "$APP_STATE" == Z* ]]; then
    if wait "$APP_PID"; then
        EXIT_CODE=0
    else
        EXIT_CODE=$?
    fi
else
    echo "Application did not exit within ${TIMEOUT}s, killing it"
    kill "$APP_PID" 2>/dev/null || true
    wait "$APP_PID" 2>/dev/null || true
    EXIT_CODE=124
fi

# Clean up Xvfb
echo "Cleaning up Xvfb..."
kill $XVFB_PID 2>/dev/null || true

echo ""
echo "Application exit code: $EXIT_CODE"

# Exit with 0 only if the application exited cleanly.
if [ "$EXIT_CODE" -eq 0 ]; then
    echo "✓ GUI test passed"
    exit 0
else
    echo "✗ GUI test failed with exit code $EXIT_CODE"
    exit 1
fi
