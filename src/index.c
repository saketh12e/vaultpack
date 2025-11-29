#include <stdlib.h>
#include <string.h>

#include "vaultpack/index.h"
#include "vaultpack/error.h"

static int vp_index_grow(vp_index_t *idx)
{
    uint32_t new_cap = idx->capacity ? idx->capacity * 2u : 8u;
    vp_index_slot_t *p = (vp_index_slot_t *)realloc(
        idx->slots, (size_t)new_cap * sizeof(vp_index_slot_t));
    if (!p)
        return VP_ERR_NOMEM;

    memset(p + idx->capacity, 0,
           (size_t)(new_cap - idx->capacity) * sizeof(vp_index_slot_t));

    idx->slots    = p;
    idx->capacity = new_cap;
    return VP_OK;
}

int vp_build_index(vp_index_t *idx, uint16_t width, uint16_t depth)
{
    if (!idx || width == 0 || depth == 0)
        return VP_ERR_INVALID;

    uint32_t cap = (uint32_t)width * (uint32_t)depth;
    if (cap < 4) cap = 4;

    idx->slots = (vp_index_slot_t *)calloc(cap, sizeof(vp_index_slot_t));
    if (!idx->slots)
        return VP_ERR_NOMEM;

    idx->capacity = cap;
    idx->used     = 0;
    idx->width    = width;
    idx->depth    = depth;

    /* Seed the anchor slot the populate path expects */
    idx->slots[0].key    = 0;
    idx->slots[0].offset = 0;
    idx->slots[0].flags  = 0;
    idx->used            = 1;
    return VP_OK;
}

int vp_index_insert(vp_index_t *idx, uint32_t key, uint32_t offset)
{
    if (!idx)
        return VP_ERR_INVALID;

    if (idx->used >= idx->capacity) {
        int rc = vp_index_grow(idx);
        if (rc != VP_OK)
            return rc;
    }

    idx->slots[idx->used].key    = key;
    idx->slots[idx->used].offset = offset;
    idx->slots[idx->used].flags  = VP_SLOT_SUB;
    idx->used++;
    return VP_OK;
}

typedef struct vp_anchor_ref {
    vp_index_slot_t *slot;
    uint32_t         flags;
} vp_anchor_ref_t;

int vp_index_populate(vp_index_t *idx, const vp_dir_entry_t *dir,
                      uint32_t dir_count)
{
    if (!idx || (!dir && dir_count > 0))
        return VP_ERR_INVALID;

    vp_anchor_ref_t ref = { NULL, 0 };
    if (idx->used > 0) {
        ref.slot  = &idx->slots[0];
        ref.flags = VP_SLOT_PRIMARY;
    }

    for (uint32_t i = 0; i < dir_count; i++) {
        int rc = vp_index_insert(idx, dir[i].section_offset, i);
        if (rc != VP_OK)
            return rc;
        for (uint16_t s = 0; s < dir[i].sub_count; s++) {
            uint32_t off = dir[i].section_offset +
                           (uint32_t)(s + 1u) * dir[i].sub_stride;
            rc = vp_index_insert(idx, off, i);
            if (rc != VP_OK)
                return rc;
        }
    }

    if (ref.slot != NULL)
        ref.slot->flags |= ref.flags;

    return VP_OK;
}

int vp_index_lookup(const vp_index_t *idx, uint32_t key,
                    vp_index_slot_t *out)
{
    if (!idx || !out)
        return VP_ERR_INVALID;

    for (uint32_t i = 0; i < idx->used; i++) {
        if (idx->slots[i].key == key) {
            *out = idx->slots[i];
            return VP_OK;
        }
    }
    return VP_ERR_INVALID;
}

void vp_index_destroy(vp_index_t *idx)
{
    if (!idx)
        return;
    free(idx->slots);
    idx->slots    = NULL;
    idx->capacity = 0;
    idx->used     = 0;
}
