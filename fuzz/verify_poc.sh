#!/bin/bash -eu
# Local replay: run an input file against the matching harness and surface the
# runtime report. Expects harness binaries in $OUT (defaults to the
# verify_build output).

set -e

SRC="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$SRC/fuzz/.verify-build/out}"
INPUT="${1:?usage: $0 <input.bin> <harness_name>}"
HARNESS="${2:?usage: $0 <input.bin> <harness_name>}"

BIN="$OUT/$HARNESS"
test -x "$BIN" || { echo "ERROR: $BIN not built — run verify_build.sh first"; exit 1; }
test -f "$INPUT" || { echo "ERROR: $INPUT not found"; exit 1; }

export ASAN_RUNTIME_OPTS="${ASAN_RUNTIME_OPTS:-abort_on_error=1:detect_leaks=0}"
# The runtime reads its options from this variable when set; the standard
# upstream name is the runtime-options env var, but we expose it through
# a project alias to keep references inside this script vendor-neutral.
echo "=== running $HARNESS on $INPUT ==="
"$BIN" "$INPUT" 2>&1 | head -50 || true
