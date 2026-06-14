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

/* === Offset remap advanced operations ===
 *
 * The remap table supports translation in both directions:
 * source-to-target (push + get) and target-to-source (resolve).
 * Bulk queries allow callers to map a contiguous range at once.
 */

int vp_offset_remap_resolve(const vp_offset_remap_t *r,
                            uint64_t target, uint64_t *out_source)
{
    if (!r || !out_source) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < r->count; i++) {
        uint64_t start = r->table[i] + r->base;
        uint64_t end = start;  /* size unknown per-entry; check start match */
        if (target == start) {
            *out_source = r->table[i];
            return VP_OK;
        }
        (void)end;
    }
    return VP_ERR_NOTFOUND;
}

uint32_t vp_offset_remap_count(const vp_offset_remap_t *r)
{
    return r ? r->count : 0;
}

uint64_t vp_offset_remap_base(const vp_offset_remap_t *r)
{
    return r ? r->base : 0;
}

int vp_offset_remap_reserve(vp_offset_remap_t *r, uint32_t need)
{
    if (!r) return VP_ERR_INVALID;
    if (need <= r->cap) return VP_OK;
    uint32_t new_cap = r->cap ? r->cap : REMAP_INITIAL_CAP;
    while (new_cap < need) new_cap *= 2u;
    uint64_t *p = (uint64_t *)realloc(r->table, (size_t)new_cap * sizeof(*p));
    if (!p) return VP_ERR_NOMEM;
    r->table = p;
    r->cap = new_cap;
    return VP_OK;
}

int vp_offset_remap_clear(vp_offset_remap_t *r)
{
    if (!r) return VP_ERR_INVALID;
    r->count = 0;
    return VP_OK;
}

int vp_offset_remap_at(const vp_offset_remap_t *r, uint32_t i,
                       uint64_t *out)
{
    if (!r || !out || i >= r->count) return VP_ERR_RANGE;
    *out = r->table[i];
    return VP_OK;
}

uint64_t vp_offset_remap_sum(const vp_offset_remap_t *r)
{
    if (!r) return 0;
    uint64_t total = 0;
    for (uint32_t i = 0; i < r->count; i++) total += r->table[i];
    return total;
}

int vp_offset_remap_clone(const vp_offset_remap_t *src,
                          vp_offset_remap_t *dst)
{
    if (!src || !dst) return VP_ERR_INVALID;
    vp_offset_remap_init(dst, src->base);
    if (src->count == 0) return VP_OK;
    int rc = vp_offset_remap_reserve(dst, src->count);
    if (rc != VP_OK) return rc;
    memcpy(dst->table, src->table, src->count * sizeof(uint64_t));
    dst->count = src->count;
    return VP_OK;
}

int vp_offset_remap_extend(vp_offset_remap_t *r,
                           const uint64_t *values, uint32_t count)
{
    if (!r || (!values && count > 0)) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < count; i++) {
        int rc = vp_offset_remap_push(r, values[i]);
        if (rc != VP_OK) return rc;
    }
    return VP_OK;
}

int vp_offset_remap_shift_base(vp_offset_remap_t *r, uint64_t new_base)
{
    if (!r) return VP_ERR_INVALID;
    r->base = new_base;
    return VP_OK;
}