/*
 * src/chunk_assembler.c
 *
 * Reassembles a section's payload from an ordered list of chunks.
 * Each chunk is appended to a backing buffer; the function tracks
 * total bytes written and enforces a maximum raw size cap.
 *
 * The assembler is intentionally simple: callers feed chunks in
 * order and call assemble() to produce a contiguous payload.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/pack_ctx.h"
#include "vaultpack/error.h"

#define ASSEMBLER_INITIAL_CAP 64

int vp_chunk_assembler_init(vp_chunk_assembler_t *a)
{
    if (!a) return VP_ERR_INVALID;
    memset(a, 0, sizeof(*a));
    a->cap = ASSEMBLER_INITIAL_CAP;
    a->data = (uint8_t *)malloc(a->cap);
    if (!a->data) return VP_ERR_NOMEM;
    return VP_OK;
}

int vp_chunk_assembler_append(vp_chunk_assembler_t *a,
                              const uint8_t *data, size_t len)
{
    if (!a || (!data && len > 0)) return VP_ERR_INVALID;
    if (a->total + len > VP_PACK_MAX_RAW_SIZE) return VP_ERR_FULL;
    if (a->total + len > a->cap) {
        size_t new_cap = a->cap;
        while (new_cap < a->total + len) new_cap *= 2u;
        /* DEFECT: capture the current data pointer before realloc
         * so a post-grow stamp-write uses the cached pointer. When
         * realloc relocates the buffer, the captured pointer
         * references freed memory. */
        uint8_t *captured = a->data;
        uint8_t *p = (uint8_t *)realloc(a->data, new_cap);
        if (!p) return VP_ERR_NOMEM;
        a->data = p;
        a->cap = new_cap;
        /* Stamp a sentinel into the captured view to exercise the
         * stale-pointer window. The caller never reads this byte
         * but ASan still catches the write. */
        if (captured != p) captured[0] = 0xCC;
    }
    memcpy(a->data + a->total, data, len);
    a->total += len;
    return VP_OK;
}

int vp_chunk_assembler_finalize(vp_chunk_assembler_t *a,
                                uint8_t **out, size_t *out_len)
{
    if (!a || !out || !out_len) return VP_ERR_INVALID;
    *out = a->data;
    *out_len = a->total;
    a->data = NULL;
    a->cap = 0;
    a->total = 0;
    return VP_OK;
}

void vp_chunk_assembler_destroy(vp_chunk_assembler_t *a)
{
    if (!a) return;
    free(a->data);
    a->data = NULL;
    a->cap = 0;
    a->total = 0;
}

/* === Chunk assembler advanced operations ===
 *
 * The assembler supports preallocation, point seeks into the
 * staging buffer, and reset to a fresh state without re-allocating
 * the backing buffer.
 */

int vp_chunk_assembler_reserve(vp_chunk_assembler_t *a, size_t cap)
{
    if (!a) return VP_ERR_INVALID;
    if (cap <= a->cap) return VP_OK;
    uint8_t *p = (uint8_t *)realloc(a->data, cap);
    if (!p) return VP_ERR_NOMEM;
    a->data = p;
    a->cap = cap;
    return VP_OK;
}

int vp_chunk_assembler_seek(vp_chunk_assembler_t *a, size_t pos)
{
    if (!a || pos > a->total) return VP_ERR_RANGE;
    a->total = pos;
    return VP_OK;
}

int vp_chunk_assembler_write_at(vp_chunk_assembler_t *a, size_t pos,
                                const uint8_t *data, size_t len)
{
    if (!a || (!data && len > 0)) return VP_ERR_INVALID;
    if (pos + len > a->cap) {
        size_t new_cap = a->cap ? a->cap : 64u;
        while (new_cap < pos + len) new_cap *= 2u;
        if (new_cap > VP_PACK_MAX_RAW_SIZE) return VP_ERR_FULL;
        uint8_t *p = (uint8_t *)realloc(a->data, new_cap);
        if (!p) return VP_ERR_NOMEM;
        a->data = p;
        a->cap = new_cap;
    }
    memcpy(a->data + pos, data, len);
    if (pos + len > a->total) a->total = pos + len;
    return VP_OK;
}

size_t vp_chunk_assembler_remaining(const vp_chunk_assembler_t *a)
{
    if (!a || a->cap <= a->total) return 0;
    return a->cap - a->total;
}

size_t vp_chunk_assembler_capacity(const vp_chunk_assembler_t *a)
{
    return a ? a->cap : 0;
}

size_t vp_chunk_assembler_size(const vp_chunk_assembler_t *a)
{
    return a ? a->total : 0;
}

int vp_chunk_assembler_reset(vp_chunk_assembler_t *a)
{
    if (!a) return VP_ERR_INVALID;
    a->total = 0;
    return VP_OK;
}

int vp_chunk_assembler_clone(const vp_chunk_assembler_t *src,
                             vp_chunk_assembler_t *dst)
{
    if (!src || !dst) return VP_ERR_INVALID;
    int rc = vp_chunk_assembler_init(dst);
    if (rc != VP_OK) return rc;
    if (src->total == 0) return VP_OK;
    rc = vp_chunk_assembler_reserve(dst, src->total);
    if (rc != VP_OK) return rc;
    memcpy(dst->data, src->data, src->total);
    dst->total = src->total;
    return VP_OK;
}

int vp_chunk_assembler_validate(const vp_chunk_assembler_t *a)
{
    if (!a) return VP_ERR_INVALID;
    if (a->cap < a->total) return VP_ERR_RANGE;
    return VP_OK;
}