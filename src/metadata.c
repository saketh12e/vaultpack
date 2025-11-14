#include <stdlib.h>
#include <string.h>

#include "vaultpack/metadata.h"
#include "vaultpack/error.h"

#define VP_META_DISPATCH_SIZE 64

typedef void (*vp_meta_handler_fn)(vp_meta_block_t *block, void *ctx);

static void vp_meta_handle_emit(vp_meta_block_t *block, void *ctx)
{
    (void)block;
    (void)ctx;
}

static void vp_meta_handle_store(vp_meta_block_t *block, void *ctx)
{
    (void)block;
    (void)ctx;
}

static void vp_meta_handle_drop(vp_meta_block_t *block, void *ctx)
{
    (void)block;
    (void)ctx;
}

static const vp_meta_handler_fn vp_meta_dispatch[VP_META_DISPATCH_SIZE] = {
    NULL,
    vp_meta_handle_emit,
    vp_meta_handle_store,
    vp_meta_handle_drop,
};

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

const vp_meta_entry_t *vp_metadata_lookup(const vp_meta_block_t *block,
                                          const char *key)
{
    if (!block || !key || block->key_count == 0)
        return NULL;

    int lo = 0;
    int hi = (int)block->key_count - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int cmp = strcmp(key, block->entries[mid].key);
        if (cmp == 0)
            return &block->entries[mid];
        if (cmp < 0)
            hi = mid - 1;
        else
            lo = mid + 1;
    }
    return NULL;
}

void vp_process_meta_block(vp_meta_block_t *block, void *ctx)
{
    if (!block)
        return;

    uint8_t idx = (block->tag[0] & 0x3Fu) + (block->tag[1] & 0x3Fu);
    vp_meta_handler_fn fn = vp_meta_dispatch[idx];
    if (fn != NULL)
        fn(block, ctx);
}

void vp_free_metadata_block(vp_meta_block_t *block)
{
    if (!block)
        return;
    free(block->entries);
    free(block);
}
