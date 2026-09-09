#!/usr/bin/env bash
set -euo pipefail

ADB="${ADB:-adb}"
ANDROID_SERIAL="${ANDROID_SERIAL:-127.0.0.1:5565}"
PORT="${PORT:-8765}"
BASE_URL="http://127.0.0.1:${PORT}"
CONTROLLER_COMPONENT="dev.pogoroot.automation/.MainActivity"
POGO_PACKAGE="${POGO_PACKAGE:-com.nianticlabs.pokemongo}"

adb_call() {
  if [[ -n "$ANDROID_SERIAL" ]]; then
    "$ADB" -s "$ANDROID_SERIAL" "$@"
  else
    "$ADB" "$@"
  fi
}

usage() {
  cat <<'EOF'
Usage:
  bash scripts/headless-control.sh bootstrap
  bash scripts/headless-control.sh status
  bash scripts/headless-control.sh start
  bash scripts/headless-control.sh stop
  bash scripts/headless-control.sh diagnostic
  bash scripts/headless-control.sh config 'autoEncounter=false&autoCatch=true&autoSpin=true&spinSettleDelayMs=1000&catchSettleDelayMs=3500&loopIntervalMs=900'
  bash scripts/headless-control.sh game

Environment:
  ADB=/path/to/adb
  ANDROID_SERIAL=127.0.0.1:5565 (BlueStacks Air 1 default)
  PORT=8765
  POGO_PACKAGE=com.nianticlabs.pokemongo
EOF
}

forward_port() {
  adb_call forward "tcp:${PORT}" "tcp:${PORT}" >/dev/null
}

api_ready() {
  curl -fsS --max-time 1 "${BASE_URL}/v1/health" >/dev/null 2>&1
}

bootstrap() {
  adb_call shell am start -n "$CONTROLLER_COMPONENT" >/dev/null
  forward_port
  for _ in 1 2 3 4 5; do
    if api_ready; then
      return 0
    fi
    sleep 1
  done
  echo "Headless API did not become ready after starting the controller." >&2
  return 1
}

ensure_api() {
  forward_port
  if ! api_ready; then
    bootstrap
  fi
}

request() {
  local method="$1"
  local path="$2"
  curl -fsS -X "$method" "${BASE_URL}${path}"
  printf '\n'
}

launch_game() {
  adb_call shell monkey -p "$POGO_PACKAGE" -c android.intent.category.LAUNCHER 1 >/dev/null
}

command="${1:-}"
case "$command" in
  bootstrap)
    bootstrap
    request GET /v1/status
    ;;
  status)
    ensure_api
    request GET /v1/status
    ;;
  start)
    ensure_api
    request POST '/v1/start?autoEncounter=false&catch=true&spin=true'
    ;;
  stop)
    ensure_api
    request POST /v1/stop
    ;;
  diagnostic)
    ensure_api
    request POST /v1/runtime/diagnostic
    ;;
  config)
    ensure_api
    query="${2:-}"
    request POST "/v1/config?${query}"
    ;;
  game)
    launch_game
    ;;
  *)
    usage
    exit 2
    ;;
esac
