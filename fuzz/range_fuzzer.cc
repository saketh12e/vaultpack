/*
 * fuzz/range_fuzzer.cc
 *
 * Range reader harness. Drives the bounded range I/O with a
 * series of read operations derived from the input bytes.
 */
#include <cstdint>
#include <cstddef>

#include "vaultpack/range_io.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    return vp_range_io_run(data, size);
}