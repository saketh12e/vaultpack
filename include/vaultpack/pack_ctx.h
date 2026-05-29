/*
 * include/vaultpack/pack_ctx.h
 *
 * Streaming pack context. Allows an archive to be assembled from
 * chunked payload records rather than a single contiguous buffer.
 * The context owns the in-flight section payload, the chunk list,
 * and a deferred central directory.
 *
 * Lifecycle:
 *   vp_pack_ctx_init()    — fresh context with empty section list
 *   vp_pack_ctx_begin()   — start a new section (name, type)
 *   vp_pack_ctx_append()  — feed a payload chunk for the current section
 *   vp_pack_ctx_finish()  — finalize the current section, attach metadata
 *   vp_pack_ctx_emit()    — produce the final archive bytes (header + sections + dir)
 *   vp_pack_ctx_destroy() — release all resources
 *
 * Errors:
 *   - VP_ERR_FULL when the chunk budget for a section is exceeded
 *   - VP_ERR_TRUNCATED when the caller closes without finishing
 *   - VP_ERR_DUPLICATE when the same section name is started twice
 */
#ifndef VAULTPACK_PACK_CTX_H
#define VAULTPACK_PACK_CTX_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VP_PACK_MAX_NAME 64
#define VP_PACK_INITIAL_CHUNKS 8
#define VP_PACK_INITIAL_DIR_ENTRIES 16
#define VP_PACK_MAX_RAW_SIZE ((size_t)(1u << 26))

typedef struct vp_pack_chunk {
    uint8_t *data;
    size_t   len;
    size_t   cap;
} vp_pack_chunk_t;

typedef struct vp_pack_section {
    char     name[VP_PACK_MAX_NAME];
    uint8_t  type;
    uint32_t flags;
    uint32_t raw_size;
    uint32_t crc;
    vp_pack_chunk_t *chunks;
    uint32_t        chunk_count;
    uint32_t        chunk_cap;
    uint8_t         finished;
} vp_pack_section_t;

typedef struct vp_pack_ctx {
    vp_pack_section_t *sections;
    uint32_t          section_count;
    uint32_t          section_cap;
    int32_t           current;       /* index of in-flight section, -1 if none */
    uint8_t          *out_buf;
    size_t            out_len;
    size_t            out_cap;
    uint32_t          dir_offset;
    uint32_t          section_data_offset;
    uint32_t          chunk_budget;
    uint32_t          chunks_used;
    int               aborted;
} vp_pack_ctx_t;

void vp_pack_ctx_init(vp_pack_ctx_t *ctx);
void vp_pack_ctx_destroy(vp_pack_ctx_t *ctx);

int  vp_pack_ctx_begin(vp_pack_ctx_t *ctx,
                       const char *name, uint8_t type, uint32_t flags);

int  vp_pack_ctx_append(vp_pack_ctx_t *ctx, const uint8_t *data, size_t len);

int  vp_pack_ctx_finish(vp_pack_ctx_t *ctx);

int  vp_pack_ctx_emit(vp_pack_ctx_t *ctx, uint8_t **out, size_t *out_len);

/* Introspection helpers. */
uint32_t vp_pack_ctx_section_count(const vp_pack_ctx_t *ctx);
int      vp_pack_ctx_in_flight(const vp_pack_ctx_t *ctx);
const char *vp_pack_ctx_current_name(const vp_pack_ctx_t *ctx);
uint32_t vp_pack_ctx_current_size(const vp_pack_ctx_t *ctx);
int      vp_pack_ctx_section_at(const vp_pack_ctx_t *ctx, uint32_t idx,
                                vp_pack_section_t *out);
int      vp_pack_ctx_abort_current(vp_pack_ctx_t *ctx);
int  vp_pack_ctx_find(const vp_pack_ctx_t *ctx, const char *name);
uint64_t vp_pack_ctx_total_size(const vp_pack_ctx_t *ctx);

/* Checkpoint / rewind for incremental pack. */
typedef struct vp_pack_checkpoint {
    uint32_t section_count;
    int32_t  current;
    uint32_t section_data_offset;
} vp_pack_checkpoint_t;

int  vp_pack_ctx_checkpoint(const vp_pack_ctx_t *ctx, vp_pack_checkpoint_t *out);
int  vp_pack_ctx_rewind(vp_pack_ctx_t *ctx, const vp_pack_checkpoint_t *cp);
int  vp_pack_ctx_truncate_current(vp_pack_ctx_t *ctx, size_t new_total);
int  vp_pack_ctx_set_chunk_budget(vp_pack_ctx_t *ctx, uint32_t budget);
uint32_t vp_pack_ctx_chunk_budget(const vp_pack_ctx_t *ctx);
uint32_t vp_pack_ctx_chunks_used(const vp_pack_ctx_t *ctx);
int  vp_pack_ctx_validate(const vp_pack_ctx_t *ctx);
int  vp_pack_ctx_mark_finished_all(vp_pack_ctx_t *ctx);
int  vp_pack_ctx_recompute_crc(vp_pack_ctx_t *ctx, uint32_t idx);

/* Test entry point used by the bridge. */
int vp_pack_ctx_run(const uint8_t *data, size_t size);

