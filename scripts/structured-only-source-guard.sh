#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_SRC="$ROOT_DIR/app/src/main"

test ! -e "$APP_SRC/java/dev/pogoroot/automation/headless/ScreenAutomation.kt"
test ! -e "$APP_SRC/java/dev/pogoroot/automation/root/RootBinaryShell.kt"

if rg -n -i \
  'AutomationRuntimeMode|runtimeMode|GameScreenState|GameScreenAnalyzer|RootScreenCapture|RootUiDriver|screencap|input (tap|swipe)|manualCatch|manualSpin|screenWidth|screenHeight|framesAnalyzed|encounterSweepTaps|structuredRuntime|pokemonGoForeground' \
  "$APP_SRC/java"; then
  echo "structured-only source guard failed" >&2
  exit 1
fi

if rg -n '/v1/actions/(catch|spin)' "$ROOT_DIR/app/src/main" "$ROOT_DIR/scripts"; then
  echo "direct manual action endpoint guard failed" >&2
  exit 1
fi

test -f "$APP_SRC/java/dev/pogoroot/automation/root/RootShell.kt"
echo "structured-only source guard passed"
