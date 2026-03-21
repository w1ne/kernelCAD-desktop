#!/usr/bin/env bash
# kernelCAD UI test harness
# Usage:
#   ./scripts/ui_test.sh launch          — start app in background
#   ./scripts/ui_test.sh screenshot      — capture window screenshot
#   ./scripts/ui_test.sh click_menu M I  — click menu M, then item I (by name)
#   ./scripts/ui_test.sh key <key>       — send keypress (e.g. "ctrl+n")
#   ./scripts/ui_test.sh kill            — close the app
#   ./scripts/ui_test.sh status          — check if running
#   ./scripts/ui_test.sh full_test       — launch, create box, screenshot, kill

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP="$SCRIPT_DIR/../build/src/kernelCAD"
PIDFILE="/tmp/kernelcad_test.pid"
XVFB_PIDFILE="/tmp/kernelcad_xvfb.pid"
SCREENSHOT_DIR="/tmp/kernelcad_screenshots"
VIRTUAL_DISPLAY=":99"
export DISPLAY="$VIRTUAL_DISPLAY"
mkdir -p "$SCREENSHOT_DIR"

start_xvfb() {
    if [[ -f "$XVFB_PIDFILE" ]] && kill -0 "$(cat "$XVFB_PIDFILE")" 2>/dev/null; then
        return 0
    fi
    Xvfb "$VIRTUAL_DISPLAY" -screen 0 1920x1080x24 &>/dev/null &
    echo $! > "$XVFB_PIDFILE"
    sleep 0.5
    echo "Xvfb started on $VIRTUAL_DISPLAY"
}

stop_xvfb() {
    if [[ -f "$XVFB_PIDFILE" ]]; then
        kill "$(cat "$XVFB_PIDFILE")" 2>/dev/null || true
        rm -f "$XVFB_PIDFILE"
        echo "Xvfb stopped"
    fi
}

get_wid() {
    # Find the kernelCAD window by PID to avoid false matches
    local pid=""
    if [[ -f "$PIDFILE" ]]; then
        pid=$(cat "$PIDFILE")
    fi
    for i in $(seq 1 20); do
        local wid=""
        if [[ -n "$pid" ]]; then
            wid=$(xdotool search --pid "$pid" --name "kernelCAD" 2>/dev/null | head -1) || true
        fi
        if [[ -z "$wid" ]]; then
            wid=$(xdotool search --class "kernelCAD" 2>/dev/null | head -1) || true
        fi
        if [[ -n "$wid" ]]; then
            echo "$wid"
            return 0
        fi
        sleep 0.3
    done
    echo "ERROR: kernelCAD window not found" >&2
    return 1
}

cmd_launch() {
    if [[ -f "$PIDFILE" ]] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
        echo "Already running (PID $(cat "$PIDFILE"))"
        return 0
    fi
    start_xvfb
    "$APP" &
    echo $! > "$PIDFILE"
    echo "Launched kernelCAD (PID $(cat "$PIDFILE")) on $VIRTUAL_DISPLAY"
    # Wait for window to appear, then resize (no WM on Xvfb)
    local wid
    if wid=$(get_wid); then
        xdotool windowsize "$wid" 1440 900 2>/dev/null || true
        xdotool windowfocus "$wid" 2>/dev/null || true
        sleep 0.3
        echo "Window ready ($(xdotool getwindowgeometry "$wid" 2>/dev/null | grep Geometry))"
    else
        echo "WARNING: window did not appear within 6s"
    fi
}

cmd_screenshot() {
    local name="${1:-screenshot_$(date +%s)}"
    local wid
    wid=$(get_wid) || exit 1
    local outfile="$SCREENSHOT_DIR/${name}.png"
    # Focus the window first, then use xdotool to get geometry and capture with import
    xdotool windowactivate --sync "$wid" 2>/dev/null || true
    xdotool windowfocus --sync "$wid" 2>/dev/null || true
    xdotool windowraise "$wid" 2>/dev/null || true
    sleep 0.5
    # Get window geometry
    local geom
    geom=$(xdotool getwindowgeometry --shell "$wid" 2>/dev/null) || true
    # Capture the specific window via xwd
    xwd -id "$wid" -silent 2>/dev/null | convert xwd:- "$outfile" 2>/dev/null
    if [[ ! -s "$outfile" ]]; then
        # Fallback: crop from root
        eval "$(xdotool getwindowgeometry --shell "$wid" 2>/dev/null)" || true
        import -window root -crop "${WIDTH:-1920}x${HEIGHT:-1080}+${X:-0}+${Y:-0}" +repage "$outfile"
    fi
    echo "$outfile"
}

cmd_click_menu() {
    local menu_name="$1"
    local item_name="${2:-}"
    local wid
    wid=$(get_wid) || exit 1

    # Activate window
    xdotool windowactivate --sync "$wid"
    sleep 0.2

    # Use xdotool to send Alt+<first letter> to open menu
    local key
    case "$menu_name" in
        File)     key="alt+f" ;;
        Edit)     key="alt+e" ;;
        View)     key="alt+v" ;;
        Sketch)   key="alt+s" ;;
        Model)    key="alt+m" ;;
        Assembly) key="alt+a" ;;
        *)        echo "Unknown menu: $menu_name" >&2; return 1 ;;
    esac

    xdotool key "$key"
    sleep 0.3

    if [[ -n "$item_name" ]]; then
        # Use arrow keys to navigate to the item, then Enter to select.
        # $item_name can be an integer (1-based position) or a known name.
        local pos=1
        case "$item_name" in
            "Create Box") pos=1 ;;  # First item under Model
            "New")        pos=1 ;;  # First item under File
            "Open")       pos=2 ;;
            "Save")       pos=3 ;;
            "Export STEP") pos=5 ;; # after separator
            "Quit")       pos=7 ;;  # after separator
            [0-9]*)       pos="$item_name" ;;
            *)            pos=1 ;;
        esac
        for ((i=0; i<pos; i++)); do
            xdotool key Down
            sleep 0.1
        done
        xdotool key Return
        sleep 0.3
    fi
}

cmd_key() {
    local wid
    wid=$(get_wid) || exit 1
    xdotool windowactivate --sync "$wid"
    sleep 0.1
    xdotool key "$1"
}

cmd_kill() {
    if [[ -f "$PIDFILE" ]]; then
        local pid
        pid=$(cat "$PIDFILE")
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid"
            echo "Killed kernelCAD (PID $pid)"
        else
            echo "Process $pid already dead"
        fi
        rm -f "$PIDFILE"
    else
        pkill -f "build/src/kernelCAD" 2>/dev/null && echo "Killed" || echo "Not running"
    fi
    stop_xvfb
}

cmd_status() {
    if [[ -f "$PIDFILE" ]] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
        echo "Running (PID $(cat "$PIDFILE"))"
        get_wid > /dev/null && echo "Window visible" || echo "No window found"
    else
        echo "Not running"
    fi
}

cmd_full_test() {
    echo "=== kernelCAD UI Test ==="

    # Launch
    cmd_launch
    sleep 1

    # Take initial screenshot
    echo "--- Initial state ---"
    cmd_screenshot "01_initial"

    # Create a box via Model menu
    echo "--- Creating box ---"
    cmd_click_menu "Model" "Create Box"
    sleep 1

    # Screenshot after box creation
    cmd_screenshot "02_after_box"

    echo "--- Done ---"
    echo "Screenshots in $SCREENSHOT_DIR/"
    ls -la "$SCREENSHOT_DIR/"
}

# Dispatch
case "${1:-help}" in
    launch)      cmd_launch ;;
    screenshot)  cmd_screenshot "${2:-}" ;;
    click_menu)  cmd_click_menu "${2:-}" "${3:-}" ;;
    key)         cmd_key "${2:-}" ;;
    kill)        cmd_kill ;;
    status)      cmd_status ;;
    full_test)   cmd_full_test ;;
    help)
        echo "Usage: $0 {launch|screenshot|click_menu|key|kill|status|full_test}"
        ;;
    *)
        echo "Unknown command: $1" >&2
        exit 1
        ;;
esac
