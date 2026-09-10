#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/lib/emulator.sh"
usage() {
  cat <<'EOF'
Usage: scripts/logcat-full.sh [--follow] [output.txt]
Save all available logcat buffers, all tags/priorities, as root.
Default: dump buffered logs and exit. --follow: keep recording until Ctrl+C.
Does not clear buffers. Already overwritten logs cannot be recovered.
Default output: build/logs/logcat-full-<timestamp>-<pid>.txt
EOF
}
FOLLOW=false
OUTPUT=
while [[ $# -gt 0 ]]; do
  case "$1" in
    --help|-h) usage; exit 0 ;;
    --follow) FOLLOW=true; shift ;;
    -*) usage >&2; fail "unknown option: $1" ;;
    *) [[ -z "$OUTPUT" ]] || fail "only one output path is accepted"; OUTPUT="$1"; shift ;;
  esac
done
OUTPUT="${OUTPUT:-$ROOT_DIR/build/logs/logcat-full-$(date +%Y%m%d-%H%M%S)-$$.txt}"
connect_emulator
require_root
mkdir -p "$(dirname "$OUTPUT")"
# Refuse to overwrite an earlier capture, including an existing symlink.
set -o noclobber
exec 3>"$OUTPUT"
set +o noclobber
echo "Writing all logcat buffers to $OUTPUT" >&2
if [[ "$FOLLOW" == true ]]; then
  echo "Recording buffered and new logs; press Ctrl+C to stop." >&2
  adb_target exec-out "su -c 'logcat -b all -v threadtime \"*:V\"'" >&3
else
  adb_target exec-out "su -c 'logcat -b all -v threadtime -d \"*:V\"'" >&3
  echo "Saved $OUTPUT" >&2
fi
