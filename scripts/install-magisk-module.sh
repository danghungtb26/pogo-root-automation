#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/lib/emulator.sh"
usage() {
  cat <<'EOF'
Usage: scripts/install-magisk-module.sh [--apk controller.apk] [--reboot]
Unzip/check the ZIP uploaded by push-emulator.sh, then install through Magisk.
The optional controller APK is installed from a local file with adb install -r.
Requires working root and an existing Magisk installation. Enable Zygisk in settings.
Reboot is opt-in; otherwise restart BlueStacks Air 1 yourself after installation.
EOF
}
CONTROLLER_APK=
REBOOT=false
while [[ $# -gt 0 ]]; do
  case "$1" in
    --help|-h) usage; exit 0 ;;
    --apk)
      [[ $# -ge 2 && -s "$2" ]] || fail "$1 requires a local APK file"
      CONTROLLER_APK="$2"
      shift 2 ;;
    --reboot) REBOOT=true; shift ;;
    *) usage >&2; fail "unknown argument: $1" ;;
  esac
done
connect_emulator
require_root
if [[ -n "$CONTROLLER_APK" ]]; then adb_target install -r "$CONTROLLER_APK"; fi
adb_target shell "su -c 'sh -s'" <<'DEVICE_SCRIPT'
set -eu
fail() { echo "FAIL: $*" >&2; exit 1; }
module_zip=/data/local/tmp/pogo-root-automation/module.zip
test -s "$module_zip" || fail "run scripts/push-emulator.sh first"
magisk_bin=$(command -v magisk || true)
if [ -z "$magisk_bin" ]; then
  for candidate in /debug_ramdisk/magisk /sbin/magisk /data/adb/magisk/magisk; do
    if [ -x "$candidate" ]; then magisk_bin=$candidate; break; fi
  done
fi
test -n "$magisk_bin" || fail "Magisk runtime is not installed; an APK alone is insufficient"
"$magisk_bin" -v
magisk_path=$("$magisk_bin" --path)
busybox_bin=
for candidate in "$magisk_path/.magisk/busybox/busybox" /data/adb/magisk/busybox; do
  if [ -x "$candidate" ]; then busybox_bin=$candidate; break; fi
done
unzip_module() {
  if [ -n "$busybox_bin" ]; then "$busybox_bin" unzip "$@"; else unzip "$@"; fi
}
work_dir=$(mktemp -d /data/local/tmp/pogo-module-check.XXXXXX)
trap 'rm -rf "$work_dir"' EXIT
unzip_module -o "$module_zip" -d "$work_dir" >/dev/null
test "$(sed -n 's/^id=//p' "$work_dir/module.prop" | tr -d '\r')" = pogo_root_automation \
  || fail "unexpected module id"
for abi in arm64-v8a x86_64; do
  test -s "$work_dir/zygisk/$abi.so" || fail "missing library for $abi"
done
"$magisk_bin" --install-module "$module_zip"
DEVICE_SCRIPT
echo "Magisk module installation succeeded. Enable Zygisk in Magisk settings."
if [[ "$REBOOT" == true ]]; then
  adb_target reboot
else
  echo "Restart BlueStacks Air 1 (or run adb -s $ANDROID_SERIAL reboot) to activate the module."
fi
