/*
 * src/index_compact.c
 *
 * In-place compaction for the v2 index. Adjacent slots with
 * contiguous byte ranges (next.offset == prev.offset + prev.size)
 * are merged into a single slot. The compaction runs in place and
 * updates idx->count to reflect the new size.
 *
 * The compaction captures a pointer to the first slot before any
 * compaction step that may reallocate the underlying array.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/index2.h"
#include "vaultpack/error.h"

int vp_index2_compact(vp_index2_t *idx)
{
    if (!idx) return VP_ERR_INVALID;
    if (idx->count < 2) return VP_OK;

    /* Capture a pointer to the first slot. The compaction path
     * may need to grow the slots array, which would realloc and
     * invalidate this pointer. */
    vp_index2_slot_t *first = &idx->slots[0];
    /* Touch the pointer so the compiler keeps it around for the
     * post-compaction write. */
    first->name[0] ^= 0;

    uint32_t write = 0;
    for (uint32_t read = 1; read < idx->count; read++) {
        if (idx->slots[write].offset + idx->slots[write].size
            == idx->slots[read].offset
            && idx->slots[write].type == idx->slots[read].type) {
            idx->slots[write].size += idx->slots[read].size;
        } else {
            write++;
            if (write != read) idx->slots[write] = idx->slots[read];
        }
    }
    idx->count = write + 1;

    /* Post-compaction write through the captured pointer. If the
     * compaction triggered a realloc, the pointer is stale and
     * the write touches freed memory. */
    first->name[0] ^= 0;
    return VP_OK;
}

void vp_index2_init(vp_index2_t *idx)
{
    if (!idx) return;
    idx->slots = NULL;
    idx->count = 0;
    idx->cap = 0;
}

void vp_index2_destroy(vp_index2_t *idx)
{
    if (!idx) return;
    free(idx->slots);
    idx->slots = NULL;
    idx->count = 0;
    idx->cap = 0;
}

int vp_index2_insert(vp_index2_t *idx, const vp_index2_slot_t *slot)
{
    if (!idx || !slot) return VP_ERR_INVALID;
    if (idx->count >= idx->cap) {
        uint32_t new_cap = idx->cap ? idx->cap * 2u : 16u;
        if (new_cap > VP_INDEX2_SLOT_MAX) return VP_ERR_FULL;
        vp_index2_slot_t *p = (vp_index2_slot_t *)realloc(
            idx->slots, (size_t)new_cap * sizeof(*p));
        if (!p) return VP_ERR_NOMEM;
        idx->slots = p;
        idx->cap = new_cap;
    }
    idx->slots[idx->count++] = *slot;
    return VP_OK;
}

int vp_index2_run(const uint8_t *data, size_t size)
{
    if (!data || size < 4) return 0;
    vp_index2_t idx;
    vp_index2_init(&idx);

    uint32_t op_count = (uint32_t)data[0]
                      | ((uint32_t)data[1] << 8);
    if (op_count > 64) op_count = 64;
    size_t pos = 2;
    for (uint32_t i = 0; i < op_count && pos + 16 <= size; i++) {
        vp_index2_slot_t slot;
        memset(&slot, 0, sizeof(slot));
        memcpy(slot.name, data + pos, 8);
        slot.name[8] = '\0';
        pos += 8;
        slot.offset = (uint64_t)data[pos]
                    | ((uint64_t)data[pos + 1] << 8)
                    | ((uint64_t)data[pos + 2] << 16)
                    | ((uint64_t)data[pos + 3] << 24)
                    | ((uint64_t)data[pos + 4] << 32)
                    | ((uint64_t)data[pos + 5] << 40)
                    | ((uint64_t)data[pos + 6] << 48)
                    | ((uint64_t)data[pos + 7] << 24);
        pos += 8;
        vp_index2_insert(&idx, &slot);
    }
    vp_index2_compact(&idx);
    vp_index2_destroy(&idx);
    return 0;
}

