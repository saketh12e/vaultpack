/*
 * fuzz/repair_fuzzer.cc
 *
 * Archive repair harness. Walks the input blob looking for valid
 * SECT headers and records each one.
 */
#include <cstdint>
#include <cstddef>

#include "vaultpack/repair.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    return vp_archive_repair_run(data, size);
}