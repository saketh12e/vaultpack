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
    "$SRC/src/stream.c"
    "$SRC/src/buffer.c"
    "$SRC/src/crc32.c"
    "$SRC/src/writer.c"
    "$SRC/src/manifest.c"
    "$SRC/src/manifest_apply.c"
    "$SRC/src/codec.c"
    "$SRC/src/validate.c"
    "$SRC/src/decompress_lz4.c"
    "$SRC/src/decompress_hc.c"
    "$SRC/src/section_pipeline.c"
    "$SRC/src/checksum_stream.c"
    "$SRC/src/pack_ctx.c"
    "$SRC/src/chunk_assembler.c"
    "$SRC/src/dir_emit.c"
    "$SRC/src/pack_abort.c"
    "$SRC/src/range_io.c"
    "$SRC/src/section_cache.c"
    "$SRC/src/archive_view.c"
    "$SRC/src/prefetch.c"
    "$SRC/src/cache_pin.c"
    "$SRC/src/interval_engine.c"
    "$SRC/src/archive_merge.c"
    "$SRC/src/archive_repair.c"
    "$SRC/src/archive_diff.c"
    "$SRC/src/offset_remap.c"
    "$SRC/src/meta_schema.c"
    "$SRC/src/meta_compose.c"
    "$SRC/src/index_sort.c"
    "$SRC/src/index_lookup.c"
    "$SRC/src/index_compact.c"
)

OBJS=()
for src in "${SRCS[@]}"; do
    obj="$WORK/$(basename "$src" .c).o"
    $CC ${CFLAGS:-} "${COMMON[@]}" -c "$src" -o "$obj"
    OBJS+=("$obj")
done

FUZZERS=(archive_fuzzer section_fuzzer metadata_fuzzer index_fuzzer extract_fuzzer \
         stream_fuzzer manifest_fuzzer codec_fuzzer writer_fuzzer validate_fuzzer \
         pipeline_fuzzer pack_fuzzer cache_fuzzer merge_fuzzer repair_fuzzer \
         schema_fuzzer index2_fuzzer range_fuzzer diff_fuzzer compose_fuzzer)

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
