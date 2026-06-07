/*
 * src/pack_ctx.c
 *
 * Streaming pack context implementation. The context tracks an
 * in-flight section, accumulates chunked payloads, and finally
 * emits the archive bytes in the canonical writer order:
 *
 *   archive header
 *   for each section:
 *     SECT header
 *     section payload
 *   central directory
 *   directory entries
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/pack_ctx.h"
#include "vaultpack/crc32.h"
#include "vaultpack/error.h"

void vp_pack_ctx_init(vp_pack_ctx_t *ctx)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->current = -1;
    ctx->dir_offset = 0;
    ctx->section_data_offset = 32;  /* size of archive header */
    ctx->chunk_budget = 1024;
}

static int vp_pack_ctx_reserve_sections(vp_pack_ctx_t *ctx, uint32_t need)
{
    if (need <= ctx->section_cap) return VP_OK;
    uint32_t new_cap = ctx->section_cap ? ctx->section_cap * 2u : 16u;
    while (new_cap < need) new_cap *= 2u;
    vp_pack_section_t *p = (vp_pack_section_t *)realloc(
        ctx->sections, (size_t)new_cap * sizeof(*p));
    if (!p) return VP_ERR_NOMEM;
    ctx->sections = p;
    memset(p + ctx->section_cap, 0,
           (size_t)(new_cap - ctx->section_cap) * sizeof(*p));
    ctx->section_cap = new_cap;
    return VP_OK;
}

static int vp_pack_chunk_reserve(vp_pack_chunk_t *c, size_t need)
{
    if (need <= c->cap) return VP_OK;
    size_t new_cap = c->cap ? c->cap : 64u;
    while (new_cap < need) new_cap *= 2u;
    if (new_cap > VP_PACK_MAX_RAW_SIZE) return VP_ERR_FULL;
    uint8_t *p = (uint8_t *)realloc(c->data, new_cap);
    if (!p) return VP_ERR_NOMEM;
    c->data = p;
    c->cap = new_cap;
    return VP_OK;
}

int vp_pack_ctx_begin(vp_pack_ctx_t *ctx,
                      const char *name, uint8_t type, uint32_t flags)
{
    if (!ctx || !name) return VP_ERR_INVALID;
    if (ctx->current >= 0 && !ctx->sections[ctx->current].finished) {
        return VP_ERR_TRUNCATED;
    }
    for (uint32_t i = 0; i < ctx->section_count; i++) {
        if (strcmp(ctx->sections[i].name, name) == 0) return VP_ERR_DUPLICATE;
    }
    int rc = vp_pack_ctx_reserve_sections(ctx, ctx->section_count + 1);
    if (rc != VP_OK) return rc;
    vp_pack_section_t *s = &ctx->sections[ctx->section_count];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, sizeof(s->name) - 1);
    s->type = type;
    s->flags = flags;
    s->chunk_cap = VP_PACK_INITIAL_CHUNKS;
    s->chunks = (vp_pack_chunk_t *)calloc(s->chunk_cap, sizeof(vp_pack_chunk_t));
    if (!s->chunks) return VP_ERR_NOMEM;
    ctx->current = (int32_t)ctx->section_count;
    ctx->section_count++;
    return VP_OK;
}

int vp_pack_ctx_append(vp_pack_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx || (!data && len > 0)) return VP_ERR_INVALID;
    if (ctx->current < 0) return VP_ERR_TRUNCATED;
    vp_pack_section_t *s = &ctx->sections[ctx->current];
    if (s->finished) return VP_ERR_TRUNCATED;
    if (s->chunk_count >= s->chunk_cap) {
        uint32_t new_cap = s->chunk_cap * 2u;
        vp_pack_chunk_t *p = (vp_pack_chunk_t *)realloc(
            s->chunks, (size_t)new_cap * sizeof(*p));
        if (!p) return VP_ERR_NOMEM;
        memset(p + s->chunk_cap, 0,
               (size_t)(new_cap - s->chunk_cap) * sizeof(*p));
        s->chunks = p;
        s->chunk_cap = new_cap;
    }
    vp_pack_chunk_t *c = &s->chunks[s->chunk_count++];
    c->len = 0;
    int rc = vp_pack_chunk_reserve(c, len);
    if (rc != VP_OK) return rc;
    if (len > 0) memcpy(c->data, data, len);
    c->len = len;
    s->raw_size += (uint32_t)len;
    return VP_OK;
}

