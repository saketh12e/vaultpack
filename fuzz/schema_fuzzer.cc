/*
 * fuzz/schema_fuzzer.cc
 *
 * Typed metadata schema harness. Drives the schema validator with
 * a series of (tag, value) pairs derived from the input bytes.
 */
#include <cstdint>
#include <cstddef>

#include "vaultpack/meta_schema.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    return vp_meta_schema_run(data, size);
}