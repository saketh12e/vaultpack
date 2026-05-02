/*
 * fuzz/diff_fuzzer.cc
 *
 * Archive diff harness. Splits input into two halves and runs the
 * diff engine on each.
 */
#include <cstdint>
#include <cstddef>

#include "vaultpack/merge.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    return vp_archive_diff_run(data, size);
}