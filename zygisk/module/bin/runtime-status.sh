#!/system/bin/sh

STATE_FILE=/data/adb/pogo_root_automation/runtime.status
GOOGLE_PACKAGE=com.nianticlabs.pokemongo
GALAXY_PACKAGE=com.nianticlabs.pokemongo.ares

protocol=1
pid=0
process_name=
native_probe_state=not_seen
native_libil2cpp_loaded=0
native_libunity_loaded=0
native_il2cpp_api_available=0
native_il2cpp_symbol_count=0
native_il2cpp_required_symbol_count=0
native_libil2cpp_path=
native_libunity_path=
native_translation_layer=
native_assembly_survey_state=unavailable
native_assembly_count=0
native_assembly_csharp_found=0
native_assembly_csharp_name=
native_class_survey_state=unavailable
native_class_count=0
native_candidate_class_count=0
native_candidate_classes=

read_state_field() {
  sed -n "s/^$1=//p" "$STATE_FILE" 2>/dev/null | head -n 1
}

if [ -f "$STATE_FILE" ]; then
  value=$(read_state_field protocol); [ -n "$value" ] && protocol="$value"
  value=$(read_state_field pid); [ -n "$value" ] && pid="$value"
  process_name=$(read_state_field process)
  value=$(read_state_field native_probe_state); [ -n "$value" ] && native_probe_state="$value"
  value=$(read_state_field native_libil2cpp_loaded); [ -n "$value" ] && native_libil2cpp_loaded="$value"
  value=$(read_state_field native_libunity_loaded); [ -n "$value" ] && native_libunity_loaded="$value"
  value=$(read_state_field native_il2cpp_api_available); [ -n "$value" ] && native_il2cpp_api_available="$value"
  value=$(read_state_field native_il2cpp_symbol_count); [ -n "$value" ] && native_il2cpp_symbol_count="$value"
  value=$(read_state_field native_il2cpp_required_symbol_count); [ -n "$value" ] && native_il2cpp_required_symbol_count="$value"
  native_libil2cpp_path=$(read_state_field native_libil2cpp_path)
  native_libunity_path=$(read_state_field native_libunity_path)
  native_translation_layer=$(read_state_field native_translation_layer)
  value=$(read_state_field native_assembly_survey_state); [ -n "$value" ] && native_assembly_survey_state="$value"
  value=$(read_state_field native_assembly_count); [ -n "$value" ] && native_assembly_count="$value"
  value=$(read_state_field native_assembly_csharp_found); [ -n "$value" ] && native_assembly_csharp_found="$value"
  native_assembly_csharp_name=$(read_state_field native_assembly_csharp_name)
  value=$(read_state_field native_class_survey_state); [ -n "$value" ] && native_class_survey_state="$value"
  value=$(read_state_field native_class_count); [ -n "$value" ] && native_class_count="$value"
  value=$(read_state_field native_candidate_class_count); [ -n "$value" ] && native_candidate_class_count="$value"
  native_candidate_classes=$(read_state_field native_candidate_classes)
fi

runtime_state=not_seen
if [ "$pid" -gt 0 ] 2>/dev/null; then
  current_process=$(tr '\000' '\n' < "/proc/$pid/cmdline" 2>/dev/null | head -n 1)
  if [ "$current_process" = "$process_name" ] && kill -0 "$pid" 2>/dev/null; then runtime_state=connected; else runtime_state=disconnected; fi
elif [ -f "$STATE_FILE" ]; then
  runtime_state=disconnected
fi

is_installed() {
  candidate_path=$(pm path "$1" 2>/dev/null | head -n 1)
  case "$candidate_path" in package:*) return 0 ;; *) return 1 ;; esac
}

package_name=
case "$process_name" in
  "$GOOGLE_PACKAGE"|"$GALAXY_PACKAGE") if is_installed "$process_name"; then package_name="$process_name"; fi ;;
esac
if [ -z "$package_name" ]; then
  for candidate in "$GOOGLE_PACKAGE" "$GALAXY_PACKAGE"; do
    if is_installed "$candidate"; then package_name="$candidate"; break; fi
  done
fi

version_name=
version_code=
if [ -n "$package_name" ]; then
  package_dump=$(dumpsys package "$package_name" 2>/dev/null)
  version_name=$(printf '%s\n' "$package_dump" | sed -n 's/^[[:space:]]*versionName=//p' | head -n 1)
  version_code=$(printf '%s\n' "$package_dump" | sed -n 's/^[[:space:]]*versionCode=\([0-9]*\).*/\1/p' | head -n 1)