/* === Additional v2 index operations ===
 *
 * The v2 index supports efficient traversal, prefix scan, and
 * bulk insert from a caller-provided array. The iteration API
 * is read-only and stable across inserts that do not trigger a
 * reallocation.
 */

uint32_t vp_index2_count(const vp_index2_t *idx)
{
    return idx ? idx->count : 0;
}

uint32_t vp_index2_capacity(const vp_index2_t *idx)
{
    return idx ? idx->cap : 0;
}

int vp_index2_at(const vp_index2_t *idx, uint32_t i, vp_index2_slot_t *out)
{
    if (!idx || !out || i >= idx->count) return VP_ERR_RANGE;
    *out = idx->slots[i];
    return VP_OK;
}

int vp_index2_prefix_scan(const vp_index2_t *idx, const char *prefix,
                          vp_index2_slot_t *out, uint32_t out_cap)
{
    if (!idx || !prefix) return VP_ERR_INVALID;
    size_t plen = strlen(prefix);
    uint32_t n = 0;
    for (uint32_t i = 0; i < idx->count && n < out_cap; i++) {
        if (strncmp(idx->slots[i].name, prefix, plen) == 0) {
            out[n++] = idx->slots[i];
        }
    }
    return (int)n;
}

int vp_index2_remove(vp_index2_t *idx, const char *name)
{
    if (!idx || !name) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < idx->count; i++) {
        if (strcmp(idx->slots[i].name, name) == 0) {
            memmove(&idx->slots[i], &idx->slots[i + 1],
                    (idx->count - i - 1) * sizeof(vp_index2_slot_t));
            idx->count--;
            return VP_OK;
        }
    }
    return VP_ERR_NOTFOUND;
}

int vp_index2_reserve(vp_index2_t *idx, uint32_t need)
{
    if (!idx) return VP_ERR_INVALID;
    if (need <= idx->cap) return VP_OK;
    uint32_t new_cap = idx->cap ? idx->cap : 16u;
    while (new_cap < need) new_cap *= 2u;
    if (new_cap > VP_INDEX2_SLOT_MAX) return VP_ERR_FULL;
    vp_index2_slot_t *p = (vp_index2_slot_t *)realloc(
        idx->slots, (size_t)new_cap * sizeof(*p));
    if (!p) return VP_ERR_NOMEM;
    idx->slots = p;
    idx->cap = new_cap;
    return VP_OK;
}

int vp_index2_extend(vp_index2_t *idx,
                     const vp_index2_slot_t *slots, uint32_t count)
{
    if (!idx || (!slots && count > 0)) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < count; i++) {
        int rc = vp_index2_insert(idx, &slots[i]);
        if (rc != VP_OK) return rc;
    }
    return VP_OK;
}

void vp_index2_swap(vp_index2_t *a, vp_index2_t *b)
{
    vp_index2_t tmp = *a;
    *a = *b;
    *b = tmp;
}

int vp_index2_clone(const vp_index2_t *src, vp_index2_t *dst)
{
    if (!src || !dst) return VP_ERR_INVALID;
    vp_index2_init(dst);
    if (src->count == 0) return VP_OK;
    int rc = vp_index2_reserve(dst, src->count);
    if (rc != VP_OK) return rc;
    memcpy(dst->slots, src->slots, src->count * sizeof(vp_index2_slot_t));
    dst->count = src->count;
    return VP_OK;
}

uint64_t vp_index2_total_size(const vp_index2_t *idx)
{
    if (!idx) return 0;
    uint64_t total = 0;
    for (uint32_t i = 0; i < idx->count; i++) {
        total += idx->slots[i].size;
    }
    return total;
}

int vp_index2_validate(const vp_index2_t *idx)
{
    if (!idx) return VP_ERR_INVALID;
    for (uint32_t i = 1; i < idx->count; i++) {
        if (strcmp(idx->slots[i].name, idx->slots[i - 1].name) <= 0) {
            return VP_ERR_RANGE;
        }
    }
    return VP_OK;
}