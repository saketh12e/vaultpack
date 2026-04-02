/*
 * include/vaultpack/writer.h
 *
 * In-memory archive writer. The writer accumulates sections, metadata
 * blocks, and a flat directory; on emit, it serializes the directory
 * header, the central directory, and the metadata blocks into a single
 * byte buffer.
 *
 * Section names must be unique within an archive; duplicate names are
 * rejected at add-time. The writer does not write to disk directly —
 * callers emit to memory and persist as they see fit.
 */
#ifndef VAULTPACK_WRITER_H
#define VAULTPACK_WRITER_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/buffer.h"
#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VP_WRITER_MAX_NAME     32
#define VP_WRITER_MAX_SECTIONS 1024

typedef struct vp_writer_meta_entry {
    char key[24];
    char value[40];
} vp_writer_meta_entry_t;

typedef struct vp_writer_meta_block {
    uint8_t                   tag[4];
    vp_writer_meta_entry_t   *entries;
    uint32_t                  entry_count;
    uint32_t                  entry_cap;
    uint64_t                  block_offset;
} vp_writer_meta_block_t;

typedef struct vp_writer_section {
    char     name[VP_WRITER_MAX_NAME];
    uint8_t  type;
    uint8_t  flags;
    uint32_t checksum;
    uint8_t *payload;          /* owned by section if owns=1 */
    size_t   payload_len;
    int      owns;
    uint64_t payload_offset;   /* filled at emit time */
    uint32_t meta_count;       /* trailing metadata entry count */
    struct vp_writer_meta_block *meta_block;  /* points into writer->meta[] */
} vp_writer_section_t;

typedef struct vp_writer {
    vp_buffer_t           out;
    vp_writer_section_t  *sections;
    uint32_t              section_count;
    uint32_t              section_cap;
    vp_writer_meta_block_t *meta;
    uint32_t              meta_count;
    uint32_t              meta_cap;
    uint16_t              flags;
    uint16_t              index_width;
    uint16_t              index_depth;
    /* Aliased heap copy of the most-recently-added payload, used by
     * rollback and destroyed in destroy. Owned when non-NULL. */
    uint8_t              *last_payload;
    size_t                last_payload_len;
} vp_writer_t;

int  vp_writer_init(vp_writer_t *w);
void vp_writer_destroy(vp_writer_t *w);
void vp_writer_reset(vp_writer_t *w);

int  vp_writer_set_flags(vp_writer_t *w, uint16_t flags);
int  vp_writer_set_index_dims(vp_writer_t *w, uint16_t width, uint16_t depth);

int  vp_writer_add_section(vp_writer_t *w,
                           const char *name,
                           uint8_t type,
                           const void *payload, size_t payload_len);
int  vp_writer_set_section_flags(vp_writer_t *w, const char *name, uint16_t flags);
int  vp_writer_add_metadata(vp_writer_t *w, const uint8_t tag[4],
                           const vp_writer_meta_entry_t *entries,
                           uint32_t count);

int  vp_writer_emit(vp_writer_t *w, uint8_t **out_bytes, size_t *out_len);

const vp_writer_section_t *vp_writer_section(const vp_writer_t *w,
                                               const char *name);

/* Internal: rollback the most-recently-added pending section. */
void vp_writer_rollback_section(vp_writer_t *w);

#ifdef __cplusplus
}
#endif

#endif
