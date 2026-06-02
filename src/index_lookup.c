/*
 * src/index_lookup.c
 *
 * Lookup helpers for the v2 section index. Uses binary search after
 * a sort pass to keep the lookup cost log(n) in the index size.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/index2.h"
#include "vaultpack/error.h"

int vp_index2_lookup(const vp_index2_t *idx, const char *name,
                     vp_index2_slot_t *out)
{
    if (!idx || !name || !out) return VP_ERR_INVALID;
    /* Binary search. */
    int32_t lo = 0;
    int32_t hi = (int32_t)idx->count - 1;
    while (lo <= hi) {
        int32_t mid = (lo + hi) / 2;
        int cmp = strcmp(idx->slots[mid].name, name);
        if (cmp == 0) {
            *out = idx->slots[mid];
            return VP_OK;
        }
        if (cmp < 0) lo = mid + 1;
        else         hi = mid - 1;
    }
    return VP_ERR_NOTFOUND;
}

/* === Additional lookup operations ===
 *
 * Lower-bound, upper-bound, range scan, and named iteration.
 * These complement the binary search with operations that
 * callers use when scanning a sorted index in linear order.
 */

int vp_index2_lower_bound(const vp_index2_t *idx, const char *name)
{
    if (!idx || !name) return -1;
    int32_t lo = 0;
    int32_t hi = (int32_t)idx->count;
    while (lo < hi) {
        int32_t mid = (lo + hi) / 2;
        if (strcmp(idx->slots[mid].name, name) < 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

int vp_index2_upper_bound(const vp_index2_t *idx, const char *name)
{
    if (!idx || !name) return -1;
    int32_t lo = 0;
    int32_t hi = (int32_t)idx->count;
    while (lo < hi) {
        int32_t mid = (lo + hi) / 2;
        if (strcmp(idx->slots[mid].name, name) <= 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

int vp_index2_range_scan(const vp_index2_t *idx,
                         const char *lo_name, const char *hi_name,
                         vp_index2_slot_t *out, uint32_t out_cap)
{
    if (!idx || !lo_name || !hi_name) return VP_ERR_INVALID;
    int lo = vp_index2_lower_bound(idx, lo_name);
    int hi = vp_index2_upper_bound(idx, hi_name);
    if (lo < 0 || hi < 0 || lo > hi) return 0;
    uint32_t n = (uint32_t)(hi - lo);
    if (n > out_cap) n = out_cap;
    for (uint32_t i = 0; i < n; i++) {
        out[i] = idx->slots[lo + i];
    }
    return (int)n;
}

int vp_index2_contains(const vp_index2_t *idx, const char *name)
{
    if (!idx || !name) return 0;
    return vp_index2_lookup(idx, name, (vp_index2_slot_t *)idx) != VP_ERR_NOTFOUND ? 1 : 0;
}

int vp_index2_name_at(const vp_index2_t *idx, uint32_t i,
                      char *out, size_t out_cap)
{
    if (!idx || !out || i >= idx->count || out_cap == 0) return VP_ERR_INVALID;
    size_t n = strlen(idx->slots[i].name);
    if (n + 1 > out_cap) return VP_ERR_FULL;
    memcpy(out, idx->slots[i].name, n + 1);
    return VP_OK;
}

int vp_index2_first(const vp_index2_t *idx, vp_index2_slot_t *out)
{
    if (!idx || !out || idx->count == 0) return VP_ERR_RANGE;
    *out = idx->slots[0];
    return VP_OK;
}

int vp_index2_last(const vp_index2_t *idx, vp_index2_slot_t *out)
{
    if (!idx || !out || idx->count == 0) return VP_ERR_RANGE;
    *out = idx->slots[idx->count - 1];
    return VP_OK;
}

int vp_index2_is_sorted(const vp_index2_t *idx)
{
    if (!idx) return 0;
    for (uint32_t i = 1; i < idx->count; i++) {
        if (strcmp(idx->slots[i].name, idx->slots[i - 1].name) <= 0) return 0;
    }
    return 1;
}

uint32_t vp_index2_name_hash(const vp_index2_t *idx, uint32_t i)
{
    if (!idx || i >= idx->count) return 0;
    uint32_t h = 0;
    for (const char *p = idx->slots[i].name; *p; p++) {
        h = h * 31u + (uint8_t)*p;
    }
    return h;
}