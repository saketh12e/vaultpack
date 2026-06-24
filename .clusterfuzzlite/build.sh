#!/bin/bash -eu
#
# ClusterFuzzLite build entry point for VaultPack.
# Builds every fuzz harness into $OUT using OSS-Fuzz toolchain variables.
#

: "${SRC:=$(cd "$(dirname "$0")/.." && pwd)}"
: "${WORK:=/tmp/vaultpack-fuzz-work}"
: "${OUT:?OUT must be set by the fuzzing environment}"

mkdir -p "${WORK}"

COMMON_CFLAGS=(
    -std=c11
    -D_GNU_SOURCE
    -I"${SRC}/include"
    -Wno-error
    -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
)

COMMON_SRC=(
    "${SRC}/src/archive.c"
    "${SRC}/src/section.c"
    "${SRC}/src/metadata.c"
    "${SRC}/src/index.c"
    "${SRC}/src/extract.c"
    "${SRC}/src/util.c"
    "${SRC}/src/error.c"
    "${SRC}/src/stream.c"
    "${SRC}/src/buffer.c"
    "${SRC}/src/crc32.c"
    "${SRC}/src/writer.c"
    "${SRC}/src/manifest.c"
    "${SRC}/src/manifest_apply.c"
    "${SRC}/src/codec.c"
    "${SRC}/src/validate.c"
    "${SRC}/src/decompress_lz4.c"
    "${SRC}/src/decompress_hc.c"
    "${SRC}/src/section_pipeline.c"
    "${SRC}/src/checksum_stream.c"
    "${SRC}/src/pack_ctx.c"
    "${SRC}/src/chunk_assembler.c"
    "${SRC}/src/dir_emit.c"
    "${SRC}/src/pack_abort.c"
    "${SRC}/src/range_io.c"
    "${SRC}/src/section_cache.c"
    "${SRC}/src/archive_view.c"
    "${SRC}/src/prefetch.c"
    "${SRC}/src/cache_pin.c"
    "${SRC}/src/interval_engine.c"
    "${SRC}/src/archive_merge.c"
    "${SRC}/src/archive_repair.c"
    "${SRC}/src/archive_diff.c"
    "${SRC}/src/offset_remap.c"
    "${SRC}/src/meta_schema.c"
    "${SRC}/src/meta_compose.c"
    "${SRC}/src/index_sort.c"
    "${SRC}/src/index_lookup.c"
    "${SRC}/src/index_compact.c"
)

OBJECTS=()
for src in "${COMMON_SRC[@]}"; do
    obj="${WORK}/$(basename "${src}" .c).o"
    ${CC} ${CFLAGS} "${COMMON_CFLAGS[@]}" -c "${src}" -o "${obj}"
    OBJECTS+=("${obj}")
done

FUZZERS=(
    archive_fuzzer
    section_fuzzer
    metadata_fuzzer
    index_fuzzer
    extract_fuzzer
    stream_fuzzer
    manifest_fuzzer
    codec_fuzzer
    writer_fuzzer
    validate_fuzzer
    pipeline_fuzzer
    pack_fuzzer
    cache_fuzzer
    merge_fuzzer
    repair_fuzzer
    schema_fuzzer
    index2_fuzzer
    range_fuzzer
    diff_fuzzer
    compose_fuzzer
)

for fuzzer in "${FUZZERS[@]}"; do
    ${CXX} ${CXXFLAGS} "${COMMON_CFLAGS[@]}" -std=c++17 \
        "${SRC}/fuzz/${fuzzer}.cc" "${OBJECTS[@]}" \
        ${LIB_FUZZING_ENGINE} -o "${OUT}/${fuzzer}"
done

if [[ -d "${SRC}/fuzz/corpus" ]]; then
    for fuzzer in "${FUZZERS[@]}"; do
        if [[ -d "${SRC}/fuzz/corpus/${fuzzer}" ]]; then
            cp -r "${SRC}/fuzz/corpus/${fuzzer}" \
                  "${OUT}/${fuzzer}_seed_corpus" 2>/dev/null || true
        fi
    done
fi

if [[ -f "${SRC}/fuzz/dictionary.txt" ]]; then
    cp "${SRC}/fuzz/dictionary.txt" "${OUT}/"
fi

for fuzzer in "${FUZZERS[@]}"; do
    test -x "${OUT}/${fuzzer}" || { echo "error: fuzz target missing: ${OUT}/${fuzzer}" >&2; exit 1; }
done

echo "Built fuzz targets in ${OUT}: ${FUZZERS[*]}"
