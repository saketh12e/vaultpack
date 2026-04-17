/*
 * src/dir_emit.c
 *
 * Emits the central directory record at the end of an archive.
 * The directory is built from a sorted list of section descriptors
 * and offsets, then serialized into a contiguous byte buffer.
 *
 * Callers must provide a directory descriptor array sorted by
 * section name; this module does not re-sort.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/pack_ctx.h"
#include "vaultpack/error.h"

void vp_dir_emit_init(vp_dir_emit_t *e)
{
    if (!e) return;
    memset(e, 0, sizeof(*e));
}

int vp_dir_emit_append(vp_dir_emit_t *e, const char *name,
                       uint64_t offset, uint32_t size, uint8_t type)
{
    if (!e || !name) return VP_ERR_INVALID;
    if (e->count >= e->cap) {
        uint32_t new_cap = e->cap ? e->cap * 2u : 16u;
        vp_dir_entry_desc_t *p = (vp_dir_entry_desc_t *)realloc(
            e->entries, (size_t)new_cap * sizeof(*p));
        if (!p) return VP_ERR_NOMEM;
        e->entries = p;
        e->cap = new_cap;
    }
    vp_dir_entry_desc_t *d = &e->entries[e->count++];
    memset(d, 0, sizeof(*d));
    strncpy(d->name, name, sizeof(d->name) - 1);
    d->offset = offset;
    d->size   = size;
    d->type   = type;
    return VP_OK;
}

size_t vp_dir_emit_size(const vp_dir_emit_t *e)
{
    if (!e) return 0;
    return (size_t)e->count * 58u;
}

int vp_dir_emit_serialize(const vp_dir_emit_t *e,
                          uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!e || !out) return VP_ERR_INVALID;
    size_t need = vp_dir_emit_size(e);
    if (out_cap < need) return VP_ERR_FULL;
    uint8_t *p = out;
    for (uint32_t i = 0; i < e->count; i++) {
        const vp_dir_entry_desc_t *d = &e->entries[i];
        memset(p, 0, 58);
        memcpy(p, d->name, 32);
        p[32] = (uint8_t)d->offset;
        p[33] = (uint8_t)(d->offset >> 8);
        p[34] = (uint8_t)(d->offset >> 16);
        p[35] = (uint8_t)(d->offset >> 24);
        p[36] = (uint8_t)(d->offset >> 32);
        p[37] = (uint8_t)(d->offset >> 40);
        p[38] = (uint8_t)(d->offset >> 48);
        p[39] = (uint8_t)(d->offset >> 56);
        p[40] = (uint8_t)d->size;
        p[41] = (uint8_t)(d->size >> 8);
        p[42] = (uint8_t)(d->size >> 16);
        p[43] = (uint8_t)(d->size >> 24);
        p[44] = 0;
        p[45] = 0;
        p[46] = d->type;
        p[47] = 0;
        /* meta_count, sub_count, sub_stride left zero. */
        p += 58;
    }
    if (out_len) *out_len = need;
    return VP_OK;
}

void vp_dir_emit_destroy(vp_dir_emit_t *e)
{
    if (!e) return;
    free(e->entries);
    e->entries = NULL;
    e->count = 0;
    e->cap = 0;
}