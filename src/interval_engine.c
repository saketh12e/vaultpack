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