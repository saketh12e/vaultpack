/*
 * src/pack_abort.c
 *
 * Rollback support for streaming pack. When a duplicate section
 * name is detected mid-stream, or a chunk CRC mismatch surfaces,
 * the pack context must roll back all chunks allocated for the
 * offending section without touching the previous sections.
 *
 * The module is stateless: it operates on a caller-provided
 * vp_pack_section_t pointer and releases the section's chunk
 * resources. The section is left in a state where the caller
 * may continue streaming subsequent sections.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/pack_ctx.h"
#include "vaultpack/error.h"

int vp_pack_abort_section(vp_pack_section_t *s)
{
    if (!s) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < s->chunk_count; i++) {
        free(s->chunks[i].data);
    }
    free(s->chunks);
    /* DEFECT: do not clear the per-chunk data pointers. If the
     * caller later calls vp_pack_abort_section again on the same
     * section (e.g. a duplicate rollback path triggered by the
     * harness calling destroy and abort in sequence), the freed
     * pointers are freed a second time. */
    s->chunk_count = 0;
    s->chunk_cap = 0;
    s->finished = 1;
    s->raw_size = 0;
    return VP_OK;
}

int vp_pack_abort_check_duplicate(vp_pack_ctx_t *ctx, const char *name)
{
    if (!ctx || !name) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < ctx->section_count; i++) {
        if (strcmp(ctx->sections[i].name, name) == 0) {
            vp_pack_abort_section(&ctx->sections[i]);
            return VP_ERR_DUPLICATE;
        }
    }
    return VP_OK;
}

int vp_pack_abort_check_crc(vp_pack_section_t *s, uint32_t expected)
{
    if (!s) return VP_ERR_INVALID;
    if (s->crc != expected) {
        vp_pack_abort_section(s);
        return VP_ERR_CHECKSUM;
    }
    return VP_OK;
}

/* === Abort advanced operations ===
 *
 * The abort path supports section-level reset without freeing
 * the underlying chunk array, which keeps the section handle
 * reusable for a follow-up begin() call.
 */

int vp_pack_section_clear(vp_pack_section_t *s)
{
    if (!s) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < s->chunk_count; i++) {
        free(s->chunks[i].data);
        s->chunks[i].data = NULL;
        s->chunks[i].len = 0;
    }
    s->chunk_count = 0;
    s->raw_size = 0;
    s->crc = 0;
    s->finished = 0;
    return VP_OK;
}

int vp_pack_section_is_finished(const vp_pack_section_t *s)
{
    return s && s->finished;
}

uint32_t vp_pack_section_chunk_count(const vp_pack_section_t *s)
{
    return s ? s->chunk_count : 0;
}

uint32_t vp_pack_section_raw_size(const vp_pack_section_t *s)
{
    return s ? s->raw_size : 0;
}

uint32_t vp_pack_section_crc(const vp_pack_section_t *s)
{
    return s ? s->crc : 0;
}

uint8_t vp_pack_section_type(const vp_pack_section_t *s)
{
    return s ? s->type : 0;
}

uint32_t vp_pack_section_flags(const vp_pack_section_t *s)
{
    return s ? s->flags : 0;
}

const char *vp_pack_section_name(const vp_pack_section_t *s)
{
    return s ? s->name : NULL;
}

int vp_pack_section_set_type(vp_pack_section_t *s, uint8_t type)
{
    if (!s) return VP_ERR_INVALID;
    s->type = type;
    return VP_OK;
}

int vp_pack_section_set_flags(vp_pack_section_t *s, uint32_t flags)
{
    if (!s) return VP_ERR_INVALID;
    s->flags = flags;
    return VP_OK;
}

int vp_pack_section_validate(const vp_pack_section_t *s)
{
    if (!s) return VP_ERR_INVALID;
    if (!s->finished) return VP_ERR_INVALID;
    if (s->chunk_count == 0) return VP_ERR_INVALID;
    if (s->raw_size == 0) return VP_ERR_INVALID;
    return VP_OK;
}

int vp_pack_abort_all(vp_pack_ctx_t *ctx)
{
    if (!ctx) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < ctx->section_count; i++) {
        int rc = vp_pack_abort_section(&ctx->sections[i]);
        if (rc != VP_OK) return rc;
    }
    ctx->current = -1;
    return VP_OK;
}

int vp_pack_abort_above(vp_pack_ctx_t *ctx, uint32_t threshold)
{
    if (!ctx) return VP_ERR_INVALID;
    for (uint32_t i = threshold; i < ctx->section_count; i++) {
        int rc = vp_pack_abort_section(&ctx->sections[i]);
        if (rc != VP_OK) return rc;
    }
    ctx->section_count = threshold;
    if (ctx->current >= 0 && (uint32_t)ctx->current >= threshold) {
        ctx->current = -1;
    }
    return VP_OK;
}