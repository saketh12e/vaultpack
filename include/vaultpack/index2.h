/*
 * include/vaultpack/index2.h
 *
 * Sorted / compacted section index v2. The v2 index maintains a
 * sorted array of section descriptors keyed by name and supports
 * prefix-scan iteration. The compaction pass merges adjacent slots
 * with overlapping or contiguous ranges into a single slot.
 */
#ifndef VAULTPACK_INDEX2_H
#define VAULTPACK_INDEX2_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VP_INDEX2_SLOT_MAX 1024

typedef struct vp_index2_slot {
    char     name[32];
    uint64_t offset;
    uint32_t size;
    uint8_t  type;
} vp_index2_slot_t;

typedef struct vp_index2 {
    vp_index2_slot_t *slots;
    uint32_t          count;
    uint32_t          cap;
} vp_index2_t;

void vp_index2_init(vp_index2_t *idx);
void vp_index2_destroy(vp_index2_t *idx);

int  vp_index2_insert(vp_index2_t *idx, const vp_index2_slot_t *slot);
int  vp_index2_lookup(const vp_index2_t *idx, const char *name,
                      vp_index2_slot_t *out);
int  vp_index2_compact(vp_index2_t *idx);

/* Additional v2 index operations. */
uint32_t vp_index2_count(const vp_index2_t *idx);
uint32_t vp_index2_capacity(const vp_index2_t *idx);
int  vp_index2_at(const vp_index2_t *idx, uint32_t i, vp_index2_slot_t *out);
int  vp_index2_prefix_scan(const vp_index2_t *idx, const char *prefix,
                          vp_index2_slot_t *out, uint32_t out_cap);
int  vp_index2_remove(vp_index2_t *idx, const char *name);
int  vp_index2_reserve(vp_index2_t *idx, uint32_t need);
int  vp_index2_extend(vp_index2_t *idx,
                     const vp_index2_slot_t *slots, uint32_t count);
void vp_index2_swap(vp_index2_t *a, vp_index2_t *b);
int  vp_index2_clone(const vp_index2_t *src, vp_index2_t *dst);
uint64_t vp_index2_total_size(const vp_index2_t *idx);
int  vp_index2_validate(const vp_index2_t *idx);

/* Additional lookup operations. */
int  vp_index2_lower_bound(const vp_index2_t *idx, const char *name);
int  vp_index2_upper_bound(const vp_index2_t *idx, const char *name);

/* Sort alternatives. */
void vp_index2_sort_by_offset(vp_index2_t *idx);
void vp_index2_sort_by_size(vp_index2_t *idx);
void vp_index2_sort_by_type(vp_index2_t *idx);
void vp_index2_reverse(vp_index2_t *idx);
int  vp_index2_upper_bound(const vp_index2_t *idx, const char *name);
int  vp_index2_range_scan(const vp_index2_t *idx,
                          const char *lo_name, const char *hi_name,
                          vp_index2_slot_t *out, uint32_t out_cap);
int  vp_index2_contains(const vp_index2_t *idx, const char *name);
int  vp_index2_name_at(const vp_index2_t *idx, uint32_t i, char *out, size_t out_cap);
int  vp_index2_first(const vp_index2_t *idx, vp_index2_slot_t *out);
int  vp_index2_last(const vp_index2_t *idx, vp_index2_slot_t *out);
int  vp_index2_is_sorted(const vp_index2_t *idx);
uint32_t vp_index2_name_hash(const vp_index2_t *idx, uint32_t i);

/* Test entry point used by the bridge. */
int vp_index2_run(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* VAULTPACK_INDEX2_H */