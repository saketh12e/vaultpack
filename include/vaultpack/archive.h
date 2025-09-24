#ifndef VAULTPACK_ARCHIVE_H
#define VAULTPACK_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/metadata.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VP_MAGIC_VPAK           0x4B415056u /* "VPAK" little-endian */
#define VP_MAGIC_VPAK_BYTES     "VPAK"
#define VP_ARCHIVE_HDR_SIZE     32
#define VP_ARCHIVE_VERSION      0x0001

#define VP_SECTION_RAW          0x01
#define VP_SECTION_COMPRESSED   0x02
#define VP_SECTION_METADATA     0x03

typedef struct vp_archive_hdr {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t entry_count;
    uint64_t dir_offset;
    uint16_t index_width;
    uint16_t index_depth;
    uint8_t  reserved[8];
} vp_archive_hdr_t;

typedef struct vp_dir_entry {
    char     name[32];
    uint64_t section_offset;
    uint32_t section_size;
    uint16_t flags;
    uint8_t  type;
    uint8_t  reserved;
    uint32_t meta_count;
    uint16_t sub_count;
    uint32_t sub_stride;
} vp_dir_entry_t;

typedef struct vp_archive {
    vp_archive_hdr_t  hdr;
    vp_dir_entry_t   *dir;
    uint32_t          dir_count;
    vp_meta_entry_t  *meta_flat;
    uint32_t          meta_total;
} vp_archive_t;

int vp_check_header(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif
