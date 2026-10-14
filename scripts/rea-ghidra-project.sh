#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  REA_AGENTS_ROOT=/path/to/rea-agents \
    scripts/rea-ghidra-project.sh --project /path/to/name.gpr \
      [--program project/item] -- <rea arguments>

Runs rea-agents 6.4.0 against one already-analyzed Ghidra project. The script
creates copy-on-write snapshots of both REA and the .gpr/.rep pair, applies the
repository overlay, then opens the selected Program with:
  -process <program> -noanalysis -readOnly

Examples:
  scripts/rea-ghidra-project.sh \
    --project reverse/artifacts/pogo-0.433.0/pogo-0.433.0.gpr \
    --program libil2cpp.so -- \
    search reverse/artifacts/pogo-0.433.0/native/libil2cpp.so TryCapture \
      --kind procedures --provider ghidra --format json --full-output

  scripts/rea-ghidra-project.sh --project /path/to/name.gpr -- --mcp

Required environment:
  REA_AGENTS_ROOT  Extracted/npm rea-agents 6.4.0 package root.
  GHIDRA_INSTALL_DIR and JAVA_HOME remain REA's normal Ghidra configuration.
EOF
}

fail() { echo "FAIL: $*" >&2; exit 1; }

PROJECT_PATH=""
PROGRAM_PATH="libil2cpp.so"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --project)
      [[ $# -ge 2 ]] || fail "--project requires a .gpr path"
      PROJECT_PATH="$2"
      shift 2
      ;;
    --program)
      [[ $# -ge 2 ]] || fail "--program requires a project item path"
      PROGRAM_PATH="$2"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    --)
      shift
      break
      ;;
    *)
      fail "unknown option: $1"
      ;;
  esac
done

[[ -n "$PROJECT_PATH" ]] || fail "--project is required"
[[ $# -gt 0 ]] || fail "REA arguments are required after --"
[[ "$PROJECT_PATH" == *.gpr ]] || fail "--project must point to a .gpr file"
[[ -f "$PROJECT_PATH" ]] || fail "Ghidra project file not found: $PROJECT_PATH"
[[ -n "${REA_AGENTS_ROOT:-}" ]] || fail "set REA_AGENTS_ROOT to rea-agents 6.4.0"
[[ -f "$REA_AGENTS_ROOT/package.json" && -f "$REA_AGENTS_ROOT/scripts/rea.mjs" ]] \
  || fail "REA_AGENTS_ROOT is not an rea-agents package root"
[[ "$(basename "$REA_AGENTS_ROOT")" == rea-agents ]] \
  || fail "REA_AGENTS_ROOT must be the rea-agents directory inside node_modules"
for tool in node patch; do command -v "$tool" >/dev/null || fail "missing $tool"; done
REA_VERSION="$(node "$REA_AGENTS_ROOT/scripts/rea.mjs" --version)"
[[ "$REA_VERSION" == 6.4.0 ]] || fail "overlay requires rea-agents 6.4.0, found $REA_VERSION"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PATCH_FILE="$ROOT_DIR/reverse/rea/ghidra-existing-project/rea-agents-6.4.0.patch"
[[ -f "$PATCH_FILE" ]] || fail "missing overlay: $PATCH_FILE"

PROJECT_PATH="$(cd "$(dirname "$PROJECT_PATH")" && pwd)/$(basename "$PROJECT_PATH")"
PROJECT_DIR="$(dirname "$PROJECT_PATH")"
PROJECT_NAME="$(basename "$PROJECT_PATH" .gpr)"
PROJECT_REP="$PROJECT_DIR/$PROJECT_NAME.rep"
[[ -d "$PROJECT_REP" ]] || fail "project repository not found: $PROJECT_REP"

WORK_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/rea-ghidra-project.XXXXXX")"
cleanup() { rm -rf -- "$WORK_ROOT"; }
trap cleanup EXIT

NODE_MODULES_ROOT="$(cd "$(dirname "$REA_AGENTS_ROOT")" && pwd)"
FORK_ROOT="$WORK_ROOT/node_modules/rea-agents"
SNAPSHOT_ROOT="$WORK_ROOT/project"
mkdir "$SNAPSHOT_ROOT"
if [[ "$(uname -s)" == Darwin ]]; then
  cp -cR "$NODE_MODULES_ROOT" "$WORK_ROOT/node_modules"
  cp -c "$PROJECT_PATH" "$SNAPSHOT_ROOT/$PROJECT_NAME.gpr"
  cp -cR "$PROJECT_REP" "$SNAPSHOT_ROOT/$PROJECT_NAME.rep"
else
  cp -R "$NODE_MODULES_ROOT" "$WORK_ROOT/node_modules"
  cp "$PROJECT_PATH" "$SNAPSHOT_ROOT/$PROJECT_NAME.gpr"
  cp -R "$PROJECT_REP" "$SNAPSHOT_ROOT/$PROJECT_NAME.rep"
fi

(cd "$FORK_ROOT" && patch --batch --forward -p1 < "$PATCH_FILE")

export REA_GHIDRA_PROJECT_LOCATION="$SNAPSHOT_ROOT"
export REA_GHIDRA_PROJECT_IDENTITY_LOCATION="$PROJECT_DIR"
export REA_GHIDRA_PROJECT_NAME="$PROJECT_NAME"
export REA_GHIDRA_PROJECT_PROGRAM="$PROGRAM_PATH"
node "$FORK_ROOT/scripts/rea.mjs" "$@"
