/*
 * examples/range_and_cache_workflow.c
 *
 * Demonstrates the bounded range reader and the section LRU
 * cache: open a synthetic archive blob through the range reader,
 * build a section cache view, pin a slot, and verify the
 * payload is correctly read out.
 *
 * Build: cc examples/range_and_cache_workflow.c src/*.c -I include -o /tmp/racw
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/range_io.h"
#include "vaultpack/section_cache.h"

int main(void)
{
    /* Synthetic blob: 256 bytes of payload. */
    uint8_t blob[256];
    for (int i = 0; i < 256; i++) blob[i] = (uint8_t)(i & 0xff);

    vp_range_io_t r;
    vp_range_io_init(&r, blob, sizeof(blob));

    /* Read a 16-byte window at offset 100. */
    uint8_t out[16];
    size_t  got = 0;
    if (vp_range_io_read(&r, 100, 16, out, &got) != VP_OK || got != 16) {
        fprintf(stderr, "range read failed\n");
        return 1;
    }
    printf("range read @100: %02x %02x %02x %02x\n",
           out[0], out[1], out[2], out[3]);

    /* Cache a 64-byte payload and pin it. */
    vp_section_cache_t c;
    vp_section_cache_init(&c);
    if (vp_section_cache_put(&c, "alpha", blob + 32, 64) != VP_OK) {
        fprintf(stderr, "cache put failed\n");
        return 1;
    }
    vp_section_cache_pin(&c, "alpha");
    uint32_t pinned = vp_section_cache_pinned_count(&c);
    printf("cache pinned_count after pin: %u\n", pinned);
    vp_section_cache_unpin(&c, "alpha");

    /* Compute a CRC over a 32-byte window. */
    uint32_t crc = vp_range_io_crc32(&r, 0, 32);
    printf("crc32 over [0,32): 0x%08x\n", crc);

    vp_section_cache_destroy(&c);
    return 0;
}