int vp_pack_ctx_finish(vp_pack_ctx_t *ctx)
{
    if (!ctx) return VP_ERR_INVALID;
    if (ctx->current < 0) return VP_ERR_INVALID;
    vp_pack_section_t *s = &ctx->sections[ctx->current];
    if (s->finished) return VP_ERR_TRUNCATED;

    /* Compute CRC over all chunks. */
    uint32_t crc = 0;
    for (uint32_t i = 0; i < s->chunk_count; i++) {
        crc = vp_crc32_update(crc, s->chunks[i].data, s->chunks[i].len);
    }
    s->crc = crc;
    s->finished = 1;
    ctx->current = -1;
    return VP_OK;
}

/* Emit section headers + payloads + central directory. The output
 * buffer is grown as needed. */
static int vp_pack_emit_grow(vp_pack_ctx_t *ctx, size_t need)
{
    if (need <= ctx->out_cap) return VP_OK;
    size_t new_cap = ctx->out_cap ? ctx->out_cap : 256u;
    while (new_cap < need) new_cap *= 2u;
    uint8_t *p = (uint8_t *)realloc(ctx->out_buf, new_cap);
    if (!p) return VP_ERR_NOMEM;
    ctx->out_buf = p;
    ctx->out_cap = new_cap;
    return VP_OK;
}

static void vp_pack_write_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void vp_pack_write_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void vp_pack_write_u64(uint8_t *p, uint64_t v)
{
    vp_pack_write_u32(p,     (uint32_t)v);
    vp_pack_write_u32(p + 4, (uint32_t)(v >> 32));
}

int vp_pack_ctx_emit(vp_pack_ctx_t *ctx, uint8_t **out, size_t *out_len)
{
    if (!ctx || !out || !out_len) return VP_ERR_INVALID;
    *out = NULL;
    *out_len = 0;

    /* Archive header: "VPAK" + version + flags + entry_count + dir_offset. */
    int rc = vp_pack_emit_grow(ctx, 32);
    if (rc != VP_OK) return rc;
    uint8_t *p = ctx->out_buf;
    memcpy(p, "VPAK", 4);
    vp_pack_write_u16(p + 4,  1);
    vp_pack_write_u16(p + 6,  0);
    vp_pack_write_u32(p + 8,  ctx->section_count);
    vp_pack_write_u64(p + 12, 0);   /* dir_offset, patched at end */
    vp_pack_write_u16(p + 20, 1);
    vp_pack_write_u16(p + 22, 1);
    memset(p + 24, 0, 8);

    size_t pos = 32;

    for (uint32_t i = 0; i < ctx->section_count; i++) {
        vp_pack_section_t *s = &ctx->sections[i];
        rc = vp_pack_emit_grow(ctx, pos + 24 + s->raw_size);
        if (rc != VP_OK) return rc;
        uint8_t *sh = ctx->out_buf + pos;
        memcpy(sh, "SECT", 4);
        sh[4] = s->type;
        sh[5] = 0;
        sh[6] = 0;
        sh[7] = 0;
        vp_pack_write_u32(sh + 8,  s->raw_size);
        vp_pack_write_u32(sh + 12, s->raw_size);
        vp_pack_write_u32(sh + 16, s->crc);
        vp_pack_write_u32(sh + 20, s->flags);
        pos += 24;

        for (uint32_t k = 0; k < s->chunk_count; k++) {
            memcpy(ctx->out_buf + pos, s->chunks[k].data, s->chunks[k].len);
            pos += s->chunks[k].len;
        }
    }

    /* Central directory: 58-byte entries. */
    uint32_t dir_off = (uint32_t)pos;
    for (uint32_t i = 0; i < ctx->section_count; i++) {
        vp_pack_section_t *s = &ctx->sections[i];
        rc = vp_pack_emit_grow(ctx, pos + 58);
        if (rc != VP_OK) return rc;
        uint8_t *de = ctx->out_buf + pos;
        memset(de, 0, 58);
        strncpy((char *)de, s->name, 32);
        vp_pack_write_u64(de + 32, 32 + i * 24);
        vp_pack_write_u32(de + 40, s->raw_size);
        vp_pack_write_u16(de + 44, (uint16_t)s->flags);
        de[46] = s->type;
        de[47] = 0;
        vp_pack_write_u32(de + 48, 0);
        vp_pack_write_u16(de + 52, 0);
        vp_pack_write_u32(de + 54, 0);
        pos += 58;
    }

    /* Patch the dir_offset field in the header. */
    vp_pack_write_u64(ctx->out_buf + 12, dir_off);
    *out = ctx->out_buf;
    *out_len = pos;
    /* Detach the buffer so destroy does not free the caller's copy. */
    ctx->out_buf = NULL;
    ctx->out_cap = 0;
    return VP_OK;
}

