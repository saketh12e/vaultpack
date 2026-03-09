#!/bin/bash -eu

: "${SRC:=$(cd "$(dirname "$0")/.." && pwd)}"
: "${OUT:?OUT must be set}"
: "${WORK:=/tmp/vp-work}"

mkdir -p "$WORK" "$OUT"

COMMON_CFLAGS=(
    -std=c11
    -D_GNU_SOURCE
    -I"$SRC/include"
    -Wno-error
    -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
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
    $CC $CFLAGS "${COMMON_CFLAGS[@]}" -c "$src" -o "$obj"
    OBJS+=("$obj")
done

FUZZERS=(archive_fuzzer section_fuzzer metadata_fuzzer index_fuzzer extract_fuzzer)

for fuzzer in "${FUZZERS[@]}"; do
    $CXX $CXXFLAGS "${COMMON_CFLAGS[@]}" -std=c++17 \
        "$SRC/fuzz/${fuzzer}.cc" "${OBJS[@]}" \
        $LIB_FUZZING_ENGINE -o "$OUT/$fuzzer"
done

for fuzzer in "${FUZZERS[@]}"; do
    test -x "$OUT/$fuzzer" || { echo "ERROR: $fuzzer not built"; exit 1; }
done

for fuzzer in "${FUZZERS[@]}"; do
    dir="$SRC/fuzz/corpus/$fuzzer"
    [[ -d "$dir" ]] && \
        zip -j "$OUT/${fuzzer}_seed_corpus.zip" "$dir"/* 2>/dev/null || true
done

[[ -f "$SRC/fuzz/dictionary.txt" ]] && cp "$SRC/fuzz/dictionary.txt" "$OUT/"

echo "VaultPack build complete: ${FUZZERS[*]}"
