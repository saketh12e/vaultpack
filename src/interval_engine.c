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
    uint64_t merged_start = start;
    uint64_t merged_end   = end;
    while (i < e->count && e->ranges[i].start <= merged_end) {
        if (e->ranges[i].start < merged_start) merged_start = e->ranges[i].start;
        if (e->ranges[i].end > merged_end)     merged_end   = e->ranges[i].end;
        i++;
    }
    /* Replace the merged range with a single entry at position i-1 */
    memmove(&e->ranges[i + 1], &e->ranges[i],
            (e->count - i) * sizeof(vp_interval_t));
    e->ranges[i].start = merged_start;
    e->ranges[i].end   = merged_end;
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