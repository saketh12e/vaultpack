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