fi

probe_state=not_running
binding_engine=unknown
binding_strategy=unavailable
process_exe=
libil2cpp_path=
libunity_path=
libmain_path=
translation_layer=none

if [ "$runtime_state" = connected ] && [ "$pid" -gt 0 ] 2>/dev/null; then
  probe_state=waiting
  process_exe=$(readlink "/proc/$pid/exe" 2>/dev/null)
  maps_path="/proc/$pid/maps"
  if [ -r "$maps_path" ]; then
    libil2cpp_path=$(awk '$NF ~ /\/libil2cpp\.so$/ { print $NF; exit }' "$maps_path" 2>/dev/null)
    libunity_path=$(awk '$NF ~ /\/libunity\.so$/ { print $NF; exit }' "$maps_path" 2>/dev/null)
    libmain_path=$(awk '$NF ~ /\/libmain\.so$/ { print $NF; exit }' "$maps_path" 2>/dev/null)
    if grep -q '/libhoudini[^/]*\.so' "$maps_path" 2>/dev/null; then translation_layer=houdini
    elif grep -q '/libndk_translation[^/]*\.so' "$maps_path" 2>/dev/null; then translation_layer=ndk_translation; fi
  fi
  if [ -n "$libil2cpp_path" ]; then
    binding_engine=il2cpp
    probe_state=ready
    binding_strategy=il2cpp_mapped_only
    if [ "$native_probe_state" = complete ] && [ "$native_il2cpp_api_available" = 1 ]; then binding_strategy=il2cpp_exported_api; fi
  elif [ -n "$libunity_path" ]; then
    binding_engine=unity_unknown_backend
    probe_state=unity_loaded
  fi
fi

if [ -n "$native_translation_layer" ] && [ "$native_translation_layer" != none ]; then translation_layer="$native_translation_layer"; fi

device_primary_abi=$(getprop ro.product.cpu.abi 2>/dev/null)
device_supported_abis=$(getprop ro.product.cpu.abilist 2>/dev/null)
native_bridge=$(getprop ro.dalvik.vm.native.bridge 2>/dev/null)
zygote=$(getprop ro.zygote 2>/dev/null)
kernel_machine=$(uname -m 2>/dev/null)

printf 'protocol=%s\n' "$protocol"
printf 'runtime_state=%s\n' "$runtime_state"
printf 'pid=%s\n' "$pid"
printf 'process=%s\n' "$process_name"
printf 'package=%s\n' "$package_name"
printf 'version_name=%s\n' "$version_name"
printf 'version_code=%s\n' "$version_code"
printf 'probe_state=%s\n' "$probe_state"
printf 'binding_engine=%s\n' "$binding_engine"
printf 'binding_strategy=%s\n' "$binding_strategy"
printf 'process_exe=%s\n' "$process_exe"
printf 'libil2cpp_path=%s\n' "$libil2cpp_path"
printf 'libunity_path=%s\n' "$libunity_path"
printf 'libmain_path=%s\n' "$libmain_path"
printf 'translation_layer=%s\n' "$translation_layer"
printf 'native_probe_state=%s\n' "$native_probe_state"
printf 'native_il2cpp_api_available=%s\n' "$native_il2cpp_api_available"
printf 'native_il2cpp_symbol_count=%s\n' "$native_il2cpp_symbol_count"
printf 'native_il2cpp_required_symbol_count=%s\n' "$native_il2cpp_required_symbol_count"
printf 'native_libil2cpp_path=%s\n' "$native_libil2cpp_path"
printf 'native_libunity_path=%s\n' "$native_libunity_path"
printf 'native_assembly_survey_state=%s\n' "$native_assembly_survey_state"
printf 'native_assembly_count=%s\n' "$native_assembly_count"
printf 'native_assembly_csharp_found=%s\n' "$native_assembly_csharp_found"
printf 'native_assembly_csharp_name=%s\n' "$native_assembly_csharp_name"
printf 'native_class_survey_state=%s\n' "$native_class_survey_state"
printf 'native_class_count=%s\n' "$native_class_count"
printf 'native_candidate_class_count=%s\n' "$native_candidate_class_count"
printf 'native_candidate_classes=%s\n' "$native_candidate_classes"
printf 'device_primary_abi=%s\n' "$device_primary_abi"
printf 'device_supported_abis=%s\n' "$device_supported_abis"
printf 'native_bridge=%s\n' "$native_bridge"
printf 'zygote=%s\n' "$zygote"
printf 'kernel_machine=%s\n' "$kernel_machine"
