/*
 * src/offset_remap.c
 *
 * Section offset remap table used by the merger to translate old
 * section offsets into new ones. The remap table is indexed by the
 * old offset; the table is sized to cover the largest source
 * archive.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/merge.h"
#include "vaultpack/error.h"

#define REMAP_INITIAL_CAP 64

typedef struct vp_offset_remap {
    uint64_t *table;
    uint32_t  count;
    uint32_t  cap;
    uint64_t  base;
} vp_offset_remap_t;

void vp_offset_remap_init(vp_offset_remap_t *r, uint64_t base)
{
    if (!r) return;
    r->table = NULL;
    r->count = 0;
    r->cap = 0;
    r->base = base;
}

void vp_offset_remap_destroy(vp_offset_remap_t *r)
{
    if (!r) return;
    free(r->table);
    r->table = NULL;
    r->count = 0;
    r->cap = 0;
}

int vp_offset_remap_push(vp_offset_remap_t *r, uint64_t offset)
{
    if (!r) return VP_ERR_INVALID;
    if (r->count >= r->cap) {
        uint32_t new_cap = r->cap ? r->cap * 2u : REMAP_INITIAL_CAP;
        uint64_t *p = (uint64_t *)realloc(r->table,
                                          (size_t)new_cap * sizeof(*p));
        if (!p) return VP_ERR_NOMEM;
        r->table = p;
        r->cap = new_cap;
    }
    r->table[r->count++] = offset - r->base;
    return VP_OK;
}

uint64_t vp_offset_remap_get(const vp_offset_remap_t *r, uint32_t i)
{
    if (!r || i >= r->count) return 0;
    return r->table[i] + r->base;
}