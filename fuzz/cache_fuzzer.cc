/*
 * fuzz/cache_fuzzer.cc
 *
 * Section cache harness. Drives the LRU slot pool through
 * put/get/pin/unpin operations and exercises the eviction
 * paths under pinning.
 */
#include <cstdint>
#include <cstddef>

#include "vaultpack/section_cache.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    return vp_section_cache_run(data, size);
}