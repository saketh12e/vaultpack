#include <cstdint>
#include <cstddef>

#include "vaultpack/codec.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0) return 0;
    /* Decode into a generous output buffer so the in-bounds path runs.
     * Oversized repeat values should be rejected without a crash. */
    static uint8_t out[16 * 1024 * 1024];
    size_t out_len = 0;
    vp_codec_decode(data, size, out, sizeof(out), &out_len);
    return 0;
}
