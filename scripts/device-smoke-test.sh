#!/usr/bin/env bash
set -euo pipefail

MODULE_STATUS_SCRIPT=/data/adb/modules/pogo_root_automation/bin/runtime-status.sh
GOOGLE_PACKAGE=com.nianticlabs.pokemongo
GALAXY_PACKAGE=com.nianticlabs.pokemongo.ares

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

adb_shell() {
  adb shell "$@"
}

root_shell() {
  adb shell su -c "$*"
}

read_field() {
  local key="$1"
  sed -n "s/^${key}=//p" | head -n 1 | tr -d '\r'
}

runtime_status() {
  root_shell "sh $MODULE_STATUS_SCRIPT"
}

wait_for_state() {
  local expected="$1"
  local attempts="${2:-10}"
  local index status state

  for ((index = 1; index <= attempts; index++)); do
    status="$(runtime_status 2>/dev/null || true)"
    state="$(printf '%s\n' "$status" | read_field runtime_state)"
    if [[ "$state" == "$expected" ]]; then
      printf '%s\n' "$status"
      return 0
    fi
    sleep 1
  done

  echo "Last status:" >&2
  runtime_status >&2 || true
  return 1
}

command -v adb >/dev/null 2>&1 || fail "adb is not installed"
adb get-state >/dev/null 2>&1 || fail "no adb device connected"

root_id="$(root_shell 'id -u' 2>/dev/null | tr -d '\r' || true)"
[[ "$root_id" == "0" ]] || fail "adb shell cannot obtain root through su"

root_shell "test -x $MODULE_STATUS_SCRIPT" || fail "Magisk module is not installed/enabled; install the CI Magisk zip and reboot first"

package_name=
for candidate in "$GOOGLE_PACKAGE" "$GALAXY_PACKAGE"; do
  package_path="$(adb_shell pm path "$candidate" 2>/dev/null | head -n 1 | tr -d '\r' || true)"
  if [[ "$package_path" == package:* ]]; then
    package_name="$candidate"
    break
  fi
done
[[ -n "$package_name" ]] || fail "official Pokémon GO package not found"

echo "Package: $package_name"
echo "Starting Pokémon GO…"
adb_shell monkey -p "$package_name" -c android.intent.category.LAUNCHER 1 >/dev/null

connected="$(wait_for_state connected 15)" || fail "runtime did not report connected after launch"
printf '%s\n' "$connected"

reported_package="$(printf '%s\n' "$connected" | read_field package)"
[[ "$reported_package" == "$package_name" ]] || fail "runtime reported package '$reported_package' instead of '$package_name'"

version_name="$(printf '%s\n' "$connected" | read_field version_name)"
[[ -n "$version_name" ]] || fail "game versionName was not reported"

echo "Stopping Pokémon GO…"
adb_shell am force-stop "$package_name"
wait_for_state disconnected 10 >/dev/null || fail "runtime did not report disconnected after force-stop"

echo "Relaunching Pokémon GO…"
adb_shell monkey -p "$package_name" -c android.intent.category.LAUNCHER 1 >/dev/null
wait_for_state connected 15 >/dev/null || fail "runtime did not reconnect after relaunch"

echo "PASS: M1 lifecycle bridge connected → disconnected → connected; version=$version_name"
