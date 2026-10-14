#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 5 ]]; then
  echo "usage: $0 <arm64-zygisk-so> <x86_64-zygisk-so> <arm64-core-so> <x86_64-core-so> <output-zip>" >&2
  exit 2
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARM64_SO="$(realpath "$1")"
X86_64_SO="$(realpath "$2")"
ARM64_CORE_SO="$(realpath "$3")"
X86_64_CORE_SO="$(realpath "$4")"
OUTPUT_ZIP_INPUT="$5"
if [[ -e "$OUTPUT_ZIP_INPUT" ]]; then
  OUTPUT_ZIP="$(realpath "$OUTPUT_ZIP_INPUT")"
else
  OUTPUT_ZIP="$(realpath "$(dirname "$OUTPUT_ZIP_INPUT")")/$(basename "$OUTPUT_ZIP_INPUT")"
fi
OUTPUT_MANIFEST="${OUTPUT_ZIP%.zip}.manifest.json"
WORK_DIR="$ROOT_DIR/build/magisk-module"
MODULE_PROP="$ROOT_DIR/zygisk/module/module.prop"
TARGET_PACKAGE=com.nianticlabs.pokemongo
TARGET_VERSION_NAME=0.433.0
TARGET_VERSION_CODE=2026100702
TARGET_ABI=arm64-v8a
TARGET_IL2CPP_BUILD_ID=da59ce3024c4ccfe16fa6616281a4d111de67aa2
TARGET_METADATA_SHA256=f5961a8548256da287b4f79466a169c302b90ec59be7f6461e8e58b482c0fb0d
TARGET_BASE_APK_SHA256=1c8f198cd212e7303e5a5e38e34857f4ced42eb432ac62b07807bfd286503690
CORE_PROTOCOL=1
READELF_BIN="${ANDROID_READELF:-}"
[[ -x "$READELF_BIN" ]] || {
  echo "set ANDROID_READELF to the NDK llvm-readelf" >&2
  exit 1
}

[[ -s "$MODULE_PROP" ]] || {
  echo "missing Magisk module metadata: $MODULE_PROP" >&2
  exit 1
}
MODULE_ID="$(sed -n 's/^id=//p' "$MODULE_PROP")"
MODULE_VERSION="$(sed -n 's/^version=//p' "$MODULE_PROP")"
MODULE_VERSION_CODE="$(sed -n 's/^versionCode=//p' "$MODULE_PROP")"
[[ "$MODULE_ID" == pogo_root_automation && -n "$MODULE_VERSION" && -n "$MODULE_VERSION_CODE" ]] || {
  echo "invalid Magisk module metadata in $MODULE_PROP" >&2
  exit 1
}

for input in "$ARM64_SO" "$X86_64_SO" "$ARM64_CORE_SO" "$X86_64_CORE_SO"; do
  [[ -s "$input" ]] || {
    echo "missing or empty native library: $input" >&2
    exit 1
  }
done

validate_elf() {
  local path="$1" machine="$2"
  local header
  header="$("$READELF_BIN" --file-header "$path")"
  [[ "$header" == *'Class:                             ELF64'* ]] || {
    echo "expected ELF64: $path" >&2; exit 1;
  }
  [[ "$header" == *"Machine:                           $machine"* ]] || {
    echo "unexpected ELF machine: $path" >&2; exit 1;
  }
}
validate_elf "$ARM64_SO" AArch64
validate_elf "$ARM64_CORE_SO" AArch64
validate_elf "$X86_64_SO" 'Advanced Micro Devices X86-64'
validate_elf "$X86_64_CORE_SO" 'Advanced Micro Devices X86-64'
for core in "$ARM64_CORE_SO" "$X86_64_CORE_SO"; do
  "$READELF_BIN" --dyn-syms --wide "$core" | grep -E 'GLOBAL +DEFAULT +[0-9]+ +pogo_runtime_core_start_v1$' >/dev/null || {
    echo "missing core bootstrap export: $core" >&2; exit 1;
  }
done

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
cp -a "$ROOT_DIR/zygisk/module/." "$WORK_DIR/"
mkdir -p "$WORK_DIR/zygisk"
cp "$ARM64_SO" "$WORK_DIR/zygisk/arm64-v8a.so"
cp "$X86_64_SO" "$WORK_DIR/zygisk/x86_64.so"
mkdir -p "$WORK_DIR/runtime-core"
cp "$ARM64_CORE_SO" "$WORK_DIR/runtime-core/arm64-v8a.so"
cp "$X86_64_CORE_SO" "$WORK_DIR/runtime-core/x86_64.so"