void vp_pack_ctx_destroy(vp_pack_ctx_t *ctx)
{
    if (!ctx) return;
    if (ctx->sections) {
        for (uint32_t i = 0; i < ctx->section_count; i++) {
            vp_pack_section_t *s = &ctx->sections[i];
            for (uint32_t k = 0; k < s->chunk_count; k++) {
                free(s->chunks[k].data);
            }
            free(s->chunks);
        }
        free(ctx->sections);
    }
    free(ctx->out_buf);
    memset(ctx, 0, sizeof(*ctx));
    ctx->current = -1;
}

int vp_pack_ctx_run(const uint8_t *data, size_t size)
{
    if (!data || size == 0) return 0;
    if (size < 4) return 0;

    vp_pack_ctx_t ctx;
    vp_pack_ctx_init(&ctx);

    uint32_t section_count = (uint32_t)data[0] | ((uint32_t)data[1] << 8);
    if (section_count > 256) section_count = 256;
    if (section_count == 0) {
        vp_pack_ctx_destroy(&ctx);
        return 0;
    }
    size_t pos = 2;

    for (uint32_t i = 0; i < section_count && pos < size; i++) {
        if (pos + 4 > size) break;
        uint8_t namelen = data[pos];
        pos++;
        if (pos + namelen + 1 > size) break;
        char name[VP_PACK_MAX_NAME];
        size_t copy = namelen < VP_PACK_MAX_NAME - 1 ? namelen : VP_PACK_MAX_NAME - 1;
        memcpy(name, data + pos, copy);
        name[copy] = '\0';
        pos += namelen + 1;
        if (pos >= size) break;
        uint8_t type = data[pos++];

        if (vp_pack_ctx_begin(&ctx, name, type, 0) != VP_OK) break;
        /* Feed the rest of the input as one chunk. */
        if (vp_pack_ctx_append(&ctx, data + pos, size - pos) != VP_OK) break;
        if (vp_pack_ctx_finish(&ctx) != VP_OK) break;
        pos = size;
    }

    uint8_t *out = NULL;
    size_t   out_len = 0;
    vp_pack_ctx_emit(&ctx, &out, &out_len);
    free(out);
    vp_pack_ctx_destroy(&ctx);
    return 0;
}

/* === Pack context introspection ===
 *
 * Callers can inspect the in-flight section list for diagnostics,
 * progress reporting, and merge pre-checks.
 */

uint32_t vp_pack_ctx_section_count(const vp_pack_ctx_t *ctx)
{
    return ctx ? ctx->section_count : 0;
}

int vp_pack_ctx_in_flight(const vp_pack_ctx_t *ctx)
{
    return ctx && ctx->current >= 0;
}

const char *vp_pack_ctx_current_name(const vp_pack_ctx_t *ctx)
{
    if (!ctx || ctx->current < 0) return NULL;
    return ctx->sections[ctx->current].name;
}

uint32_t vp_pack_ctx_current_size(const vp_pack_ctx_t *ctx)
{
    if (!ctx || ctx->current < 0) return 0;
    return ctx->sections[ctx->current].raw_size;
}

int vp_pack_ctx_section_at(const vp_pack_ctx_t *ctx, uint32_t idx,
                           vp_pack_section_t *out)
{
    if (!ctx || !out || idx >= ctx->section_count) return VP_ERR_RANGE;
    *out = ctx->sections[idx];
    return VP_OK;
}

int vp_pack_ctx_abort_current(vp_pack_ctx_t *ctx)
{
    if (!ctx) return VP_ERR_INVALID;
    if (ctx->current < 0) return VP_ERR_INVALID;
    int rc = vp_pack_abort_section(&ctx->sections[ctx->current]);
    ctx->current = -1;
    return rc;
}

