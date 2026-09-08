#!/system/bin/sh

STATE_DIR=/data/adb/pogo_root_automation
mkdir -p "$STATE_DIR"
# Keep the state directory traversable for diagnostics and UID registration.
# The bridge transport is abstract; authorization still uses SO_PEERCRED and
# the controller.uids allowlist, so this directory mode is not the boundary.
chmod 711 "$STATE_DIR"
rm -f "$STATE_DIR/runtime.status.tmp"
rm -f "$STATE_DIR/runtime.sock"
