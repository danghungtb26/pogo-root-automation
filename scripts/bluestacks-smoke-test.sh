#!/usr/bin/env bash
set -euo pipefail

MODULE_DIR=/data/adb/modules/pogo_root_automation
MODULE_STATUS_SCRIPT="$MODULE_DIR/bin/runtime-status.sh"
GOOGLE_PACKAGE=com.nianticlabs.pokemongo
GALAXY_PACKAGE=com.nianticlabs.pokemongo.ares

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

root_shell() {
  adb shell su -c "$*"
}

getprop_value() {
  adb shell getprop "$1" 2>/dev/null | tr -d '\r'
}

command -v adb >/dev/null 2>&1 || fail "adb is not installed"
adb get-state >/dev/null 2>&1 || fail "no adb device connected"

manufacturer="$(getprop_value ro.product.manufacturer)"
brand="$(getprop_value ro.product.brand)"
model="$(getprop_value ro.product.model)"
product="$(getprop_value ro.product.name)"
hardware="$(getprop_value ro.hardware)"
primary_abi="$(getprop_value ro.product.cpu.abi)"
supported_abis="$(getprop_value ro.product.cpu.abilist)"
native_bridge="$(getprop_value ro.dalvik.vm.native.bridge)"

identity="$(printf '%s %s %s %s %s' "$manufacturer" "$brand" "$model" "$product" "$hardware" | tr '[:upper:]' '[:lower:]')"
if [[ "$identity" != *bluestacks* && "$identity" != *bstacks* ]]; then
  echo "Environment properties:"
  echo "  manufacturer=$manufacturer"
  echo "  brand=$brand"
  echo "  model=$model"
  echo "  product=$product"
  echo "  hardware=$hardware"
  fail "connected device does not identify as BlueStacks"
fi

root_id="$(root_shell 'id -u' 2>/dev/null | tr -d '\r' || true)"
[[ "$root_id" == "0" ]] || fail "BlueStacks ADB shell cannot obtain uid 0 through su"

root_shell "test -x '$MODULE_STATUS_SCRIPT'" \
  || fail "pogo-root-automation Magisk module is not installed/enabled"

module_abi=
case "$primary_abi" in
  x86_64) module_abi=x86_64 ;;
  arm64-v8a) module_abi=arm64-v8a ;;
  *)
    if [[ ",$supported_abis," == *,x86_64,* ]]; then
      module_abi=x86_64
    elif [[ ",$supported_abis," == *,arm64-v8a,* ]]; then
      module_abi=arm64-v8a
    else
      fail "unsupported BlueStacks ABI: primary=$primary_abi supported=$supported_abis"
    fi
    ;;
esac

root_shell "test -s '$MODULE_DIR/zygisk/$module_abi.so'" \
  || fail "Magisk module does not contain zygisk/$module_abi.so"

package_name=
for candidate in "$GOOGLE_PACKAGE" "$GALAXY_PACKAGE"; do
  package_path="$(adb shell pm path "$candidate" 2>/dev/null | head -n 1 | tr -d '\r' || true)"
  if [[ "$package_path" == package:* ]]; then
    package_name="$candidate"
    break
  fi
done
[[ -n "$package_name" ]] || fail "official Pokémon GO package not found in BlueStacks"

echo "BlueStacks runtime:"
echo "  manufacturer=$manufacturer"
echo "  model=$model"
echo "  android=$(getprop_value ro.build.version.release) / sdk=$(getprop_value ro.build.version.sdk)"
echo "  primary_abi=$primary_abi"
echo "  supported_abis=$supported_abis"
echo "  native_bridge=${native_bridge:-none}"
echo "  module_abi=$module_abi"
echo "  package=$package_name"

echo "Running generic lifecycle smoke test…"
"$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/device-smoke-test.sh"

echo "PASS: BlueStacks root/module/ABI checks and lifecycle smoke test completed"
