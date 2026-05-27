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
#include <stdio.h>

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

/* === Repair advanced operations ===
 *
 * Additional helpers used by archive maintenance tooling: record
 * lookup by offset, record count, record iteration, and
 * reconstruction of a minimal central directory from the
 * recovered records.
 */

uint32_t vp_repair_count(const vp_repair_ctx_t *r)
{
    return r ? r->count : 0;
}

int vp_repair_at(const vp_repair_ctx_t *r, uint32_t idx,
                 vp_repair_record_t *out)
{
    if (!r || !out || idx >= r->count) return VP_ERR_RANGE;
    *out = r->records[idx];
    return VP_OK;
}

uint32_t vp_repair_total_raw(const vp_repair_ctx_t *r)
{
    if (!r) return 0;
    uint32_t total = 0;
    for (uint32_t i = 0; i < r->count; i++) total += r->records[i].raw_size;
    return total;
}

uint32_t vp_repair_total_comp(const vp_repair_ctx_t *r)
{
    if (!r) return 0;
    uint32_t total = 0;
    for (uint32_t i = 0; i < r->count; i++) total += r->records[i].comp_size;
    return total;
}

int vp_repair_find_at_offset(const vp_repair_ctx_t *r, uint64_t offset)
{
    if (!r) return -1;
    for (uint32_t i = 0; i < r->count; i++) {
        if (r->records[i].offset == offset) return (int)i;
    }
    return -1;
}

int vp_repair_iter(const vp_repair_ctx_t *r,
                   vp_repair_iter_cb cb, void *user)
{
    if (!r || !cb) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < r->count; i++) {
        if (!cb(&r->records[i], user)) return VP_ERR_TRUNCATED;
    }
    return VP_OK;
}

int vp_repair_emit_directory(const vp_repair_ctx_t *r,
                             uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!r || !out) return VP_ERR_INVALID;
    if (out_cap < (size_t)r->count * 58u) return VP_ERR_FULL;
    uint8_t *p = out;
    for (uint32_t i = 0; i < r->count; i++) {
        const vp_repair_record_t *rec = &r->records[i];
        memset(p, 0, 58);
        snprintf((char *)p, 32, "recovered_%u", i);
        p[32] = (uint8_t)rec->offset;
        p[33] = (uint8_t)(rec->offset >> 8);
        p[34] = (uint8_t)(rec->offset >> 16);
        p[35] = (uint8_t)(rec->offset >> 24);
        p[36] = (uint8_t)(rec->offset >> 32);
        p[37] = (uint8_t)(rec->offset >> 40);
        p[38] = (uint8_t)(rec->offset >> 48);
        p[39] = (uint8_t)(rec->offset >> 56);
        p[40] = (uint8_t)rec->raw_size;
        p[41] = (uint8_t)(rec->raw_size >> 8);
        p[42] = (uint8_t)(rec->raw_size >> 16);
        p[43] = (uint8_t)(rec->raw_size >> 24);
        p[44] = 0;
        p[45] = 0;
        p[46] = 0;
        p[47] = 0;
        p += 58;
    }
    if (out_len) *out_len = (size_t)r->count * 58u;
    return VP_OK;
}

int vp_repair_rescan(vp_repair_ctx_t *r)
{
    if (!r) return VP_ERR_INVALID;
    r->count = 0;
    return vp_repair_scan(r);
}

int vp_repair_validate(const vp_repair_ctx_t *r)
{
    if (!r) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < r->count; i++) {
        if (r->records[i].comp_size > 0
            && r->records[i].offset + 24 + r->records[i].comp_size > r->blob_len) {
            return VP_ERR_RANGE;
        }
    }
    return VP_OK;
}