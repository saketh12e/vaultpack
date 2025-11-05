#include <stdlib.h>
#include <string.h>

#include "vaultpack/metadata.h"
#include "vaultpack/error.h"

static int vp_meta_entry_cmp(const void *a, const void *b)
{
    const vp_meta_entry_t *ea = (const vp_meta_entry_t *)a;
    const vp_meta_entry_t *eb = (const vp_meta_entry_t *)b;
    return strcmp(ea->key, eb->key);
}

int vp_read_metadata_block(const uint8_t *data, size_t size,
                           vp_meta_block_t **out)
{
    if (!out || !data || size < sizeof(vp_meta_block_hdr_t))
        return VP_ERR_TRUNCATED;

    vp_meta_block_t *block = calloc(1, sizeof(*block));
    if (!block)
        return VP_ERR_NOMEM;

    memcpy(block->tag, data, 4);
    block->key_count = (uint16_t)data[4] | ((uint16_t)data[5] << 8);
    block->reserved  = (uint16_t)data[6] | ((uint16_t)data[7] << 8);

    size_t need = sizeof(vp_meta_block_hdr_t)
                + (size_t)block->key_count * sizeof(vp_meta_entry_t);
    if (need > size) {
        free(block);
        return VP_ERR_TRUNCATED;
    }

    if (block->key_count > 0) {
        block->entries = calloc(block->key_count, sizeof(vp_meta_entry_t));
        if (!block->entries) {
            free(block);
            return VP_ERR_NOMEM;
        }
        memcpy(block->entries, data + sizeof(vp_meta_block_hdr_t),
               (size_t)block->key_count * sizeof(vp_meta_entry_t));
        qsort(block->entries, block->key_count,
              sizeof(vp_meta_entry_t), vp_meta_entry_cmp);
    }

    *out = block;
    return VP_OK;
}