for abi in arm64-v8a x86_64; do
  core="$WORK_DIR/runtime-core/$abi.so"
  core_size="$(wc -c < "$core" | tr -d ' ')"
  core_sha256="$(shasum -a 256 "$core" | awk '{print $1}')"
  {
    echo "protocol=$CORE_PROTOCOL"
    echo "abi=$abi"
    echo "size=$core_size"
    echo "sha256=$core_sha256"
  } > "$WORK_DIR/runtime-core/$abi.manifest"
done

[[ -s "$WORK_DIR/zygisk/arm64-v8a.so" ]]
[[ -s "$WORK_DIR/zygisk/x86_64.so" ]]
[[ -s "$WORK_DIR/runtime-core/arm64-v8a.so" ]]
[[ -s "$WORK_DIR/runtime-core/x86_64.so" ]]

mkdir -p "$(dirname "$OUTPUT_ZIP")"
rm -f "$OUTPUT_ZIP"
(
  cd "$WORK_DIR"
  zip -qr "$OUTPUT_ZIP" .
)

unzip -l "$OUTPUT_ZIP" | grep 'zygisk/arm64-v8a.so' >/dev/null
unzip -l "$OUTPUT_ZIP" | grep 'zygisk/x86_64.so' >/dev/null
for abi in arm64-v8a x86_64; do
  unzip -l "$OUTPUT_ZIP" | grep "runtime-core/$abi.so" >/dev/null
  unzip -l "$OUTPUT_ZIP" | grep "runtime-core/$abi.manifest" >/dev/null
  [[ "$(unzip -p "$OUTPUT_ZIP" "runtime-core/$abi.so" | shasum -a 256 | awk '{print $1}')" == \
    "$(sed -n 's/^sha256=//p' "$WORK_DIR/runtime-core/$abi.manifest")" ]]
done
unzip -p "$OUTPUT_ZIP" module.prop | grep -qx "id=$MODULE_ID"

ZIP_SHA256="$(shasum -a 256 "$OUTPUT_ZIP" | awk '{print $1}')"
SOURCE_REVISION="$(git -C "$ROOT_DIR" rev-parse HEAD)"
SOURCE_TREE_STATE=clean
if [[ -n "$(git -C "$ROOT_DIR" status --porcelain --untracked-files=no)" ]]; then
  SOURCE_TREE_STATE=dirty
fi
TEMP_MANIFEST="$(mktemp "$(dirname "$OUTPUT_MANIFEST")/.pogo-module-manifest.XXXXXX")"
trap 'rm -f "$TEMP_MANIFEST"' EXIT
{
  echo '{'
  echo '  "schemaVersion": 1,'
  echo "  \"moduleId\": \"$MODULE_ID\","
  echo "  \"moduleVersion\": \"$MODULE_VERSION\","
  echo "  \"moduleVersionCode\": $MODULE_VERSION_CODE,"
  echo '  "abis": ["arm64-v8a", "x86_64"],'
  echo '  "coreArtifacts": ['
  for abi in arm64-v8a x86_64; do
    core="$WORK_DIR/runtime-core/$abi.so"
    core_size="$(wc -c < "$core" | tr -d ' ')"
    core_sha256="$(shasum -a 256 "$core" | awk '{print $1}')"
    suffix=,
    [[ "$abi" != x86_64 ]] || suffix=
    echo "    {\"protocol\": $CORE_PROTOCOL, \"abi\": \"$abi\", \"size\": $core_size, \"sha256\": \"$core_sha256\"}$suffix"
  done
  echo '  ],'
  echo "  \"zipFile\": \"$(basename "$OUTPUT_ZIP")\","
  echo "  \"zipSha256\": \"$ZIP_SHA256\","
  echo "  \"sourceRevision\": \"$SOURCE_REVISION\","
  echo "  \"sourceTreeState\": \"$SOURCE_TREE_STATE\","
  echo '  "target": {'
  echo "    \"packageName\": \"$TARGET_PACKAGE\","
  echo "    \"versionName\": \"$TARGET_VERSION_NAME\","
  echo "    \"versionCode\": $TARGET_VERSION_CODE,"
  echo "    \"abi\": \"$TARGET_ABI\","
  echo "    \"il2cppBuildId\": \"$TARGET_IL2CPP_BUILD_ID\","
  echo "    \"globalMetadataSha256\": \"$TARGET_METADATA_SHA256\","
  echo "    \"baseApkSha256\": \"$TARGET_BASE_APK_SHA256\""
  echo '  }'
  echo '}'
} > "$TEMP_MANIFEST"
mv "$TEMP_MANIFEST" "$OUTPUT_MANIFEST"

echo "$OUTPUT_ZIP"
echo "$OUTPUT_MANIFEST"
