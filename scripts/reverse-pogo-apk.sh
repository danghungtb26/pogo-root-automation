#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
tool="$repo_root/tools/il2cppdumper/Il2CppDumper.dll"
base_apk="${POGO_BASE_APK:-$repo_root/pogo-apkm/base.apk}"
native_apk="${POGO_NATIVE_APK:-$repo_root/pogo-apkm/split_config.arm64_v8a.apk}"
output_dir="${POGO_REVERSE_OUTPUT:-$repo_root/reverse/pogo-0.427.0}"
dotnet_cli_home="${POGO_DOTNET_CLI_HOME:-/private/tmp/dotnet-cli-pogo}"

fail() { echo "FAIL: $*" >&2; exit 1; }
require_file() { [[ -f "$1" ]] || fail "missing file: $1"; }

require_file "$tool"
require_file "$base_apk"
require_file "$native_apk"
command -v dotnet >/dev/null 2>&1 || fail "dotnet is not installed"
command -v unzip >/dev/null 2>&1 || fail "unzip is not installed"

work_dir="$(mktemp -d /private/tmp/pogo-reverse-run.XXXXXX)"
trap 'rm -rf "$work_dir"' EXIT
mkdir -p "$output_dir/classes"

unzip -p "$base_apk" \
  assets/bin/Data/Managed/Metadata/global-metadata.dat \
  > "$work_dir/global-metadata.dat"
unzip -p "$native_apk" lib/arm64-v8a/libil2cpp.so \
  > "$work_dir/libil2cpp.so"
[[ -s "$work_dir/global-metadata.dat" ]] || fail "metadata extraction failed"
[[ -s "$work_dir/libil2cpp.so" ]] || fail "libil2cpp extraction failed"

DOTNET_ROOT="${DOTNET_ROOT:-/opt/homebrew/opt/dotnet/libexec}" \
DOTNET_CLI_HOME="$dotnet_cli_home" \
DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1 \
DOTNET_ROLL_FORWARD=Major \
dotnet "$tool" "$work_dir/libil2cpp.so" "$work_dir/global-metadata.dat" "$output_dir"

dump_file="$output_dir/dump.cs"
require_file "$dump_file"

extract_class() {
  local pattern="$1"
  local destination="$2"
  awk -v pattern="$pattern" '
    !found && index($0, pattern) { found = 1 }
    found && /^\/\/ Namespace:/ { exit }
    found { print }
  ' "$dump_file" > "$output_dir/classes/$destination"
  [[ -s "$output_dir/classes/$destination" ]] || fail "class extraction failed: $pattern"
}

extract_class 'public class MapEntityCell :' MapEntityCell.cs
extract_class 'public abstract class MapPokemon :' MapPokemon.cs
extract_class 'public class WildMapPokemon :' WildMapPokemon.cs
extract_class 'public class TapGesture :' TapGesture.cs
extract_class 'public interface IDynamicTappable //' IDynamicTappable.cs
extract_class 'public interface IDynamicTappablesService :' IDynamicTappablesService.cs
extract_class 'public class DynamicTappablesService :' DynamicTappablesService.cs

for generated in dump.cs il2cpp.h script.json stringliteral.json; do
  [[ -f "$output_dir/$generated" ]] || fail "missing generated file: $generated"
  gzip -9 -f "$output_dir/$generated"
done

echo "Reverse artifacts written to $output_dir"
echo "Readable classes: $output_dir/classes"
