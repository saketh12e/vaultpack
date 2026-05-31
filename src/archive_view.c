/*
 * src/archive_view.c
 *
 * Open an archive through a bounded range reader. The view owns
 * the directory table but borrows the section payloads from the
 * range reader, never copying them.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/range_io.h"
#include "vaultpack/error.h"

void vp_archive_view_init(vp_archive_view_t *v)
{
    if (!v) return;
    memset(v, 0, sizeof(*v));
}

void vp_archive_view_destroy(vp_archive_view_t *v)
{
    if (!v) return;
    free(v->entries);
    memset(v, 0, sizeof(*v));
}

int vp_archive_view_open(vp_archive_view_t *v, const vp_range_io_t *r)
{
    if (!v || !r) return VP_ERR_INVALID;
    if (r->size < 32) return VP_ERR_TRUNCATED;
    /* Read the magic and entry count. */
    if (memcmp(r->base, "VPAK", 4) != 0) return VP_ERR_INVALID;
    uint32_t entry_count = (uint32_t)r->base[8]
                         | ((uint32_t)r->base[9] << 8)
                         | ((uint32_t)r->base[10] << 16)
                         | ((uint32_t)r->base[11] << 24);
    uint64_t dir_offset = (uint64_t)r->base[12]
                        | ((uint64_t)r->base[13] << 8)
                        | ((uint64_t)r->base[14] << 16)
                        | ((uint64_t)r->base[15] << 24)
                        | ((uint64_t)r->base[16] << 32)
                        | ((uint64_t)r->base[17] << 40)
                        | ((uint64_t)r->base[18] << 48)
                        | ((uint64_t)r->base[19] << 56);
    v->entry_count = entry_count;
    v->dir_offset  = dir_offset;
    v->entries = (vp_view_entry_t *)calloc(entry_count, sizeof(vp_view_entry_t));
    if (entry_count > 0 && !v->entries) return VP_ERR_NOMEM;
    for (uint32_t i = 0; i < entry_count; i++) {
        size_t off = (size_t)dir_offset + (size_t)i * 58u;
        if (off + 58 > r->size) return VP_ERR_TRUNCATED;
        const uint8_t *p = r->base + off;
        vp_view_entry_t *e = &v->entries[i];
        memcpy(e->name, p, 32);
        e->offset = (uint64_t)p[32]
                  | ((uint64_t)p[33] << 8)
                  | ((uint64_t)p[34] << 16)
                  | ((uint64_t)p[35] << 24)
                  | ((uint64_t)p[36] << 32)
                  | ((uint64_t)p[37] << 40)
                  | ((uint64_t)p[38] << 48)
                  | ((uint64_t)p[39] << 56);
        e->size = (uint32_t)p[40]
                | ((uint32_t)p[41] << 8)
                | ((uint32_t)p[42] << 16)
                | ((uint32_t)p[43] << 24);
        e->type = p[46];
    }
    return VP_OK;
}

/* === Archive view additional operations ===
 *
 * Look up entries by name, iterate, and read a section's payload
 * out of the underlying range.
 */

int vp_archive_view_lookup(const vp_archive_view_t *v, const char *name,
                           vp_view_entry_t *out)
{
    if (!v || !name || !out) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < v->entry_count; i++) {
        if (strcmp(v->entries[i].name, name) == 0) {
            *out = v->entries[i];
            return VP_OK;
        }
    }
    return VP_ERR_NOTFOUND;
}

int vp_archive_view_at(const vp_archive_view_t *v, uint32_t idx,
                       vp_view_entry_t *out)
{
    if (!v || !out || idx >= v->entry_count) return VP_ERR_RANGE;
    *out = v->entries[idx];
    return VP_OK;
}

uint32_t vp_archive_view_count(const vp_archive_view_t *v)
{
    return v ? v->entry_count : 0;
}

int vp_archive_view_iter(const vp_archive_view_t *v,
                         vp_archive_view_iter_cb cb, void *user)
{
    if (!v || !cb) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < v->entry_count; i++) {
        if (!cb(&v->entries[i], user)) return VP_ERR_TRUNCATED;
    }
    return VP_OK;
}

int vp_archive_view_read_payload(const vp_archive_view_t *v,
                                 const vp_range_io_t *r,
                                 uint32_t idx,
                                 uint8_t *out, size_t out_cap,
                                 size_t *out_len)
{
    if (!v || !r || !out || !out_len) return VP_ERR_INVALID;
    if (idx >= v->entry_count) return VP_ERR_RANGE;
    const vp_view_entry_t *e = &v->entries[idx];
    if (e->size > out_cap) return VP_ERR_FULL;
    if (e->offset + e->size > r->size) return VP_ERR_RANGE;
    memcpy(out, r->base + e->offset, e->size);
    *out_len = e->size;
    return VP_OK;
}

uint64_t vp_archive_view_total_size(const vp_archive_view_t *v)
{
    if (!v) return 0;
    uint64_t total = 0;
    for (uint32_t i = 0; i < v->entry_count; i++) {
        total += v->entries[i].size;
    }
    return total;
}