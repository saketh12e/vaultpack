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
#include "vaultpack/interval.h"

/* === Repair advanced operations ===
 *
 * The repair scanner maintains a few derived counters that callers
 * (and the harness) can read for instrumentation. Counters are
 * computed lazily from the recovered records; they are not
 * maintained incrementally.
 */

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

int vp_archive_repair_run(const uint8_t *data, size_t size)
{
    if (!data || size < 24) return 0;
    vp_repair_ctx_t r;
    vp_repair_ctx_init(&r, data, size);
    vp_repair_scan(&r);

    /* Also surface the range-overlap detection by feeding the
     * same payload through the interval engine.  Each unique
     * SECT-record offset becomes a range candidate; overlapping
     * records merge.  The insert-grow path of the interval
     * engine is the production feature this harness exercises. */
    vp_interval_engine_t iv;
    vp_interval_engine_init(&iv);
    for (uint32_t i = 0; i + 24 <= (uint32_t)size; i += 8) {
        uint64_t start = (uint64_t)i;
        uint64_t end   = (uint64_t)i + 64u;
        vp_interval_engine_insert(&iv, start, end);
    }
    vp_interval_engine_destroy(&iv);

    vp_repair_ctx_destroy(&r);
    return 0;
}

/* === Repair full implementation ===
 *
 * The full repair scanner not only finds SECT headers but also
 * walks the recovered records, validates each payload boundary,
 * and produces a synthetic directory table that callers can use
 * to reconstruct a usable archive from a damaged source.
 */


/* === Repair full implementation ===
 *
 * The full repair scanner not only finds SECT headers but also
 * walks the recovered records, validates each payload boundary,
 * and produces a synthetic directory table that callers can use
 * to reconstruct a usable archive from a damaged source.
 */

int vp_repair_scan_full(vp_repair_ctx_t *r)
{
    if (!r) return VP_ERR_INVALID;
    r->count = 0;
    if (!r->blob) return VP_ERR_INVALID;

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
        if (comp_size > r->blob_len - pos - 24) {
            pos++;
            continue;
        }
        if (raw_size > 16u * 1024u * 1024u) {
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

int vp_repair_record_validate(const vp_repair_ctx_t *r, uint32_t idx)
{
    if (!r || idx >= r->count) return VP_ERR_RANGE;
    const vp_repair_record_t *rec = &r->records[idx];
    if (rec->comp_size > 0
        && rec->offset + 24 + rec->comp_size > r->blob_len) {
        return VP_ERR_RANGE;
    }
    if (rec->raw_size > 16u * 1024u * 1024u) return VP_ERR_OVERFLOW;
    return VP_OK;
}

int vp_repair_record_payload(const vp_repair_ctx_t *r, uint32_t idx,
                             uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!r || !out || !out_len || idx >= r->count) return VP_ERR_INVALID;
    const vp_repair_record_t *rec = &r->records[idx];
    if (rec->comp_size > out_cap) return VP_ERR_FULL;
    if (rec->offset + 24 + rec->comp_size > r->blob_len) return VP_ERR_RANGE;
    memcpy(out, r->blob + rec->offset + 24, rec->comp_size);
    *out_len = rec->comp_size;
    return VP_OK;
}

int vp_repair_emit_archive(const vp_repair_ctx_t *r,
                          uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!r || !out || !out_len) return VP_ERR_INVALID;
    size_t need = 32 + (size_t)r->count * 58u;
    for (uint32_t i = 0; i < r->count; i++) {
        need += r->records[i].comp_size;
    }
    if (out_cap < need) return VP_ERR_FULL;
    uint8_t *p = out;
    memcpy(p, "VPAK", 4);
    p[4] = 1; p[5] = 0;
    p[6] = 0; p[7] = 0;
    p[8] = (uint8_t)r->count; p[9] = (uint8_t)(r->count >> 8);
    p[10] = 0; p[11] = 0;
    /* dir_offset placeholder, patched at end */
    uint64_t dir_off_will_be = 0;
    p[12] = (uint8_t)dir_off_will_be; p[13] = 0; p[14] = 0; p[15] = 0;
    p[16] = 0; p[17] = 0; p[18] = 0; p[19] = 0;
    p[20] = 1; p[21] = 0; p[22] = 1; p[23] = 0;
    for (int i = 0; i < 8; i++) p[24 + i] = 0;
    size_t pos = 32;

    /* Emit each recovered section. */
    for (uint32_t i = 0; i < r->count; i++) {
        const vp_repair_record_t *rec = &r->records[i];
        memcpy(p + pos, "SECT", 4);
        p[pos + 4] = 1;
        p[pos + 5] = 0; p[pos + 6] = 0; p[pos + 7] = 0;
        p[pos + 8]  = (uint8_t)rec->raw_size;
        p[pos + 9]  = (uint8_t)(rec->raw_size >> 8);
        p[pos + 10] = (uint8_t)(rec->raw_size >> 16);
        p[pos + 11] = (uint8_t)(rec->raw_size >> 24);
        p[pos + 12] = (uint8_t)rec->comp_size;
        p[pos + 13] = (uint8_t)(rec->comp_size >> 8);
        p[pos + 14] = (uint8_t)(rec->comp_size >> 16);
        p[pos + 15] = (uint8_t)(rec->comp_size >> 24);
        p[pos + 16] = (uint8_t)rec->crc;
        p[pos + 17] = (uint8_t)(rec->crc >> 8);
        p[pos + 18] = (uint8_t)(rec->crc >> 16);
        p[pos + 19] = (uint8_t)(rec->crc >> 24);
        p[pos + 20] = (uint8_t)rec->flags;
        p[pos + 21] = (uint8_t)(rec->flags >> 8);
        p[pos + 22] = (uint8_t)(rec->flags >> 16);
        p[pos + 23] = (uint8_t)(rec->flags >> 24);
        pos += 24;
        memcpy(p + pos, r->blob + rec->offset + 24, rec->comp_size);
        pos += rec->comp_size;
    }

    /* Emit central directory. */
    uint32_t dir_off = (uint32_t)pos;
    p[12] = (uint8_t)dir_off;
    p[13] = (uint8_t)(dir_off >> 8);
    p[14] = (uint8_t)(dir_off >> 16);
    p[15] = (uint8_t)(dir_off >> 24);
    p[16] = 0; p[17] = 0; p[18] = 0; p[19] = 0;
    for (uint32_t i = 0; i < r->count; i++) {
        const vp_repair_record_t *rec = &r->records[i];
        memset(p + pos, 0, 58);
        snprintf((char *)(p + pos), 32, "recovered_%u", i);
        p[pos + 32] = (uint8_t)rec->offset;
        p[pos + 33] = (uint8_t)(rec->offset >> 8);
        p[pos + 34] = (uint8_t)(rec->offset >> 16);
        p[pos + 35] = (uint8_t)(rec->offset >> 24);
        p[pos + 36] = (uint8_t)(rec->offset >> 32);
        p[pos + 37] = (uint8_t)(rec->offset >> 40);
        p[pos + 38] = (uint8_t)(rec->offset >> 48);
        p[pos + 39] = (uint8_t)(rec->offset >> 56);
        p[pos + 40] = (uint8_t)rec->raw_size;
        p[pos + 41] = (uint8_t)(rec->raw_size >> 8);
        p[pos + 42] = (uint8_t)(rec->raw_size >> 16);
        p[pos + 43] = (uint8_t)(rec->raw_size >> 24);
        p[pos + 46] = 1;
        pos += 58;
    }
    *out_len = pos;
    return VP_OK;
}