#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIB_DIR="$ROOT_DIR/app/libs"
UPSTREAM="https://raw.githubusercontent.com/Map-A-Droid/PogoEnhancer/main/app/libs"

mkdir -p "$LIB_DIR"

fetch_and_verify() {
  local name="$1"
  local expected_blob="$2"
  local expected_size="$3"
  local target="$LIB_DIR/$name"

  echo "Vendoring $name"
  curl -fL --retry 3 --retry-delay 2 "$UPSTREAM/$name" -o "$target.tmp"

  local actual_size
  actual_size="$(wc -c < "$target.tmp" | tr -d ' ')"
  if [[ "$actual_size" != "$expected_size" ]]; then
    echo "Size mismatch for $name: expected=$expected_size actual=$actual_size" >&2
    rm -f "$target.tmp"
    exit 1
  fi

  local actual_blob
  actual_blob="$(git hash-object "$target.tmp")"
  if [[ "$actual_blob" != "$expected_blob" ]]; then
    echo "Git blob SHA mismatch for $name: expected=$expected_blob actual=$actual_blob" >&2
    rm -f "$target.tmp"
    exit 1
  fi

  mv "$target.tmp" "$target"
}

fetch_and_verify "POGOProtos-2.60.8.jar" "e183c559262427fef17a9c82af585e67dab4e753" "41022349"
fetch_and_verify "bcpkix-jdk15on-1.60.jar" "87ce8b473aeb3200c6ab63af512e25030f180658" "796532"
fetch_and_verify "bcprov-jdk15on-1.60.jar" "5be567cbb3dee2cc616602a479b97f05d0498efd" "4189874"
fetch_and_verify "parser-1.6.0.aar" "477b7512e2aa76283330de9e659635a4f9a19680" "37754"
fetch_and_verify "virtualjoystick-1.10.1.aar" "de697c4501bf8b2fac31343b1bee7f572eee208c" "9575"

echo "PogoEnhancer libraries vendored successfully."
