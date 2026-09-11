#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/build-magisk.sh [output.zip]
Build arm64-v8a + x86_64 libraries and package the Magisk module.
Environment: ANDROID_HOME (or ANDROID_SDK_ROOT), ANDROID_NDK,
  ANDROID_CMAKE, ANDROID_NINJA, ZYGISK_API_DIR, BUILD_DIR, BUILD_JOBS.
Defaults: NDK 28.2.13676358, CMake 3.22.1, pinned Zygisk API 4.
The controller APK is built separately with ./gradlew assembleDebug.
EOF
}
if [[ "${1:-}" == --help || "${1:-}" == -h ]]; then usage; exit 0; fi
[[ $# -le 1 && "${1:-}" != -* ]] || { usage >&2; exit 2; }
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }
SDK_DIR="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
if [[ -z "$SDK_DIR" ]]; then
  for candidate in "$HOME/Library/Android/sdk" "$HOME/Android/Sdk"; do
    if [[ -d "$candidate" ]]; then SDK_DIR="$candidate"; break; fi
  done
fi
[[ -n "$SDK_DIR" ]] || fail "set ANDROID_HOME to your Android SDK"
NDK_DIR="${ANDROID_NDK:-$SDK_DIR/ndk/28.2.13676358}"
CMAKE_BIN="${ANDROID_CMAKE:-$SDK_DIR/cmake/3.22.1/bin/cmake}"
NINJA_BIN="${ANDROID_NINJA:-$(dirname "$CMAKE_BIN")/ninja}"
[[ -f "$NDK_DIR/build/cmake/android.toolchain.cmake" ]] \
  || fail "NDK not found at $NDK_DIR; install ndk;28.2.13676358 or set ANDROID_NDK"
[[ -x "$CMAKE_BIN" && -x "$NINJA_BIN" ]] || fail "install cmake;3.22.1 or set ANDROID_CMAKE and ANDROID_NINJA"
for tool in zip unzip realpath; do command -v "$tool" >/dev/null || fail "missing $tool"; done
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
mkdir -p "$BUILD_DIR"
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"
OUTPUT_ZIP="${1:-$BUILD_DIR/pogo-root-automation-magisk-multiabi.zip}"
mkdir -p "$(dirname "$OUTPUT_ZIP")"
API_DIR="${ZYGISK_API_DIR:-$BUILD_DIR/zygisk-api-4}"
API_COMMIT=7bb941ac8edfcffd1d23761e401c45ca95409dc1
if [[ -z "${ZYGISK_API_DIR:-}" && ! -s "$API_DIR/zygisk.hpp" ]]; then
  command -v curl >/dev/null || fail "curl is required to fetch zygisk.hpp"
  mkdir -p "$API_DIR"
  HEADER_TEMP="$(mktemp "$API_DIR/zygisk.hpp.XXXXXX")"
  trap 'rm -f "$HEADER_TEMP"' EXIT
  curl --fail --location --retry 3 \
    "https://raw.githubusercontent.com/topjohnwu/zygisk-module-sample/$API_COMMIT/module/jni/zygisk.hpp" \
    -o "$HEADER_TEMP"
  mv "$HEADER_TEMP" "$API_DIR/zygisk.hpp"
fi
[[ -s "$API_DIR/zygisk.hpp" ]] || fail "missing $API_DIR/zygisk.hpp"
API_DIR="$(cd "$API_DIR" && pwd)"
for abi in arm64-v8a x86_64; do
  build_name="$abi"
  [[ "$abi" != arm64-v8a ]] || build_name=arm64
  "$CMAKE_BIN" -S "$ROOT_DIR/zygisk/jni" -B "$BUILD_DIR/zygisk-$build_name" -G Ninja \
    "-DCMAKE_TOOLCHAIN_FILE=$NDK_DIR/build/cmake/android.toolchain.cmake" \
    "-DCMAKE_MAKE_PROGRAM=$NINJA_BIN" "-DANDROID_ABI=$abi" \
    -DANDROID_PLATFORM=android-28 "-DZYGISK_API_DIR=$API_DIR"
  "$CMAKE_BIN" --build "$BUILD_DIR/zygisk-$build_name" --parallel "${BUILD_JOBS:-4}"
done
"$ROOT_DIR/scripts/package-magisk.sh" \
  "$BUILD_DIR/zygisk-arm64/libpogo_root_automation.so" \
  "$BUILD_DIR/zygisk-x86_64/libpogo_root_automation.so" "$OUTPUT_ZIP"
