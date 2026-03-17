/*
 * src/crc32.c
 *
 * Standard CRC32 with the IEEE polynomial 0xEDB88320 (bit-reversed form
 * of 0x04C11DB7). Used by section checksum verification.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "vaultpack/crc32.h"

static uint32_t vp_crc32_table[256];
static int      vp_crc32_table_ready = 0;

static void vp_crc32_build_table(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) {
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        vp_crc32_table[i] = c;
    }
    vp_crc32_table_ready = 1;
}

uint32_t vp_crc32_init(void)
{
    if (!vp_crc32_table_ready) vp_crc32_build_table();
    return VP_CRC32_INIT;
}

uint32_t vp_crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    if (!vp_crc32_table_ready) vp_crc32_build_table();
    for (size_t i = 0; i < len; i++) {
        crc = vp_crc32_table[(crc ^ data[i]) & 0xffu] ^ (crc >> 8);
    }
    return crc;
}

uint32_t vp_crc32_final(uint32_t crc)
{
    return crc ^ VP_CRC32_INIT;
}

uint32_t vp_crc32_bytes(const uint8_t *data, size_t len)
{
    if (!data && len > 0) return 0;
    return vp_crc32_final(vp_crc32_update(vp_crc32_init(), data, len));
}

uint32_t vp_crc32_two(const uint8_t *a, size_t alen,
                      const uint8_t *b, size_t blen)
{
    uint32_t c = vp_crc32_init();
    c = vp_crc32_update(c, a, alen);
    c = vp_crc32_update(c, b, blen);
    return vp_crc32_final(c);
}
