#!/bin/bash
# gui_runner.sh — Launch kernelCAD on a virtual X display, run a Python test
# script that drives the UI, then capture results.
#
# Usage: ./gui_runner.sh <test_script.py> [test_args...]
#
# Output:
#   /tmp/kernelcad_gui_test/<test_name>/
#     screenshots/  - PNG screenshots
#     stdout.log    - test script output
#     kernelcad.log - kernelCAD's own stderr
#     events.json   - structured events from the test script

set -e

TEST_SCRIPT="$1"
shift
TEST_NAME=$(basename "$TEST_SCRIPT" .py)
TEST_DIR="/tmp/kernelcad_gui_test/$TEST_NAME"
mkdir -p "$TEST_DIR/screenshots"

KERNELCAD_BIN="/home/andrii/Projects/kernelCAD-desktop/build/src/kernelCAD"
DISPLAY_NUM=99
WIDTH=1920
HEIGHT=1080

if [ ! -x "$KERNELCAD_BIN" ]; then
    echo "ERROR: kernelCAD binary not found. Run cmake/make first."
    exit 1
fi

# Kill any leftover Xvfb on this display
pkill -f "Xvfb :$DISPLAY_NUM" 2>/dev/null || true
sleep 0.3

# Start Xvfb
echo "Starting Xvfb on :$DISPLAY_NUM (${WIDTH}x${HEIGHT})..."
Xvfb :$DISPLAY_NUM -screen 0 ${WIDTH}x${HEIGHT}x24 -ac +extension GLX +render -noreset &
XVFB_PID=$!
sleep 0.5

cleanup() {
    pkill -P $$ 2>/dev/null || true
    kill $XVFB_PID 2>/dev/null || true
    pkill -f "Xvfb :$DISPLAY_NUM" 2>/dev/null || true
}
trap cleanup EXIT

# Launch kernelCAD in background on the virtual display
echo "Launching kernelCAD..."
DISPLAY=:$DISPLAY_NUM "$KERNELCAD_BIN" "$@" > "$TEST_DIR/kernelcad.log" 2>&1 &
KERNELCAD_PID=$!

# Wait for window to appear
echo "Waiting for kernelCAD window..."
DISPLAY=:$DISPLAY_NUM timeout 15 bash -c '
    while ! xdotool search --name "kernelCAD" 2>/dev/null | head -1 > /dev/null; do
        sleep 0.3
    done
'
if [ $? -ne 0 ]; then
    echo "ERROR: kernelCAD window did not appear within 15s"
    cat "$TEST_DIR/kernelcad.log" | tail -20
    exit 1
fi
sleep 1.5  # let it fully render

# Run the test script with the right environment
echo "Running test: $TEST_NAME"
DISPLAY=:$DISPLAY_NUM \
    KERNELCAD_TEST_DIR="$TEST_DIR" \
    KERNELCAD_PID="$KERNELCAD_PID" \
    python3 "$TEST_SCRIPT" "$@" 2>&1 | tee "$TEST_DIR/stdout.log"
TEST_EXIT=$?

# Take a final screenshot before cleanup
DISPLAY=:$DISPLAY_NUM scrot "$TEST_DIR/screenshots/_final.png" 2>/dev/null || true

# Clean up kernelCAD
kill $KERNELCAD_PID 2>/dev/null || true
sleep 0.3
kill -9 $KERNELCAD_PID 2>/dev/null || true

echo "Test complete. Results in: $TEST_DIR"
exit $TEST_EXIT
