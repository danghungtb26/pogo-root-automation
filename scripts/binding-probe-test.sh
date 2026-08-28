#!/usr/bin/env bash
set -euo pipefail

MODULE_STATUS_SCRIPT=/data/adb/modules/pogo_root_automation/bin/runtime-status.sh
GOOGLE_PACKAGE=com.nianticlabs.pokemongo
GALAXY_PACKAGE=com.nianticlabs.pokemongo.ares
TIMEOUT_SECONDS="${BINDING_PROBE_TIMEOUT_SECONDS:-45}"
DIAGNOSTICS_OUTPUT="${BINDING_DIAGNOSTICS_OUTPUT:-pogo-binding-diagnostics.txt}"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

root_shell() {
  adb shell su -c "$*"
}

read_field() {
  local key="$1"
  sed -n "s/^${key}=//p" | head -n 1 | tr -d '\r'
}

runtime_status() {
  root_shell "sh $MODULE_STATUS_SCRIPT" 2>/dev/null || true
}

command -v adb >/dev/null 2>&1 || fail "adb is not installed"
adb get-state >/dev/null 2>&1 || fail "no adb device connected"

root_id="$(root_shell 'id -u' 2>/dev/null | tr -d '\r' || true)"
[[ "$root_id" == "0" ]] || fail "adb shell cannot obtain uid 0 through su"
root_shell "test -x '$MODULE_STATUS_SCRIPT'" \
  || fail "pogo-root-automation Magisk module is not installed/enabled"

package_name=
for candidate in "$GOOGLE_PACKAGE" "$GALAXY_PACKAGE"; do
  package_path="$(adb shell pm path "$candidate" 2>/dev/null | head -n 1 | tr -d '\r' || true)"
  if [[ "$package_path" == package:* ]]; then
    package_name="$candidate"
    break
  fi
done
[[ -n "$package_name" ]] || fail "official Pokémon GO package not found"

adb shell monkey -p "$package_name" -c android.intent.category.LAUNCHER 1 >/dev/null

echo "Waiting up to ${TIMEOUT_SECONDS}s for runtime binding readiness…"
start_seconds=$SECONDS
last_status=
while (( SECONDS - start_seconds < TIMEOUT_SECONDS )); do
  status="$(runtime_status)"
  last_status="$status"
  runtime_state="$(printf '%s\n' "$status" | read_field runtime_state)"
  probe_state="$(printf '%s\n' "$status" | read_field probe_state)"
  engine="$(printf '%s\n' "$status" | read_field binding_engine)"

  if [[ "$runtime_state" == "connected" && "$probe_state" == "ready" ]]; then
    echo "PASS: binding probe ready"
    echo "  engine=$engine"
    echo "  primary_abi=$(printf '%s\n' "$status" | read_field device_primary_abi)"
    echo "  kernel_machine=$(printf '%s\n' "$status" | read_field kernel_machine)"
    echo "  translation_layer=$(printf '%s\n' "$status" | read_field translation_layer)"
    echo "  libil2cpp=$(printf '%s\n' "$status" | read_field libil2cpp_path)"
    exit 0
  fi

  if [[ "$runtime_state" == "connected" && "$probe_state" == "unity_loaded" ]]; then
    echo "INFO: Unity is loaded but libil2cpp is not visible yet"
  fi

  sleep 1
done

echo "Last runtime status:" >&2
printf '%s\n' "$last_status" >&2

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if "$script_dir/collect-binding-diagnostics.sh" "$DIAGNOSTICS_OUTPUT"; then
  echo "Diagnostics written to $DIAGNOSTICS_OUTPUT" >&2
fi

fail "binding probe did not become ready before timeout"
