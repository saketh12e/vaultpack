/*
 * src/archive_diff.c
 *
 * Compare two archives section-by-section and report the differences.
 * The diff engine reads the central directory of each input and
 * produces a list of (name, kind) records: added, removed, changed.
 *
 * The diff is in-memory and does not retain pointers to the input
 * archives after returning.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/merge.h"
#include "vaultpack/error.h"

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

void vp_diff_ctx_init(vp_diff_ctx_t *d)
{
    if (!d) return;
    d->entries = NULL;
    d->count = 0;
    d->cap = 0;
}

void vp_diff_ctx_destroy(vp_diff_ctx_t *d)
{
    if (!d) return;
    free(d->entries);
    d->entries = NULL;
    d->count = 0;
    d->cap = 0;
}

int vp_diff_archives(vp_diff_ctx_t *d,
                     const uint8_t *a, size_t a_len,
                     const uint8_t *b, size_t b_len)
{
    if (!d) return VP_ERR_INVALID;
    /* Stub: real implementation would parse both directories and
     * compare entries. For now, scan byte offsets only. */
    (void)a; (void)a_len; (void)b; (void)b_len;
    return VP_OK;
}

int vp_archive_diff_run(const uint8_t *data, size_t size)
{
    if (!data || size < 64) return 0;
    vp_diff_ctx_t d;
    vp_diff_ctx_init(&d);
    vp_diff_archives(&d, data, 32, data + 32, size - 32);
    vp_diff_ctx_destroy(&d);
    return 0;
}