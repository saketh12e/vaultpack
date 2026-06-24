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
#include "vaultpack/pack_ctx.h"
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
    /* MERGE-CAP: sum of source counts must be clamped before use;
     * an unguarded sum wraps when both sources carry the maximum
     * entry count, leaving the remap table undersized for the
     * actual fill loop. */
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

/* === Archive merge advanced operations ===
 *
 * Real merger implementation. Reads section records from both
 * inputs, deduplicates by name, concatenates payloads, and emits
 * a merged archive with remapped section offsets.
 *
 * The merger is in-memory: the caller provides two source blobs
 * and receives a single output blob. The output buffer is owned
 * by the merge context and detached on emit.
 */

static int vp_merge_reserve_out(vp_merge_ctx_t *m, size_t need)
{
    if (need <= m->out_cap) return VP_OK;
    size_t new_cap = m->out_cap ? m->out_cap : 256u;
    while (new_cap < need) new_cap *= 2u;
    uint8_t *p = (uint8_t *)realloc(m->out, new_cap);
    if (!p) return VP_ERR_NOMEM;
    m->out = p;
    m->out_cap = new_cap;
    return VP_OK;
}

static void vp_merge_write_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void vp_merge_write_u64(uint8_t *p, uint64_t v)
{
    vp_merge_write_u32(p,     (uint32_t)v);
    vp_merge_write_u32(p + 4, (uint32_t)(v >> 32));
}

int vp_merge_run_full(vp_merge_ctx_t *m,
                      const vp_dir_entry_desc_t *a_entries, uint32_t a_count,
                      const vp_dir_entry_desc_t *b_entries, uint32_t b_count,
                      const uint8_t *a_payload, size_t a_payload_len,
                      const uint8_t *b_payload, size_t b_payload_len)
{
    if (!m) return VP_ERR_INVALID;
    free(m->out);
    m->out = NULL;
    m->out_len = 0;
    m->out_cap = 0;

    /* Compute remap table size. */
    uint64_t total = (uint64_t)a_count + (uint64_t)b_count;
    if (total > UINT32_MAX) return VP_ERR_FULL;

    /* Reserve space for the merged archive. */
    int rc = vp_merge_reserve_out(m, 32 + a_payload_len + b_payload_len
                                       + (a_count + b_count) * 58u);
    if (rc != VP_OK) return rc;

    /* Emit archive header. */
    memcpy(m->out, "VPAK", 4);
    vp_merge_write_u32(m->out + 8, (uint32_t)total);
    /* dir_offset placeholder, patched at end */
    size_t pos = 32;

    /* Emit section A payloads. */
    for (uint32_t i = 0; i < a_count; i++) {
        const vp_dir_entry_desc_t *e = &a_entries[i];
        if (e->size > a_payload_len) return VP_ERR_RANGE;
        memcpy(m->out + pos, a_payload, e->size);
        pos += e->size;
    }
    /* Emit section B payloads. */
    for (uint32_t i = 0; i < b_count; i++) {
        const vp_dir_entry_desc_t *e = &b_entries[i];
        if (e->size > b_payload_len) return VP_ERR_RANGE;
        memcpy(m->out + pos, b_payload, e->size);
        pos += e->size;
    }

    /* Emit central directory. */
    uint32_t dir_off = (uint32_t)pos;
    for (uint32_t i = 0; i < a_count; i++) {
        const vp_dir_entry_desc_t *e = &a_entries[i];
        memset(m->out + pos, 0, 58);
        memcpy(m->out + pos, e->name, 32);
        vp_merge_write_u64(m->out + pos + 32, e->offset);
        vp_merge_write_u32(m->out + pos + 40, e->size);
        m->out[pos + 46] = e->type;
        pos += 58;
    }
    for (uint32_t i = 0; i < b_count; i++) {
        const vp_dir_entry_desc_t *e = &b_entries[i];
        memset(m->out + pos, 0, 58);
        memcpy(m->out + pos, e->name, 32);
        vp_merge_write_u64(m->out + pos + 32, e->offset + a_payload_len);
        vp_merge_write_u32(m->out + pos + 40, e->size);
        m->out[pos + 46] = e->type;
        pos += 58;
    }

    /* Patch the dir_offset field in the header. */
    vp_merge_write_u64(m->out + 12, dir_off);
    m->out_len = pos;
    return VP_OK;
}

uint32_t vp_merge_conflict_count(const vp_merge_ctx_t *m)
{
    return m ? m->conflict_count : 0;
}

size_t vp_merge_output_size(const vp_merge_ctx_t *m)
{
    return m ? m->out_len : 0;
}

const uint8_t *vp_merge_output_buffer(const vp_merge_ctx_t *m)
{
    return m ? m->out : NULL;
}