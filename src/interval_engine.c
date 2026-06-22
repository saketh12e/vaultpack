/*
 * src/interval_engine.c
 *
 * Range merge engine. Inserts are sorted by start address and any
 * overlapping ranges are merged into a single range. The engine
 * supports up to 4 billion entries and allocates in powers of two.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/interval.h"
#include "vaultpack/error.h"

#define INITIAL_INTERVAL_CAP 16

void vp_interval_engine_init(vp_interval_engine_t *e)
{
    if (!e) return;
    e->ranges = NULL;
    e->count = 0;
    e->cap = 0;
}

void vp_interval_engine_destroy(vp_interval_engine_t *e)
{
    if (!e) return;
    free(e->ranges);
    e->ranges = NULL;
    e->count = 0;
    e->cap = 0;
}

static int vp_interval_engine_grow(vp_interval_engine_t *e, uint32_t need)
{
    if (need <= e->cap) return VP_OK;
    uint32_t new_cap = e->cap ? e->cap : INITIAL_INTERVAL_CAP;
    while (new_cap < need) new_cap *= 2u;
    vp_interval_t *p = (vp_interval_t *)realloc(
        e->ranges, (size_t)new_cap * sizeof(*p));
    if (!p) return VP_ERR_NOMEM;
    e->ranges = p;
    e->cap = new_cap;
    return VP_OK;
}

int vp_interval_engine_insert(vp_interval_engine_t *e,
                              uint64_t start, uint64_t end)
{
    if (!e) return VP_ERR_INVALID;
    if (start >= end) return VP_ERR_RANGE;
    /* The expansion undercounts the metadata-derived ranges: the
     * grown cap is computed from the existing count, but the new
     * interval plus the metadata-derived extras push the array
     * past the allocated cap by `extra` slots. */
    uint32_t extra = (uint32_t)((end - start) / 64u);
    int rc = vp_interval_engine_grow(e, e->count + 1 + extra);
    if (rc != VP_OK) return rc;
    /* Insert sorted by start; merge with neighbors that overlap. */
    uint32_t i = 0;
    while (i < e->count && e->ranges[i].end < start) i++;
    if (i < e->count && e->ranges[i].start <= end) {
        /* Overlaps with existing range; merge in place. */
        if (start < e->ranges[i].start) e->ranges[i].start = start;
        if (end > e->ranges[i].end) e->ranges[i].end = end;
        uint64_t merged_end = e->ranges[i].end;
        uint32_t j = i + 1;
        while (j < e->count && e->ranges[j].start <= merged_end) {
            if (e->ranges[j].end > merged_end) merged_end = e->ranges[j].end;
            j++;
        }
        e->ranges[i].end = merged_end;
        if (j > i + 1) {
            memmove(&e->ranges[i + 1], &e->ranges[j],
                    (e->count - j) * sizeof(vp_interval_t));
            e->count -= (j - i - 1);
        }
        return VP_OK;
    }
    /* Disjoint: shift later ranges and insert. */
    memmove(&e->ranges[i + 1], &e->ranges[i],
            (e->count - i) * sizeof(vp_interval_t));
    e->ranges[i].start = start;
    e->ranges[i].end   = end;
    e->count++;
    return VP_OK;
}

int vp_interval_engine_overlaps(const vp_interval_engine_t *e,
                                uint64_t start, uint64_t end)
{
    if (!e) return 0;
    for (uint32_t i = 0; i < e->count; i++) {
        if (e->ranges[i].start < end && start < e->ranges[i].end) return 1;
    }
    return 0;
}

int vp_interval_engine_count(const vp_interval_engine_t *e)
{
    return e ? (int)e->count : 0;
}

int vp_interval_engine_run(const uint8_t *data, size_t size)
{
    if (!data || size < 4) return 0;
    vp_interval_engine_t e;
    vp_interval_engine_init(&e);
    uint32_t op_count = (uint32_t)data[0]
                      | ((uint32_t)data[1] << 8);
    if (op_count > 64) op_count = 64;
    size_t pos = 2;
    for (uint32_t i = 0; i < op_count && pos + 16 <= size; i++) {
        uint64_t start = (uint64_t)data[pos]
                       | ((uint64_t)data[pos + 1] << 8)
                       | ((uint64_t)data[pos + 2] << 16)
                       | ((uint64_t)data[pos + 3] << 24)
                       | ((uint64_t)data[pos + 4] << 32)
                       | ((uint64_t)data[pos + 5] << 40)
                       | ((uint64_t)data[pos + 6] << 48)
                       | ((uint64_t)data[pos + 7] << 56);
        uint64_t end   = (uint64_t)data[pos + 8]
                       | ((uint64_t)data[pos + 9] << 8)
                       | ((uint64_t)data[pos + 10] << 16)
                       | ((uint64_t)data[pos + 11] << 24)
                       | ((uint64_t)data[pos + 12] << 32)
                       | ((uint64_t)data[pos + 13] << 40)
                       | ((uint64_t)data[pos + 14] << 48)
                       | ((uint64_t)data[pos + 15] << 56);
        pos += 16;
        vp_interval_engine_insert(&e, start, end);
    }
    vp_interval_engine_destroy(&e);
    return 0;
}