/* Auxiliary structures used by the chunk assembler and dir emitter. */
typedef struct vp_chunk_assembler {
    uint8_t *data;
    size_t   cap;
    size_t   total;
} vp_chunk_assembler_t;

int  vp_chunk_assembler_init(vp_chunk_assembler_t *a);
int  vp_chunk_assembler_append(vp_chunk_assembler_t *a,
                               const uint8_t *data, size_t len);
int  vp_chunk_assembler_finalize(vp_chunk_assembler_t *a,
                                 uint8_t **out, size_t *out_len);
void vp_chunk_assembler_destroy(vp_chunk_assembler_t *a);

/* Chunk assembler advanced operations. */
int vp_chunk_assembler_reserve(vp_chunk_assembler_t *a, size_t cap);
int vp_chunk_assembler_seek(vp_chunk_assembler_t *a, size_t pos);
int vp_chunk_assembler_write_at(vp_chunk_assembler_t *a, size_t pos,
                                const uint8_t *data, size_t len);
size_t vp_chunk_assembler_remaining(const vp_chunk_assembler_t *a);
size_t vp_chunk_assembler_capacity(const vp_chunk_assembler_t *a);
size_t vp_chunk_assembler_size(const vp_chunk_assembler_t *a);
int vp_chunk_assembler_reset(vp_chunk_assembler_t *a);
int vp_chunk_assembler_clone(const vp_chunk_assembler_t *src,
                             vp_chunk_assembler_t *dst);
int vp_chunk_assembler_validate(const vp_chunk_assembler_t *a);

typedef struct vp_dir_entry_desc {
    char     name[32];
    uint64_t offset;
    uint32_t size;
    uint8_t  type;
} vp_dir_entry_desc_t;

typedef struct vp_dir_emit {
    vp_dir_entry_desc_t *entries;
    uint32_t             count;
    uint32_t             cap;
} vp_dir_emit_t;

void vp_dir_emit_init(vp_dir_emit_t *e);
int  vp_dir_emit_append(vp_dir_emit_t *e, const char *name,
                        uint64_t offset, uint32_t size, uint8_t type);
size_t vp_dir_emit_size(const vp_dir_emit_t *e);
int  vp_dir_emit_serialize(const vp_dir_emit_t *e,
                           uint8_t *out, size_t out_cap, size_t *out_len);
void vp_dir_emit_destroy(vp_dir_emit_t *e);

/* Directory emitter advanced operations. */
uint32_t vp_dir_emit_count(const vp_dir_emit_t *e);
uint32_t vp_dir_emit_capacity(const vp_dir_emit_t *e);
int vp_dir_emit_lookup(const vp_dir_emit_t *e, const char *name,
                       vp_dir_entry_desc_t *out);
int vp_dir_emit_at(const vp_dir_emit_t *e, uint32_t idx,
                   vp_dir_entry_desc_t *out);
int vp_dir_emit_remove(vp_dir_emit_t *e, const char *name);
int vp_dir_emit_update(vp_dir_emit_t *e, const char *name,
                       uint64_t offset, uint32_t size, uint8_t type);
int vp_dir_emit_clear(vp_dir_emit_t *e);
int vp_dir_emit_reserve(vp_dir_emit_t *e, uint32_t cap);

typedef int (*vp_dir_emit_iter_cb)(const vp_dir_entry_desc_t *e, void *user);

int vp_dir_emit_iter(const vp_dir_emit_t *e, vp_dir_emit_iter_cb cb, void *user);
int vp_dir_emit_clone(const vp_dir_emit_t *src, vp_dir_emit_t *dst);
uint64_t vp_dir_emit_total_size(const vp_dir_emit_t *e);
int vp_dir_emit_sort_by_name(vp_dir_emit_t *e);

/* Abort / rollback helpers. */
int  vp_pack_abort_section(vp_pack_section_t *s);
int  vp_pack_abort_check_duplicate(vp_pack_ctx_t *ctx, const char *name);
int  vp_pack_abort_check_crc(vp_pack_section_t *s, uint32_t expected);

/* Abort advanced operations. */
int vp_pack_section_clear(vp_pack_section_t *s);
int vp_pack_section_is_finished(const vp_pack_section_t *s);
uint32_t vp_pack_section_chunk_count(const vp_pack_section_t *s);
uint32_t vp_pack_section_raw_size(const vp_pack_section_t *s);
uint32_t vp_pack_section_crc(const vp_pack_section_t *s);
uint8_t vp_pack_section_type(const vp_pack_section_t *s);
uint32_t vp_pack_section_flags(const vp_pack_section_t *s);
const char *vp_pack_section_name(const vp_pack_section_t *s);
int vp_pack_section_set_type(vp_pack_section_t *s, uint8_t type);
int vp_pack_section_set_flags(vp_pack_section_t *s, uint32_t flags);
int vp_pack_section_validate(const vp_pack_section_t *s);
int vp_pack_abort_all(vp_pack_ctx_t *ctx);
int vp_pack_abort_above(vp_pack_ctx_t *ctx, uint32_t threshold);

#ifdef __cplusplus
}
#endif

#endif /* VAULTPACK_PACK_CTX_H */