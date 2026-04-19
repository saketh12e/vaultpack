/*
 * fuzz/pack_fuzzer.cc
 *
 * Streaming pack context harness.
 */
#include <cstdint>
#include <cstddef>

#include "vaultpack/pack_ctx.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    return vp_pack_ctx_run(data, size);
}