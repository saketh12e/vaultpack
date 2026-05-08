/*
 * fuzz/compose_fuzzer.cc
 *
 * Metadata compose harness. Treats the input as a sequence of
 * metadata blocks and runs the composer on them.
 */
#include <cstdint>
#include <cstddef>

#include "vaultpack/meta_schema.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    return vp_meta_compose_run(data, size);
}