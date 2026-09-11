#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/lib/emulator.sh"
usage() {
  echo "Usage: scripts/push-emulator.sh [module.zip]"
  echo "Push to /data/local/tmp/pogo-root-automation/module.zip on ANDROID_SERIAL."
}
if [[ "${1:-}" == --help || "${1:-}" == -h ]]; then usage; exit 0; fi
[[ $# -le 1 && "${1:-}" != -* ]] || { usage >&2; exit 2; }
MODULE_ZIP="${1:-$ROOT_DIR/build/pogo-root-automation-magisk-multiabi.zip}"
[[ -s "$MODULE_ZIP" ]] || fail "missing $MODULE_ZIP; run scripts/build-magisk.sh first"
command -v unzip >/dev/null || fail "unzip is not installed"
unzip -t "$MODULE_ZIP" >/dev/null
for entry in module.prop zygisk/arm64-v8a.so zygisk/x86_64.so; do
  unzip -p "$MODULE_ZIP" "$entry" | wc -c | awk '$1 > 0 {ok=1} END {exit !ok}' \
    || fail "missing or empty ZIP entry: $entry"
done
connect_emulator
adb_target shell mkdir -p /data/local/tmp/pogo-root-automation
adb_target push "$MODULE_ZIP" /data/local/tmp/pogo-root-automation/module.zip
echo "Pushed module ZIP. Next: scripts/install-magisk-module.sh"
