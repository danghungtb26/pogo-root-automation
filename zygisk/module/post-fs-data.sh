#!/system/bin/sh

STATE_DIR=/data/adb/pogo_root_automation
mkdir -p "$STATE_DIR"
chmod 700 "$STATE_DIR"
rm -f "$STATE_DIR/runtime.status.tmp"
