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

/* === Directory emitter advanced operations ===
 *
 * The emitter supports in-place updates, lookups by name, and
 * selective eviction. The serialized output is produced by
 * serialize() and can be embedded directly in the central
 * directory slot of the archive.
 */

uint32_t vp_dir_emit_count(const vp_dir_emit_t *e)
{
    return e ? e->count : 0;
}

uint32_t vp_dir_emit_capacity(const vp_dir_emit_t *e)
{
    return e ? e->cap : 0;
}

int vp_dir_emit_lookup(const vp_dir_emit_t *e, const char *name,
                       vp_dir_entry_desc_t *out)
{
    if (!e || !name || !out) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < e->count; i++) {
        if (strcmp(e->entries[i].name, name) == 0) {
            *out = e->entries[i];
            return VP_OK;
        }
    }
    return VP_ERR_NOTFOUND;
}

int vp_dir_emit_at(const vp_dir_emit_t *e, uint32_t idx,
                   vp_dir_entry_desc_t *out)
{
    if (!e || !out || idx >= e->count) return VP_ERR_RANGE;
    *out = e->entries[idx];
    return VP_OK;
}

int vp_dir_emit_remove(vp_dir_emit_t *e, const char *name)
{
    if (!e || !name) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < e->count; i++) {
        if (strcmp(e->entries[i].name, name) == 0) {
            memmove(&e->entries[i], &e->entries[i + 1],
                    (e->count - i - 1) * sizeof(vp_dir_entry_desc_t));
            e->count--;
            return VP_OK;
        }
    }
    return VP_ERR_NOTFOUND;
}

int vp_dir_emit_update(vp_dir_emit_t *e, const char *name,
                       uint64_t offset, uint32_t size, uint8_t type)
{
    if (!e || !name) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < e->count; i++) {
        if (strcmp(e->entries[i].name, name) == 0) {
            e->entries[i].offset = offset;
            e->entries[i].size = size;
            e->entries[i].type = type;
            return VP_OK;
        }
    }
    return vp_dir_emit_append(e, name, offset, size, type);
}

int vp_dir_emit_clear(vp_dir_emit_t *e)
{
    if (!e) return VP_ERR_INVALID;
    e->count = 0;
    return VP_OK;
}

int vp_dir_emit_reserve(vp_dir_emit_t *e, uint32_t cap)
{
    if (!e) return VP_ERR_INVALID;
    if (cap <= e->cap) return VP_OK;
    vp_dir_entry_desc_t *p = (vp_dir_entry_desc_t *)realloc(
        e->entries, (size_t)cap * sizeof(*p));
    if (!p) return VP_ERR_NOMEM;
    e->entries = p;
    e->cap = cap;
    return VP_OK;
}

int vp_dir_emit_iter(const vp_dir_emit_t *e,
                     vp_dir_emit_iter_cb cb, void *user)
{
    if (!e || !cb) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < e->count; i++) {
        if (!cb(&e->entries[i], user)) return VP_ERR_TRUNCATED;
    }
    return VP_OK;
}

int vp_dir_emit_clone(const vp_dir_emit_t *src, vp_dir_emit_t *dst)
{
    if (!src || !dst) return VP_ERR_INVALID;
    vp_dir_emit_init(dst);
    if (src->count == 0) return VP_OK;
    int rc = vp_dir_emit_reserve(dst, src->count);
    if (rc != VP_OK) return rc;
    memcpy(dst->entries, src->entries,
           src->count * sizeof(vp_dir_entry_desc_t));
    dst->count = src->count;
    return VP_OK;
}

uint64_t vp_dir_emit_total_size(const vp_dir_emit_t *e)
{
    if (!e) return 0;
    uint64_t total = 0;
    for (uint32_t i = 0; i < e->count; i++) {
        total += e->entries[i].size;
    }
    return total;
}

int vp_dir_emit_sort_by_name(vp_dir_emit_t *e)
{
    if (!e) return VP_ERR_INVALID;
    /* Simple insertion sort. */
    for (uint32_t i = 1; i < e->count; i++) {
        vp_dir_entry_desc_t key = e->entries[i];
        uint32_t j = i;
        while (j > 0 && strcmp(e->entries[j - 1].name, key.name) > 0) {
            e->entries[j] = e->entries[j - 1];
            j--;
        }
        e->entries[j] = key;
    }
    return VP_OK;
}