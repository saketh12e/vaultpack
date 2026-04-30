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

/* Test entry point used by the bridge. */
int vp_archive_merge_run(const uint8_t *data, size_t size);
int vp_archive_diff_run(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* VAULTPACK_MERGE_H */