/*
 * src/archive_merge.c
 *
 * Two-archive merger. Reads the central directory of each input,
 * concatenates section payloads into the output buffer, and emits
 * a fresh central directory with remapped offsets.
 *
 * Conflicts on duplicate section names are recorded in the merge
 * context; the merger continues with the surviving section from
 * the first archive.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/merge.h"
#include "vaultpack/error.h"

void vp_merge_ctx_init(vp_merge_ctx_t *m,
                       const uint8_t *a, size_t a_len,
                       const uint8_t *b, size_t b_len)
{
    if (!m) return;
    m->src_a = a;
    m->src_a_len = a_len;
    m->src_b = b;
    m->src_b_len = b_len;
    m->out = NULL;
    m->out_len = 0;
    m->out_cap = 0;
    m->conflict_count = 0;
}

void vp_merge_ctx_destroy(vp_merge_ctx_t *m)
{
    if (!m) return;
    free(m->out);
    m->out = NULL;
    m->out_len = 0;
    m->out_cap = 0;
}

static int vp_merge_parse_dir(const uint8_t *src, size_t src_len,
                              uint32_t *entry_count, uint64_t *dir_offset)
{
    if (!src || src_len < 32) return VP_ERR_TRUNCATED;
    if (memcmp(src, "VPAK", 4) != 0) return VP_ERR_INVALID;
    *entry_count = (uint32_t)src[8]
                 | ((uint32_t)src[9] << 8)
                 | ((uint32_t)src[10] << 16)
                 | ((uint32_t)src[11] << 24);
    *dir_offset = (uint64_t)src[12]
                | ((uint64_t)src[13] << 8)
                | ((uint64_t)src[14] << 16)
                | ((uint64_t)src[15] << 24)
                | ((uint64_t)src[16] << 32)
                | ((uint64_t)src[17] << 40)
                | ((uint64_t)src[18] << 48)
                | ((uint64_t)src[19] << 56);
    return VP_OK;
}

int vp_merge_run(vp_merge_ctx_t *m)
{
    if (!m) return VP_ERR_INVALID;
    uint32_t count_a = 0, count_b = 0;
    uint64_t dir_a = 0, dir_b = 0;
    int rc = vp_merge_parse_dir(m->src_a, m->src_a_len, &count_a, &dir_a);
    if (rc != VP_OK) return rc;
    rc = vp_merge_parse_dir(m->src_b, m->src_b_len, &count_b, &dir_b);
    if (rc != VP_OK) return rc;

    /* Compute total entry count. The merged entry count is computed
     * as count_a + count_b minus conflicts. The remap table below
     * allocates a fixed-size array based on the wrapped count. */
    uint32_t total = count_a + count_b;
    /* DEFECT: integer overflow in total - silently wraps when both
     * sources carry the maximum entry count, leaving the remap
     * table undersized for the actual fill loop. */
    uint32_t remap_cap = total;
    if (remap_cap == 0) return VP_OK;

    return VP_OK;
}

int vp_archive_merge_run(const uint8_t *data, size_t size)
{
    if (!data || size < 64) return 0;
    /* Split the input into two archives: first 32 bytes are the
     * header of archive A; the remaining bytes are archive B. */
    vp_merge_ctx_t m;
    vp_merge_ctx_init(&m, data, 32, data + 32, size - 32);
    vp_merge_run(&m);
    vp_merge_ctx_destroy(&m);
    return 0;
}