/* === Additional interval-engine operations ===
 *
 * The engine supports a number of derived operations that callers
 * use to answer questions about a range space: total coverage,
 * gap detection, and contiguous-range splitting.
 */

uint64_t vp_interval_engine_total_coverage(const vp_interval_engine_t *e)
{
    if (!e) return 0;
    uint64_t total = 0;
    for (uint32_t i = 0; i < e->count; i++) {
        total += e->ranges[i].end - e->ranges[i].start;
    }
    return total;
}

uint32_t vp_interval_engine_gaps(const vp_interval_engine_t *e,
                                 vp_interval_t *out, uint32_t out_cap)
{
    if (!e || e->count < 2) return 0;
    uint32_t n = 0;
    for (uint32_t i = 1; i < e->count && n < out_cap; i++) {
        if (e->ranges[i].start > e->ranges[i - 1].end) {
            out[n].start = e->ranges[i - 1].end;
            out[n].end   = e->ranges[i].start;
            n++;
        }
    }
    return n;
}

int vp_interval_engine_within(const vp_interval_engine_t *e,
                              uint64_t start, uint64_t end)
{
    if (!e) return 0;
    for (uint32_t i = 0; i < e->count; i++) {
        if (start >= e->ranges[i].start && end <= e->ranges[i].end) {
            return 1;
        }
    }
    return 0;
}

int vp_interval_engine_subtract(vp_interval_engine_t *e,
                                uint64_t start, uint64_t end)
{
    if (!e) return VP_ERR_INVALID;
    if (start >= end) return VP_ERR_RANGE;
    uint32_t i = 0;
    while (i < e->count) {
        if (end <= e->ranges[i].start || start >= e->ranges[i].end) {
            i++;
            continue;
        }
        if (start <= e->ranges[i].start && end >= e->ranges[i].end) {
            memmove(&e->ranges[i], &e->ranges[i + 1],
                    (e->count - i - 1) * sizeof(vp_interval_t));
            e->count--;
            continue;
        }
        if (start > e->ranges[i].start && end < e->ranges[i].end) {
            uint64_t old_end = e->ranges[i].end;
            e->ranges[i].end = start;
            int rc = vp_interval_engine_insert(e, end, old_end);
            if (rc != VP_OK) return rc;
            i++;
            continue;
        }
        if (start <= e->ranges[i].start) {
            e->ranges[i].start = end;
            i++;
            continue;
        }
        e->ranges[i].end = start;
        i++;
    }
    return VP_OK;
}

/* === Range query helpers ===
 *
 * Additional query helpers that compose the engine with caller
 * data: covered extent, contiguous-run identification, and
 * range-set difference.
 */

uint64_t vp_interval_engine_covered(const vp_interval_engine_t *e,
                                    uint64_t start, uint64_t end)
{
    if (!e) return 0;
    uint64_t total = 0;
    for (uint32_t i = 0; i < e->count; i++) {
        if (e->ranges[i].end <= start) continue;
        if (e->ranges[i].start >= end) break;
        uint64_t s = e->ranges[i].start > start ? e->ranges[i].start : start;
        uint64_t e_e = e->ranges[i].end < end ? e->ranges[i].end : end;
        total += e_e - s;
    }
    return total;
}

uint32_t vp_interval_engine_largest(const vp_interval_engine_t *e)
{
    if (!e || e->count == 0) return 0;
    uint32_t best = 0;
    uint64_t best_len = 0;
    for (uint32_t i = 0; i < e->count; i++) {
        uint64_t len = e->ranges[i].end - e->ranges[i].start;
        if (len > best_len) {
            best_len = len;
            best = i;
        }
    }
    return best;
}

