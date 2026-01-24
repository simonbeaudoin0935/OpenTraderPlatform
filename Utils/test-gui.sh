#!/bin/bash
# Test script for GUI application with Xvfb and xdotool
# This script is for local testing of the GUI test approach

set -e

echo "=== GUI Test Script ==="
echo "This script tests the L2Trader GUI application using Xvfb and xdotool"
echo ""

# Check if required tools are installed
command -v Xvfb >/dev/null 2>&1 || { echo "Error: Xvfb is not installed. Install with: sudo apt-get install xvfb"; exit 1; }
command -v xdotool >/dev/null 2>&1 || { echo "Error: xdotool is not installed. Install with: sudo apt-get install xdotool"; exit 1; }
command -v xdpyinfo >/dev/null 2>&1 || { echo "Error: xdpyinfo is not installed. Install with: sudo apt-get install x11-utils"; exit 1; }

# Check if L2Trader executable exists
if [ ! -f "./build/GUI/Src/L2Trader" ]; then
    echo "Error: L2Trader executable not found at ./build/GUI/Src/L2Trader"
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
echo "Starting L2Trader application..."
./build/GUI/Src/L2Trader &
APP_PID=$!

# Wait for application to start
echo "Waiting 5 seconds for application to start..."
sleep 5

# Find the window and send Ctrl+Q
echo "Looking for L2Trader window..."
WINDOW_ID=$(xdotool search --sync --onlyvisible --class "L2Trader" 2>/dev/null | head -1)

if [ -n "$WINDOW_ID" ]; then
    echo "Found window ID: $WINDOW_ID"
    # Activate the window and send Ctrl+Q
    echo "Sending Ctrl+Q to window..."
    xdotool windowactivate --sync "$WINDOW_ID"
    xdotool key --window "$WINDOW_ID" ctrl+q
    
    # Wait for application to exit (with timeout)
    echo "Waiting for application to exit..."
    TIMEOUT=10
    ELAPSED=0
    while kill -0 $APP_PID 2>/dev/null && [ $ELAPSED -lt $TIMEOUT ]; do
        sleep 1
        ELAPSED=$((ELAPSED + 1))
    done
    
    # Check if process exited
    if kill -0 $APP_PID 2>/dev/null; then
        echo "Application did not exit after Ctrl+Q within ${TIMEOUT}s, killing it"
        kill $APP_PID
        wait $APP_PID 2>/dev/null || true
        EXIT_CODE=$?
    else
        wait $APP_PID 2>/dev/null || true
        EXIT_CODE=$?
    fi
else
    echo "Window not found, application may have crashed or failed to start"
    wait $APP_PID 2>/dev/null || true
    EXIT_CODE=$?
fi

# Clean up Xvfb
echo "Cleaning up Xvfb..."
kill $XVFB_PID 2>/dev/null || true

echo ""
echo "Application exit code: $EXIT_CODE"

# Exit with 0 if app exited cleanly (0 or 143 for SIGTERM)
if [ $EXIT_CODE -eq 0 ] || [ $EXIT_CODE -eq 143 ]; then
    echo "✓ GUI test passed"
    exit 0
else
    echo "✗ GUI test failed with exit code $EXIT_CODE"
    exit 1
fi
