#ifndef VAULTPACK_INDEX_H
#define VAULTPACK_INDEX_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/archive.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VP_SLOT_PRIMARY  0x01u
#define VP_SLOT_SUB      0x02u

typedef struct vp_index_slot {
    uint32_t key;
    uint32_t offset;
    uint32_t flags;
} vp_index_slot_t;

typedef struct vp_index {
    vp_index_slot_t *slots;
    uint32_t         capacity;
    uint32_t         used;
    uint16_t         width;
    uint16_t         depth;
} vp_index_t;

int vp_build_index(vp_index_t *idx, uint16_t width, uint16_t depth);
int vp_index_insert(vp_index_t *idx, uint32_t key, uint32_t offset);
int vp_index_populate(vp_index_t *idx, const vp_dir_entry_t *dir,
                      uint32_t dir_count);

#ifdef __cplusplus
}
#endif

#endif