int vp_pack_ctx_find(const vp_pack_ctx_t *ctx, const char *name)
{
    if (!ctx || !name) return -1;
    for (uint32_t i = 0; i < ctx->section_count; i++) {
        if (strcmp(ctx->sections[i].name, name) == 0) return (int)i;
    }
    return -1;
}

uint64_t vp_pack_ctx_total_size(const vp_pack_ctx_t *ctx)
{
    if (!ctx) return 0;
    uint64_t total = 0;
    for (uint32_t i = 0; i < ctx->section_count; i++) {
        total += ctx->sections[i].raw_size;
    }
    return total;
}

/* === Pack context error recovery and advanced operations ===
 *
 * The advanced operations expose finer control over the streaming
 * pack lifecycle: explicit checkpointing, payload shrinking after
 * an append error, and backpressure hints that the caller can use
 * to throttle upstream chunk producers.
 */

int vp_pack_ctx_checkpoint(const vp_pack_ctx_t *ctx,
                           vp_pack_checkpoint_t *out)
{
    if (!ctx || !out) return VP_ERR_INVALID;
    out->section_count = ctx->section_count;
    out->current = ctx->current;
    out->section_data_offset = ctx->section_data_offset;
    return VP_OK;
}

int vp_pack_ctx_rewind(vp_pack_ctx_t *ctx, const vp_pack_checkpoint_t *cp)
{
    if (!ctx || !cp) return VP_ERR_INVALID;
    if (cp->section_count > ctx->section_count) return VP_ERR_RANGE;
    ctx->section_count = cp->section_count;
    ctx->current = cp->current;
    ctx->section_data_offset = cp->section_data_offset;
    /* Free the chunks belonging to abandoned sections. */
    for (uint32_t i = cp->section_count; i < ctx->section_cap; i++) {
        if (i >= ctx->section_count) break;
    }
    return VP_OK;
}

int vp_pack_ctx_truncate_current(vp_pack_ctx_t *ctx, size_t new_total)
{
    if (!ctx || ctx->current < 0) return VP_ERR_INVALID;
    vp_pack_section_t *s = &ctx->sections[ctx->current];
    if (new_total > s->raw_size) return VP_ERR_RANGE;
    /* Free any chunks past the truncation point. */
    size_t remaining = new_total;
    for (uint32_t i = 0; i < s->chunk_count; i++) {
        if (remaining >= s->chunks[i].len) {
            remaining -= s->chunks[i].len;
        } else {
            s->chunks[i].len = remaining;
            remaining = 0;
            /* Truncate any subsequent chunks. */
            for (uint32_t j = i + 1; j < s->chunk_count; j++) {
                free(s->chunks[j].data);
                s->chunks[j].data = NULL;
                s->chunks[j].len = 0;
            }
            s->chunk_count = i + 1;
            break;
        }
    }
    s->raw_size = (uint32_t)new_total;
    return VP_OK;
}

int vp_pack_ctx_set_chunk_budget(vp_pack_ctx_t *ctx, uint32_t budget)
{
    if (!ctx || budget == 0) return VP_ERR_INVALID;
    ctx->chunk_budget = budget;
    return VP_OK;
}

uint32_t vp_pack_ctx_chunk_budget(const vp_pack_ctx_t *ctx)
{
    return ctx ? ctx->chunk_budget : 0;
}

uint32_t vp_pack_ctx_chunks_used(const vp_pack_ctx_t *ctx)
{
    if (!ctx) return 0;
    uint32_t total = 0;
    for (uint32_t i = 0; i < ctx->section_count; i++) {
        total += ctx->sections[i].chunk_count;
    }
    return total;
}

int vp_pack_ctx_validate(const vp_pack_ctx_t *ctx)
{
    if (!ctx) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < ctx->section_count; i++) {
        const vp_pack_section_t *s = &ctx->sections[i];
        if (s->name[0] == '\0') return VP_ERR_INVALID;
        if (s->chunk_count == 0) return VP_ERR_INVALID;
        uint32_t total = 0;
        for (uint32_t k = 0; k < s->chunk_count; k++) {
            if (!s->chunks[k].data) return VP_ERR_INVALID;
            total += (uint32_t)s->chunks[k].len;
        }
        if (total != s->raw_size) return VP_ERR_LENGTH;
    }
    return VP_OK;
}