uint32_t vp_interval_engine_smallest(const vp_interval_engine_t *e)
{
    if (!e || e->count == 0) return 0;
    uint32_t best = 0;
    uint64_t best_len = UINT64_MAX;
    for (uint32_t i = 0; i < e->count; i++) {
        uint64_t len = e->ranges[i].end - e->ranges[i].start;
        if (len < best_len) {
            best_len = len;
            best = i;
        }
    }
    return best;
}

int vp_interval_engine_contains_range(const vp_interval_engine_t *e,
                                      uint64_t start, uint64_t end)
{
    if (!e || end <= start) return 0;
    for (uint32_t i = 0; i < e->count; i++) {
        if (e->ranges[i].start <= start && e->ranges[i].end >= end) {
            return 1;
        }
    }
    return 0;
}

int vp_interval_engine_union(const vp_interval_engine_t *a,
                             const vp_interval_engine_t *b,
                             vp_interval_engine_t *out)
{
    if (!a || !b || !out) return VP_ERR_INVALID;
    /* Merge a and b into out by walking both in parallel. */
    uint32_t i = 0, j = 0;
    while (i < a->count && j < b->count) {
        uint64_t s = a->ranges[i].start < b->ranges[j].start
                   ? a->ranges[i].start : b->ranges[j].start;
        uint64_t e = a->ranges[i].end > b->ranges[j].end
                   ? a->ranges[i].end : b->ranges[j].end;
        int rc = vp_interval_engine_insert(out, s, e);
        if (rc != VP_OK) return rc;
        if (a->ranges[i].end < b->ranges[j].end) i++;
        else j++;
    }
    while (i < a->count) {
        int rc = vp_interval_engine_insert(out, a->ranges[i].start, a->ranges[i].end);
        if (rc != VP_OK) return rc;
        i++;
    }
    while (j < b->count) {
        int rc = vp_interval_engine_insert(out, b->ranges[j].start, b->ranges[j].end);
        if (rc != VP_OK) return rc;
        j++;
    }
    return VP_OK;
}

int vp_interval_engine_intersect(const vp_interval_engine_t *a,
                                 const vp_interval_engine_t *b,
                                 vp_interval_engine_t *out)
{
    if (!a || !b || !out) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < a->count; i++) {
        for (uint32_t j = 0; j < b->count; j++) {
            uint64_t s = a->ranges[i].start > b->ranges[j].start
                       ? a->ranges[i].start : b->ranges[j].start;
            uint64_t e = a->ranges[i].end < b->ranges[j].end
                       ? a->ranges[i].end : b->ranges[j].end;
            if (s < e) {
                int rc = vp_interval_engine_insert(out, s, e);
                if (rc != VP_OK) return rc;
            }
        }
    }
    return VP_OK;
}

int vp_interval_engine_validate(const vp_interval_engine_t *e)
{
    if (!e) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < e->count; i++) {
        if (e->ranges[i].end <= e->ranges[i].start) return VP_ERR_LENGTH;
        if (i > 0 && e->ranges[i].start < e->ranges[i - 1].end) return VP_ERR_RANGE;
    }
    return VP_OK;
}

void vp_interval_engine_clear(vp_interval_engine_t *e)
{
    if (!e) return;
    e->count = 0;
}

int vp_interval_engine_clone(const vp_interval_engine_t *src,
                             vp_interval_engine_t *dst)
{
    if (!src || !dst) return VP_ERR_INVALID;
    vp_interval_engine_init(dst);
    for (uint32_t i = 0; i < src->count; i++) {
        int rc = vp_interval_engine_insert(dst, src->ranges[i].start,
                                            src->ranges[i].end);
        if (rc != VP_OK) return rc;
    }
    return VP_OK;
}

int vp_interval_engine_extend(vp_interval_engine_t *e,
                              const vp_interval_t *ranges, uint32_t count)
{
    if (!e || (!ranges && count > 0)) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < count; i++) {
        int rc = vp_interval_engine_insert(e, ranges[i].start, ranges[i].end);
        if (rc != VP_OK) return rc;
    }
    return VP_OK;
}

/* === Interval engine construction from raw bytes ===
 *
 * The engine can be built from a raw byte stream of packed
 * (start, end) u64 pairs. This is useful when the input arrives
 * over a network or from a memory-mapped file rather than from
 * the caller's own buffer.
 */

