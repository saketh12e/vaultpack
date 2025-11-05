#ifndef VAULTPACK_METADATA_H
#define VAULTPACK_METADATA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vp_meta_entry {
    char key[24];
    char value[40];
} vp_meta_entry_t;

typedef struct vp_meta_block_hdr {
    uint8_t  tag[4];
    uint16_t key_count;
    uint16_t reserved;
} vp_meta_block_hdr_t;

typedef struct vp_meta_block {
    uint8_t           tag[4];
    uint16_t          key_count;
    uint16_t          reserved;
    vp_meta_entry_t  *entries;
} vp_meta_block_t;

int vp_read_metadata_block(const uint8_t *data, size_t size,
                           vp_meta_block_t **out);

#ifdef __cplusplus
}
#endif

#endif
