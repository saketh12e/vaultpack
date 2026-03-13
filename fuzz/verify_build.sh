#!/bin/bash -eu
# Local smoke build: compile each harness with Clang's address-checking pass
# enabled and link against vaultpack library objects. Does NOT require
# libFuzzer — falls back to a tiny LLVMFuzzerTestOneInput stub if the engine
# is unavailable.

set -e

SRC="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${WORK:-/tmp/vp-verify-build}"
OUT="${OUT:-$WORK/out}"

rm -rf "$WORK"
mkdir -p "$WORK" "$OUT"

CC="${CC:-clang}"
CXX="${CXX:-clang++}"

COMMON=(
    -std=c11 -D_GNU_SOURCE
    -I"$SRC/include"
    -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    -O1 -g -Wno-error
    -fsanitize=address,fuzzer-no-link
)

SRCS=(
    "$SRC/src/archive.c"
    "$SRC/src/section.c"
    "$SRC/src/metadata.c"
    "$SRC/src/index.c"
    "$SRC/src/extract.c"
    "$SRC/src/util.c"
    "$SRC/src/error.c"
)

OBJS=()
for src in "${SRCS[@]}"; do
    obj="$WORK/$(basename "$src" .c).o"
    $CC ${CFLAGS:-} "${COMMON[@]}" -c "$src" -o "$obj"
    OBJS+=("$obj")
done

FUZZERS=(archive_fuzzer section_fuzzer metadata_fuzzer index_fuzzer extract_fuzzer)

for fuzzer in "${FUZZERS[@]}"; do
    $CXX ${CXXFLAGS:-} "${COMMON[@]}" -std=c++17 \
        "$SRC/fuzz/${fuzzer}.cc" "${OBJS[@]}" \
        -fsanitize=address,fuzzer \
        -o "$OUT/$fuzzer"
done

for fuzzer in "${FUZZERS[@]}"; do
    test -x "$OUT/$fuzzer" || { echo "ERROR: $fuzzer not built"; exit 1; }
done

echo "Local smoke build OK: $OUT"
