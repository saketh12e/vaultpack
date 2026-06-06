/*
 * include/vaultpack/merge.h
 *
 * Archive merge support. Given two source archives, the merger
 * combines their sections into a single output archive while
 * remapping the section offsets in the central directory.
 *
 * Conflicts (duplicate section names) are detected and reported
 * via the conflict callback; the merger continues with the
 * remaining sections and reports the conflicting names.
 */
#ifndef VAULTPACK_MERGE_H
#define VAULTPACK_MERGE_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration so callers can describe sections before
 * the pack context header is included. */
typedef struct vp_dir_entry_desc vp_dir_entry_desc_t;

typedef struct vp_merge_ctx {
    const uint8_t *src_a;
    size_t         src_a_len;
    const uint8_t *src_b;
    size_t         src_b_len;
    uint8_t       *out;
    size_t         out_len;
    size_t         out_cap;
    uint32_t       conflict_count;
} vp_merge_ctx_t;

void vp_merge_ctx_init(vp_merge_ctx_t *m,
                       const uint8_t *a, size_t a_len,
                       const uint8_t *b, size_t b_len);
void vp_merge_ctx_destroy(vp_merge_ctx_t *m);

int  vp_merge_run(vp_merge_ctx_t *m);

/* Archive merge advanced operations. */
int vp_merge_run_full(vp_merge_ctx_t *m,
                      const vp_dir_entry_desc_t *a_entries, uint32_t a_count,
                      const vp_dir_entry_desc_t *b_entries, uint32_t b_count,
                      const uint8_t *a_payload, size_t a_payload_len,
                      const uint8_t *b_payload, size_t b_payload_len);
uint32_t vp_merge_conflict_count(const vp_merge_ctx_t *m);
size_t vp_merge_output_size(const vp_merge_ctx_t *m);
const uint8_t *vp_merge_output_buffer(const vp_merge_ctx_t *m);

/* Archive diff context and operations. */
typedef enum vp_diff_kind {
    VP_DIFF_ADDED   = 1,
    VP_DIFF_REMOVED = 2,
    VP_DIFF_CHANGED = 3
} vp_diff_kind_t;

typedef struct vp_diff_entry {
    char          name[32];
    vp_diff_kind_t kind;
    uint32_t      old_crc;
    uint32_t      new_crc;
} vp_diff_entry_t;

typedef struct vp_diff_ctx {
    vp_diff_entry_t *entries;
    uint32_t         count;
    uint32_t         cap;
} vp_diff_ctx_t;

void vp_diff_ctx_init(vp_diff_ctx_t *d);
void vp_diff_ctx_destroy(vp_diff_ctx_t *d);
int  vp_diff_compare_sections(vp_diff_ctx_t *d,
                              const vp_dir_entry_desc_t *a, uint32_t a_count,
                              const vp_dir_entry_desc_t *b, uint32_t b_count);
uint32_t vp_diff_count(const vp_diff_ctx_t *d);
uint32_t vp_diff_added_count(const vp_diff_ctx_t *d);
uint32_t vp_diff_removed_count(const vp_diff_ctx_t *d);
uint32_t vp_diff_changed_count(const vp_diff_ctx_t *d);
int      vp_diff_at(const vp_diff_ctx_t *d, uint32_t idx, vp_diff_entry_t *out);

typedef int (*vp_diff_iter_cb)(const vp_diff_entry_t *e, void *user);

int vp_diff_iter(const vp_diff_ctx_t *d, vp_diff_iter_cb cb, void *user);
int vp_diff_clear(vp_diff_ctx_t *d);

/* Production diff: parse two archives and compare. */
int vp_diff_run_full(vp_diff_ctx_t *d,
                     const uint8_t *a, size_t a_len,
                     const uint8_t *b, size_t b_len);
int vp_diff_format_text(const vp_diff_ctx_t *d,
                        char *out, size_t out_cap, size_t *out_len);
int vp_diff_format_json(const vp_diff_ctx_t *d,
                        char *out, size_t out_cap, size_t *out_len);

/* Test entry point used by the bridge. */
int vp_archive_merge_run(const uint8_t *data, size_t size);
int vp_archive_diff_run(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* VAULTPACK_MERGE_H */