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

/* Test entry point used by the bridge. */
int vp_index2_run(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* VAULTPACK_INDEX2_H */