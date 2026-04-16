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