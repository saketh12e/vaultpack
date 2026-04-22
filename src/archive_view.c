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