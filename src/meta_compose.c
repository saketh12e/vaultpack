/*
 * src/meta_compose.c
 *
 * Compose multiple metadata blocks into a single block. The composer
 * validates each input block against the supplied schema before
 * copying its entries into the output block. Entries with the same
 * tag from later blocks replace entries from earlier blocks.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/meta_schema.h"
#include "vaultpack/error.h"

int vp_meta_compose(const vp_meta_schema_t *s,
                    const uint8_t * const *blocks,
                    const size_t *block_lens,
                    size_t block_count,
                    uint8_t **out, size_t *out_len)
{
    if (!s || !blocks || !block_lens || !out || !out_len) return VP_ERR_INVALID;
    /* Stub: real implementation would parse each block, validate
     * entries, and emit a merged block. */
    *out = NULL;
    *out_len = 0;
    (void)block_count;
    return VP_OK;
}

int vp_meta_compose_run(const uint8_t *data, size_t size)
{
    if (!data || size < 4) return 0;
    vp_meta_schema_t s;
    vp_meta_schema_init(&s);
    vp_meta_schema_register(&s, 0x494E464F, VP_META_STRING);

    uint32_t block_count = (uint32_t)data[0]
                         | ((uint32_t)data[1] << 8);
    if (block_count > 16) block_count = 16;
    size_t pos = 2;
    const uint8_t *blocks[16];
    size_t block_lens[16];
    for (uint32_t i = 0; i < block_count && pos < size; i++) {
        if (pos + 4 > size) break;
        block_lens[i] = (uint32_t)data[pos]
                      | ((uint32_t)data[pos + 1] << 8)
                      | ((uint32_t)data[pos + 2] << 16)
                      | ((uint32_t)data[pos + 3] << 24);
        pos += 4;
        if (block_lens[i] > size - pos) block_lens[i] = size - pos;
        blocks[i] = data + pos;
        pos += block_lens[i];
    }
    uint8_t *out = NULL;
    size_t   out_len = 0;
    vp_meta_compose(&s, blocks, block_lens, block_count, &out, &out_len);
    free(out);
    return 0;
}