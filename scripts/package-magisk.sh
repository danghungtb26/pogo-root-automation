#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 <arm64-zygisk-so> <x86_64-zygisk-so> <output-zip>" >&2
  exit 2
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARM64_SO="$(realpath "$1")"
X86_64_SO="$(realpath "$2")"
OUTPUT_ZIP="$(realpath -m "$3")"
WORK_DIR="$ROOT_DIR/build/magisk-module"

for input in "$ARM64_SO" "$X86_64_SO"; do
  [[ -s "$input" ]] || {
    echo "missing or empty native library: $input" >&2
    exit 1
  }
done

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
cp -a "$ROOT_DIR/zygisk/module/." "$WORK_DIR/"
mkdir -p "$WORK_DIR/zygisk"
cp "$ARM64_SO" "$WORK_DIR/zygisk/arm64-v8a.so"
cp "$X86_64_SO" "$WORK_DIR/zygisk/x86_64.so"

[[ -s "$WORK_DIR/zygisk/arm64-v8a.so" ]]
[[ -s "$WORK_DIR/zygisk/x86_64.so" ]]

mkdir -p "$(dirname "$OUTPUT_ZIP")"
rm -f "$OUTPUT_ZIP"
(
  cd "$WORK_DIR"
  zip -qr "$OUTPUT_ZIP" .
)

unzip -l "$OUTPUT_ZIP" | grep -q 'zygisk/arm64-v8a.so'
unzip -l "$OUTPUT_ZIP" | grep -q 'zygisk/x86_64.so'

echo "$OUTPUT_ZIP"
