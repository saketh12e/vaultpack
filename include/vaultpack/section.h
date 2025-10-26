#ifndef VAULTPACK_SECTION_H
#define VAULTPACK_SECTION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VP_MAGIC_SECT       0x54434553u /* "SECT" little-endian */
#define VP_MAGIC_SECT_BYTES "SECT"
#define VP_SECTION_HDR_SIZE 20
#define VP_MAX_COMP_LEVEL   9

typedef struct vp_section_hdr {
    uint32_t magic;
    uint8_t  type;
    uint8_t  comp_method;
    uint8_t  comp_level;
    uint8_t  flags;
    uint32_t raw_size;
    uint32_t comp_size;
    uint32_t checksum;
    uint32_t reserved;
} vp_section_hdr_t;

typedef struct vp_comp_ctx {
    uint8_t method;
    uint8_t level;
} vp_comp_ctx_t;

typedef struct vp_section {
    vp_section_hdr_t  hdr;
    uint8_t           type;
    uint8_t           flags;
    vp_comp_ctx_t    *comp_ctx;
    uint8_t          *data;
    size_t            data_len;
} vp_section_t;

int vp_parse_section_header(const uint8_t *data, size_t size,
                            vp_section_t *section);
int vp_validate_section(vp_section_t *section);
void vp_section_summary(const vp_section_t *section);
void vp_section_cleanup(vp_section_t *section);
int vp_fill_section_data(vp_section_t *section, const uint8_t *payload,
                         size_t len);

#ifdef __cplusplus
}
#endif

#endif
