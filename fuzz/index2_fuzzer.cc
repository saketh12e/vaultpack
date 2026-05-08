/*
 * fuzz/index2_fuzzer.cc
 *
 * v2 index harness. Inserts slots and then runs compaction.
 */
#include <cstdint>
#include <cstddef>

#include "vaultpack/index2.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    return vp_index2_run(data, size);
}