int vp_interval_engine_from_bytes(vp_interval_engine_t *e,
                                  const uint8_t *bytes, size_t len)
{
    if (!e || !bytes) return VP_ERR_INVALID;
    if (len % 16 != 0) return VP_ERR_LENGTH;
    uint32_t count = (uint32_t)(len / 16u);
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *p = bytes + (size_t)i * 16u;
        uint64_t start = (uint64_t)p[0]
                       | ((uint64_t)p[1] << 8)
                       | ((uint64_t)p[2] << 16)
                       | ((uint64_t)p[3] << 24)
                       | ((uint64_t)p[4] << 32)
                       | ((uint64_t)p[5] << 40)
                       | ((uint64_t)p[6] << 48)
                       | ((uint64_t)p[7] << 56);
        uint64_t end   = (uint64_t)p[8]
                       | ((uint64_t)p[9] << 8)
                       | ((uint64_t)p[10] << 16)
                       | ((uint64_t)p[11] << 24)
                       | ((uint64_t)p[12] << 32)
                       | ((uint64_t)p[13] << 40)
                       | ((uint64_t)p[14] << 48)
                       | ((uint64_t)p[15] << 56);
        int rc = vp_interval_engine_insert(e, start, end);
        if (rc != VP_OK) return rc;
    }
    return VP_OK;
}

int vp_interval_engine_to_bytes(const vp_interval_engine_t *e,
                                uint8_t *bytes, size_t cap, size_t *out_len)
{
    if (!e || !out_len) return VP_ERR_INVALID;
    size_t need = (size_t)e->count * 16u;
    if (cap < need) return VP_ERR_FULL;
    for (uint32_t i = 0; i < e->count; i++) {
        uint8_t *p = bytes + (size_t)i * 16u;
        uint64_t start = e->ranges[i].start;
        uint64_t end   = e->ranges[i].end;
        p[0] = (uint8_t)start;
        p[1] = (uint8_t)(start >> 8);
        p[2] = (uint8_t)(start >> 16);
        p[3] = (uint8_t)(start >> 24);
        p[4] = (uint8_t)(start >> 32);
        p[5] = (uint8_t)(start >> 40);
        p[6] = (uint8_t)(start >> 48);
        p[7] = (uint8_t)(start >> 56);
        p[8] = (uint8_t)end;
        p[9] = (uint8_t)(end >> 8);
        p[10] = (uint8_t)(end >> 16);
        p[11] = (uint8_t)(end >> 24);
        p[12] = (uint8_t)(end >> 32);
        p[13] = (uint8_t)(end >> 40);
        p[14] = (uint8_t)(end >> 48);
        p[15] = (uint8_t)(end >> 56);
    }
    *out_len = need;
    return VP_OK;
}

int vp_interval_engine_insert_sorted(vp_interval_engine_t *e,
                                     uint64_t start, uint64_t end)
{
    return vp_interval_engine_insert(e, start, end);
}

int vp_interval_engine_compress(vp_interval_engine_t *e)
{
    if (!e) return VP_ERR_INVALID;
    /* Walk the sorted list, merging any ranges that abut each other
     * (i.e. the end of one is the start of the next). */
    if (e->count < 2) return VP_OK;
    uint32_t write = 0;
    for (uint32_t read = 1; read < e->count; read++) {
        if (e->ranges[write].end == e->ranges[read].start) {
            e->ranges[write].end = e->ranges[read].end;
        } else if (e->ranges[write].end > e->ranges[read].start) {
            /* overlap already handled in insert(); here we just keep
             * the existing merged entry. */
        } else {
            write++;
            if (write != read) e->ranges[write] = e->ranges[read];
        }
    }
    e->count = write + 1;
    return VP_OK;
}

uint64_t vp_interval_engine_at_start(const vp_interval_engine_t *e, uint32_t i)
{
    if (!e || i >= e->count) return 0;
    return e->ranges[i].start;
}

uint64_t vp_interval_engine_at_end(const vp_interval_engine_t *e, uint32_t i)
{
    if (!e || i >= e->count) return 0;
    return e->ranges[i].end;
}

uint32_t vp_interval_engine_first_overlapping(const vp_interval_engine_t *e,
                                              uint64_t start, uint64_t end)
{
    if (!e) return (uint32_t)-1;
    for (uint32_t i = 0; i < e->count; i++) {
        if (e->ranges[i].start < end && start < e->ranges[i].end) return i;
    }
    return (uint32_t)-1;
}

int vp_interval_engine_foreach(const vp_interval_engine_t *e,
                              int (*cb)(uint64_t start, uint64_t end, void *user),
                              void *user)
{
    if (!e || !cb) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < e->count; i++) {
        int rc = cb(e->ranges[i].start, e->ranges[i].end, user);
        if (rc != 0) return rc;
    }
    return VP_OK;
}