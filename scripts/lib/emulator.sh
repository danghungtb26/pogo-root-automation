#!/usr/bin/env bash
# Shared by the host-side emulator scripts. Source from Bash.

fail() { echo "FAIL: $*" >&2; exit 1; }

adb_target() { adb -s "$ANDROID_SERIAL" "$@"; }

connect_emulator() {
  command -v adb >/dev/null 2>&1 || fail "adb is not installed or not in PATH"
  export ANDROID_SERIAL="${ANDROID_SERIAL:-127.0.0.1:5565}"
  local devices
  devices="$(adb devices -l 2>/dev/null || true)"
  if ! awk -v serial="$ANDROID_SERIAL" '$1 == serial && $2 == "device" {ok=1} END {exit !ok}' <<< "$devices"; then
    adb kill-server
    adb start-server
    if [[ "$ANDROID_SERIAL" == *:* ]]; then
      adb connect "$ANDROID_SERIAL" || true
    fi
    devices="$(adb devices -l)"
  fi
  printf '%s\n' "$devices" >&2
  awk -v serial="$ANDROID_SERIAL" '$1 == serial && $2 == "device" {ok=1} END {exit !ok}' <<< "$devices" \
    || fail "emulator $ANDROID_SERIAL is not connected/authorized; start BlueStacks Air 1"
}

require_root() {
  local uid
  uid="$(adb_target shell "su -c 'id -u'" | tr -d '\r')"
  [[ "$uid" == 0 ]] || fail "emulator cannot obtain root through su"
}