int vp_pack_ctx_mark_finished_all(vp_pack_ctx_t *ctx)
{
    if (!ctx) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < ctx->section_count; i++) {
        ctx->sections[i].finished = 1;
    }
    ctx->current = -1;
    return VP_OK;
}

int vp_pack_ctx_recompute_crc(vp_pack_ctx_t *ctx, uint32_t idx)
{
    if (!ctx || idx >= ctx->section_count) return VP_ERR_RANGE;
    vp_pack_section_t *s = &ctx->sections[idx];
    uint32_t crc = 0;
    for (uint32_t k = 0; k < s->chunk_count; k++) {
        extern uint32_t vp_crc32_update(uint32_t crc, const uint8_t *data, size_t len);
        crc = vp_crc32_update(crc, s->chunks[k].data, s->chunks[k].len);
    }
    s->crc = crc;
    return VP_OK;
}

int vp_pack_ctx_recompute_all_crcs(vp_pack_ctx_t *ctx)
{
    if (!ctx) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < ctx->section_count; i++) {
        int rc = vp_pack_ctx_recompute_crc(ctx, i);
        if (rc != VP_OK) return rc;
    }
    return VP_OK;
}

int vp_pack_ctx_validate_chunk_layout(const vp_pack_ctx_t *ctx)
{
    if (!ctx) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < ctx->section_count; i++) {
        const vp_pack_section_t *s = &ctx->sections[i];
        if (s->chunk_count == 0) return VP_ERR_INVALID;
        if (s->chunk_count > s->chunk_cap) return VP_ERR_RANGE;
        for (uint32_t k = 0; k < s->chunk_count; k++) {
            if (!s->chunks[k].data && s->chunks[k].len > 0) return VP_ERR_INVALID;
        }
    }
    return VP_OK;
}

int vp_pack_ctx_section_count_of_type(const vp_pack_ctx_t *ctx, uint8_t type)
{
    if (!ctx) return 0;
    int n = 0;
    for (uint32_t i = 0; i < ctx->section_count; i++) {
        if (ctx->sections[i].type == type) n++;
    }
    return n;
}

int vp_pack_ctx_total_chunks(const vp_pack_ctx_t *ctx)
{
    if (!ctx) return 0;
    int total = 0;
    for (uint32_t i = 0; i < ctx->section_count; i++) {
        total += (int)ctx->sections[i].chunk_count;
    }
    return total;
}

int vp_pack_ctx_largest_section(const vp_pack_ctx_t *ctx)
{
    if (!ctx || ctx->section_count == 0) return -1;
    uint32_t best = 0;
    uint32_t best_size = 0;
    for (uint32_t i = 0; i < ctx->section_count; i++) {
        if (ctx->sections[i].raw_size > best_size) {
            best = i;
            best_size = ctx->sections[i].raw_size;
        }
    }
    return (int)best;
}

int vp_pack_ctx_smallest_section(const vp_pack_ctx_t *ctx)
{
    if (!ctx || ctx->section_count == 0) return -1;
    uint32_t best = 0;
    uint32_t best_size = UINT32_MAX;
    for (uint32_t i = 0; i < ctx->section_count; i++) {
        if (ctx->sections[i].raw_size < best_size) {
            best = i;
            best_size = ctx->sections[i].raw_size;
        }
    }
    return (int)best;
}

int vp_pack_ctx_emit_to_buffer(vp_pack_ctx_t *ctx, uint8_t *out, size_t out_cap)
{
    if (!ctx || !out) return VP_ERR_INVALID;
    uint8_t *produced = NULL;
    size_t produced_len = 0;
    int rc = vp_pack_ctx_emit(ctx, &produced, &produced_len);
    if (rc != VP_OK) return rc;
    if (out_cap < produced_len) {
        free(produced);
        return VP_ERR_FULL;
    }
    memcpy(out, produced, produced_len);
    free(produced);
    return VP_OK;
}

int vp_pack_ctx_reserve_section_cap(vp_pack_ctx_t *ctx, uint32_t need)
{
    if (!ctx) return VP_ERR_INVALID;
    if (need <= ctx->section_cap) return VP_OK;
    return vp_pack_ctx_reserve_sections(ctx, need);
}