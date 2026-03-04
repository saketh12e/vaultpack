#include <cstdint>
#include <cstddef>
#include <cstring>

#include "vaultpack/index.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8)
        return 0;
    uint16_t width, depth;
    memcpy(&width, data,     2);
    memcpy(&depth, data + 2, 2);
    if (width == 0 || depth == 0)
        return 0;

    vp_index_t idx = {};
    if (vp_build_index(&idx, width, depth) != VP_OK)
        return 0;

    uint32_t n_insert = (uint32_t)width * (uint32_t)depth;
    if (n_insert > 1024) n_insert = 1024;

    for (uint32_t i = 0; i < n_insert; i++) {
        if (8 + (size_t)i * 4 + 4 > size) break;
        uint32_t offset = 0;
        memcpy(&offset, data + 8 + i * 4, sizeof(uint32_t));
        vp_index_insert(&idx, i, offset);
    }

    vp_index_destroy(&idx);
    return 0;
}
