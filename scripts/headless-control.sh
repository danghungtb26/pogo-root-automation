#!/usr/bin/env bash
set -euo pipefail

ADB="${ADB:-adb}"
PORT="${PORT:-8765}"
BASE_URL="http://127.0.0.1:${PORT}"

usage() {
  cat <<'EOF'
Usage:
  ./scripts/headless-control.sh status
  ./scripts/headless-control.sh start
  ./scripts/headless-control.sh stop
  ./scripts/headless-control.sh catch
  ./scripts/headless-control.sh spin
  ./scripts/headless-control.sh config 'autoCatch=true&autoSpin=true&encounterSweep=true&loopIntervalMs=900'

Environment:
  ADB=/path/to/adb
  PORT=8765
EOF
}

forward_port() {
  "$ADB" forward "tcp:${PORT}" "tcp:${PORT}" >/dev/null
}

request() {
  local method="$1"
  local path="$2"
  curl -fsS -X "$method" "${BASE_URL}${path}"
  printf '\n'
}

command="${1:-}"
case "$command" in
  status)
    forward_port
    request GET /v1/status
    ;;
  start)
    forward_port
    request POST '/v1/start?catch=true&spin=true&encounterSweep=true'
    ;;
  stop)
    forward_port
    request POST /v1/stop
    ;;
  catch)
    forward_port
    request POST /v1/actions/catch
    ;;
  spin)
    forward_port
    request POST /v1/actions/spin
    ;;
  config)
    forward_port
    query="${2:-}"
    request POST "/v1/config?${query}"
    ;;
  *)
    usage
    exit 2
    ;;
esac
