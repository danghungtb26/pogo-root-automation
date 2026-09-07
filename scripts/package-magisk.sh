#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <arm64-zygisk-so> <output-zip>" >&2
  exit 2
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INPUT_SO="$(realpath "$1")"
OUTPUT_ZIP="$(realpath -m "$2")"
WORK_DIR="$ROOT_DIR/build/magisk-module"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
cp -a "$ROOT_DIR/zygisk/module/." "$WORK_DIR/"
mkdir -p "$WORK_DIR/zygisk"
cp "$INPUT_SO" "$WORK_DIR/zygisk/arm64-v8a.so"

mkdir -p "$(dirname "$OUTPUT_ZIP")"
rm -f "$OUTPUT_ZIP"
(
  cd "$WORK_DIR"
  zip -qr "$OUTPUT_ZIP" .
)

echo "$OUTPUT_ZIP"
