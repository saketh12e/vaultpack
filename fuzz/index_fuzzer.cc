#include <cstdint>
#include <cstddef>
#include <cstring>

#include "vaultpack/index.h"
#include "vaultpack/archive.h"
#include "vaultpack/error.h"

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

    /* Decode dir entries after the 4-byte header.
     * Each dir entry is 52 bytes: section_offset(8) + section_size(4) + flags(2) +
     *   type(1) + reserved(1) + meta_count(4) + sub_count(2) + sub_stride(4)
     * followed by name[32] at offset 0 and meta_count[4] at offset 48.
     * The populate path reads section_offset, sub_count, sub_stride.
     */
    const uint8_t *p = data + 4;
    size_t remaining = size - 4;
    uint32_t max_entries = remaining / 52;
    if (max_entries > 1024) max_entries = 1024;

    /* Stack-allocate a fixed-size dir array (capped). */
    vp_dir_entry_t dir[64];
    uint32_t n = max_entries < 64 ? max_entries : 64;
    for (uint32_t i = 0; i < n; i++) {
        memset(&dir[i], 0, sizeof(dir[i]));
        memcpy(dir[i].name, p, 32); p += 32;
        dir[i].section_offset = 0;
        for (int k = 0; k < 8; k++) {
            dir[i].section_offset |= (uint64_t)p[k] << (k * 8);
        }
        p += 8;
        dir[i].section_size = (uint32_t)p[0]
                            | ((uint32_t)p[1] << 8)
                            | ((uint32_t)p[2] << 16)
                            | ((uint32_t)p[3] << 24);
        p += 4;
        dir[i].flags = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
        p += 2;
        dir[i].type = p[0];
        dir[i].reserved = p[1];
        p += 2;
        dir[i].meta_count = (uint32_t)p[0]
                          | ((uint32_t)p[1] << 8)
                          | ((uint32_t)p[2] << 16)
                          | ((uint32_t)p[3] << 24);
        p += 4;
        dir[i].sub_count = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
        p += 2;
        dir[i].sub_stride = (uint32_t)p[0]
                          | ((uint32_t)p[1] << 8)
                          | ((uint32_t)p[2] << 16)
                          | ((uint32_t)p[3] << 24);
        p += 4;
    }

    vp_index_populate(&idx, dir, n);
    vp_index_destroy(&idx);
    return 0;
}
