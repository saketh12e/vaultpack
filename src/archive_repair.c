/*
 * src/archive_repair.c
 *
 * Repair scanner. Walks the archive blob looking for SECT headers
 * and records each one whose declared payload fits within the
 * archive boundary. Used to recover data from archives whose
 * central directory has been lost.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/repair.h"
#include "vaultpack/error.h"

void vp_repair_ctx_init(vp_repair_ctx_t *r, const uint8_t *blob, size_t len)
{
    if (!r) return;
    r->blob = blob;
    r->blob_len = len;
    r->records = NULL;
    r->count = 0;
    r->cap = 0;
}

void vp_repair_ctx_destroy(vp_repair_ctx_t *r)
{
    if (!r) return;
    free(r->records);
    r->records = NULL;
    r->count = 0;
    r->cap = 0;
}

static int vp_repair_grow(vp_repair_ctx_t *r, uint32_t need)
{
    if (need <= r->cap) return VP_OK;
    uint32_t new_cap = r->cap ? r->cap * 2u : 16u;
    while (new_cap < need) new_cap *= 2u;
    vp_repair_record_t *p = (vp_repair_record_t *)realloc(
        r->records, (size_t)new_cap * sizeof(*p));
    if (!p) return VP_ERR_NOMEM;
    r->records = p;
    r->cap = new_cap;
    return VP_OK;
}

int vp_repair_scan(vp_repair_ctx_t *r)
{
    if (!r || !r->blob) return VP_ERR_INVALID;
    size_t pos = 0;
    while (pos + 24 <= r->blob_len) {
        if (memcmp(r->blob + pos, "SECT", 4) != 0) {
            pos++;
            continue;
        }
        uint32_t raw_size = (uint32_t)r->blob[pos + 8]
                          | ((uint32_t)r->blob[pos + 9] << 8)
                          | ((uint32_t)r->blob[pos + 10] << 16)
                          | ((uint32_t)r->blob[pos + 11] << 24);
        uint32_t comp_size = (uint32_t)r->blob[pos + 12]
                           | ((uint32_t)r->blob[pos + 13] << 8)
                           | ((uint32_t)r->blob[pos + 14] << 16)
                           | ((uint32_t)r->blob[pos + 15] << 24);
        uint32_t crc = (uint32_t)r->blob[pos + 16]
                     | ((uint32_t)r->blob[pos + 17] << 8)
                     | ((uint32_t)r->blob[pos + 18] << 16)
                     | ((uint32_t)r->blob[pos + 19] << 24);
        uint32_t flags = (uint32_t)r->blob[pos + 20]
                       | ((uint32_t)r->blob[pos + 21] << 8)
                       | ((uint32_t)r->blob[pos + 22] << 16)
                       | ((uint32_t)r->blob[pos + 23] << 24);
        if (pos + 24 + comp_size > r->blob_len) {
            pos++;
            continue;
        }
        int rc = vp_repair_grow(r, r->count + 1);
        if (rc != VP_OK) return rc;
        vp_repair_record_t *rec = &r->records[r->count++];
        rec->offset = (uint64_t)pos;
        rec->raw_size = raw_size;
        rec->comp_size = comp_size;
        rec->crc = crc;
        rec->flags = flags;
        pos += 24 + comp_size;
    }
    return VP_OK;
}

int vp_archive_repair_run(const uint8_t *data, size_t size)
{
    if (!data || size < 24) return 0;
    vp_repair_ctx_t r;
    vp_repair_ctx_init(&r, data, size);
    vp_repair_scan(&r);
    vp_repair_ctx_destroy(&r);
    return 0;
}