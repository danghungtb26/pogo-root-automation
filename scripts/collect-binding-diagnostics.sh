#!/usr/bin/env bash
set -euo pipefail

GOOGLE_PACKAGE=com.nianticlabs.pokemongo
GALAXY_PACKAGE=com.nianticlabs.pokemongo.ares
MODULE_STATUS_SCRIPT=/data/adb/modules/pogo_root_automation/bin/runtime-status.sh
OUTPUT="${1:-pogo-binding-diagnostics.txt}"

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

getprop_value() {
  adb shell getprop "$1" 2>/dev/null | tr -d '\r'
}

command -v adb >/dev/null 2>&1 || fail "adb is not installed"
adb get-state >/dev/null 2>&1 || fail "no adb device connected"

root_id="$(root_shell 'id -u' 2>/dev/null | tr -d '\r' || true)"
[[ "$root_id" == "0" ]] || fail "adb shell cannot obtain root through su"

status="$(root_shell "sh $MODULE_STATUS_SCRIPT" 2>/dev/null || true)"
process_name="$(printf '%s\n' "$status" | read_field process)"
pid="$(printf '%s\n' "$status" | read_field pid)"
package_name="$(printf '%s\n' "$status" | read_field package)"

if [[ -z "$package_name" ]]; then
  for candidate in "$GOOGLE_PACKAGE" "$GALAXY_PACKAGE"; do
    package_path="$(adb shell pm path "$candidate" 2>/dev/null | head -n 1 | tr -d '\r' || true)"
    if [[ "$package_path" == package:* ]]; then
      package_name="$candidate"
      break
    fi
  done
fi
[[ -n "$package_name" ]] || fail "official Pokémon GO package not found"

manufacturer="$(getprop_value ro.product.manufacturer)"
brand="$(getprop_value ro.product.brand)"
model="$(getprop_value ro.product.model)"
product="$(getprop_value ro.product.name)"
hardware="$(getprop_value ro.hardware)"
primary_abi="$(getprop_value ro.product.cpu.abi)"
supported_abis="$(getprop_value ro.product.cpu.abilist)"
native_bridge="$(getprop_value ro.dalvik.vm.native.bridge)"
identity="$(printf '%s %s %s %s %s' "$manufacturer" "$brand" "$model" "$product" "$hardware" | tr '[:upper:]' '[:lower:]')"
environment=android-device
if [[ "$identity" == *bluestacks* || "$identity" == *bstacks* ]]; then
  environment=bluestacks
fi

{
  echo "# pogo-root-automation binding diagnostics"
  echo
  echo "[runtime-status]"
  printf '%s\n' "$status"
  echo

  echo "[device]"
  echo "environment=$environment"
  echo "manufacturer=$manufacturer"
  echo "brand=$brand"
  echo "model=$model"
  echo "product=$product"
  echo "hardware=$hardware"
  echo "android_release=$(getprop_value ro.build.version.release)"
  echo "android_sdk=$(getprop_value ro.build.version.sdk)"
  echo "kernel_machine=$(adb shell uname -m | tr -d '\r')"
  echo "primary_abi=$primary_abi"
  echo "supported_abis=$supported_abis"
  echo "native_bridge=${native_bridge:-none}"
  echo "zygote=$(getprop_value ro.zygote)"
  echo

  echo "[package]"
  echo "package=$package_name"
  root_shell "dumpsys package '$package_name'" \
    | tr -d '\r' \
    | grep -E '^[[:space:]]*(versionName=|versionCode=|primaryCpuAbi=|secondaryCpuAbi=)' \
    | sed 's/^[[:space:]]*//'
  echo

  echo "[apk-paths-and-hashes]"
  while IFS= read -r apk_path; do
    [[ -n "$apk_path" ]] || continue
    apk_path="${apk_path#package:}"
    echo "path=$apk_path"
    root_shell "sha256sum '$apk_path' 2>/dev/null || toybox sha256sum '$apk_path' 2>/dev/null || true" | tr -d '\r'
  done < <(adb shell pm path "$package_name" | tr -d '\r')
  echo

  echo "[process]"
  echo "process=$process_name"
  echo "pid=$pid"
  if [[ "$pid" =~ ^[0-9]+$ ]] && (( pid > 0 )); then
    current_cmdline="$(root_shell "tr '\\000' ' ' < /proc/$pid/cmdline 2>/dev/null" | tr -d '\r' || true)"
    process_exe="$(root_shell "readlink /proc/$pid/exe 2>/dev/null" | tr -d '\r' || true)"
    echo "cmdline=$current_cmdline"
    echo "exe=$process_exe"
    echo

    echo "[native-mappings]"
    root_shell "cat /proc/$pid/maps 2>/dev/null" \
      | tr -d '\r' \
      | awk '$NF ~ /\.so($| \(deleted\)$)/ {print}' \
      | sort -u
  else
    echo "note=Pokémon GO process is not currently attached; launch the game and rerun to capture native mappings"
  fi
} > "$OUTPUT"

echo "Wrote $OUTPUT"
