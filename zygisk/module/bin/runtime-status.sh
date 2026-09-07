#!/system/bin/sh

STATE_FILE=/data/adb/pogo_root_automation/runtime.status
GOOGLE_PACKAGE=com.nianticlabs.pokemongo
GALAXY_PACKAGE=com.nianticlabs.pokemongo.ares

protocol=1
pid=0
process_name=

if [ -f "$STATE_FILE" ]; then
  status_protocol=$(sed -n 's/^protocol=//p' "$STATE_FILE" | head -n 1)
  status_pid=$(sed -n 's/^pid=//p' "$STATE_FILE" | head -n 1)
  status_process=$(sed -n 's/^process=//p' "$STATE_FILE" | head -n 1)

  [ -n "$status_protocol" ] && protocol="$status_protocol"
  [ -n "$status_pid" ] && pid="$status_pid"
  process_name="$status_process"
fi

runtime_state=not_seen
if [ "$pid" -gt 0 ] 2>/dev/null; then
  current_process=$(tr '\000' '\n' < "/proc/$pid/cmdline" 2>/dev/null | head -n 1)
  if [ "$current_process" = "$process_name" ] && kill -0 "$pid" 2>/dev/null; then
    runtime_state=connected
  else
    runtime_state=disconnected
  fi
elif [ -f "$STATE_FILE" ]; then
  runtime_state=disconnected
fi

is_installed() {
  candidate_path=$(pm path "$1" 2>/dev/null | head -n 1)
  case "$candidate_path" in
    package:*) return 0 ;;
    *) return 1 ;;
  esac
}

package_name=
# When a runtime process has been observed, prefer its package so dual-install
# devices report the version of the process we actually attached to.
case "$process_name" in
  "$GOOGLE_PACKAGE"|"$GALAXY_PACKAGE")
    if is_installed "$process_name"; then
      package_name="$process_name"
    fi
    ;;
esac

if [ -z "$package_name" ]; then
  for candidate in "$GOOGLE_PACKAGE" "$GALAXY_PACKAGE"; do
    if is_installed "$candidate"; then
      package_name="$candidate"
      break
    fi
  done
fi

version_name=
version_code=
if [ -n "$package_name" ]; then
  package_dump=$(dumpsys package "$package_name" 2>/dev/null)
  version_name=$(printf '%s\n' "$package_dump" | sed -n 's/^[[:space:]]*versionName=//p' | head -n 1)
  version_code=$(printf '%s\n' "$package_dump" | sed -n 's/^[[:space:]]*versionCode=\([0-9]*\).*/\1/p' | head -n 1)
fi

printf 'protocol=%s\n' "$protocol"
printf 'runtime_state=%s\n' "$runtime_state"
printf 'pid=%s\n' "$pid"
printf 'process=%s\n' "$process_name"
printf 'package=%s\n' "$package_name"
printf 'version_name=%s\n' "$version_name"
printf 'version_code=%s\n' "$version_code"
