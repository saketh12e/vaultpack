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

/* struct field sizes for binary layout reference:
 *   vp_dir_entry_t = 32 (name) + 8 (offset) + 4 (size) + 2 (flags)
 *                  + 1 (type) + 1 (reserved) + 4 (meta_count)
 *                  + 2 (sub_count) + 4 (sub_stride)
 *                  = 58 bytes (no padding because u32/u64 are aligned) */

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
int vp_open_archive(const uint8_t *data, size_t size, vp_archive_t **out);
void vp_close_archive(vp_archive_t *arc);

typedef struct vp_dir_iter {
    const vp_archive_t *arc;
    uint32_t           pos;
} vp_dir_iter_t;

void vp_dir_iter_init(vp_dir_iter_t *it, const vp_archive_t *arc);
const vp_dir_entry_t *vp_dir_next(vp_dir_iter_t *it);

uint32_t vp_dir_count(const vp_archive_t *arc);
const vp_dir_entry_t *vp_dir_at(const vp_archive_t *arc, uint32_t pos);

#ifdef __cplusplus
}
#endif

#endif
