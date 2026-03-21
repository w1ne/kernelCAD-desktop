#!/usr/bin/env bash
# Helper to interact with the Fusion 360 VM (GNOME Boxes / libvirt)
set -euo pipefail

VM="win10"
CONN="qemu:///session"
BOXES_WID=""
SCREENSHOT_DIR="/tmp/fusion_docs"
mkdir -p "$SCREENSHOT_DIR"

export DISPLAY=:1

find_boxes_wid() {
    for wid in $(xdotool search --name "Boxes" 2>/dev/null); do
        local geom
        geom=$(xdotool getwindowgeometry "$wid" 2>/dev/null | grep Geometry | awk '{print $2}')
        local w=${geom%%x*}
        if [ "${w:-0}" -gt 100 ]; then
            BOXES_WID=$wid
            return 0
        fi
    done
    return 1
}

snap() {
    local name="${1:-screenshot}"
    virsh -c "$CONN" screenshot "$VM" "$SCREENSHOT_DIR/${name}.ppm" 2>/dev/null
    convert "$SCREENSHOT_DIR/${name}.ppm" "$SCREENSHOT_DIR/${name}.png" 2>/dev/null
    rm -f "$SCREENSHOT_DIR/${name}.ppm"
    echo "$SCREENSHOT_DIR/${name}.png"
}

sendkey() {
    virsh -c "$CONN" send-key "$VM" "$@" 2>/dev/null
}

click() {
    find_boxes_wid || { echo "Boxes window not found"; return 1; }
    xdotool mousemove --window "$BOXES_WID" "$1" "$2"
    sleep 0.15
    xdotool click 1
}

rclick() {
    find_boxes_wid || return 1
    xdotool mousemove --window "$BOXES_WID" "$1" "$2"
    sleep 0.15
    xdotool click 3
}

dblclick() {
    find_boxes_wid || return 1
    xdotool mousemove --window "$BOXES_WID" "$1" "$2"
    sleep 0.15
    xdotool click --repeat 2 --delay 100 1
}

case "${1:-help}" in
    snap)     snap "${2:-screenshot}" ;;
    key)      shift; sendkey "$@" ;;
    click)    click "$2" "$3" ;;
    rclick)   rclick "$2" "$3" ;;
    dblclick) dblclick "$2" "$3" ;;
    help)     echo "Usage: $0 {snap|key|click|rclick|dblclick} [args]" ;;
esac
