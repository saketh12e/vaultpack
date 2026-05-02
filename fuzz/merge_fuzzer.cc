/*
 * fuzz/merge_fuzzer.cc
 *
 * Archive merge harness. Splits the input into two halves and
 * feeds each through the merger.
 */
#include <cstdint>
#include <cstddef>

#include "vaultpack/merge.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    return vp_archive_merge_run(data, size);
}