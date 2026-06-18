/*
 * tests/test_pack_emit.c
 *
 * Tests for the pack context emission helpers and the offset table
 * builder. Validates that the on-disk byte layout produced by the
 * writer can be round-tripped through the archive_view API.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/pack_ctx.h"
#include "vaultpack/range_io.h"

static void test_emit_index_v2(void)
{
    vp_pack_ctx_t ctx;
    vp_pack_ctx_init(&ctx);
    vp_pack_ctx_begin(&ctx, "a", 1, 0);
    vp_pack_ctx_append(&ctx, (const uint8_t *)"AAA", 3);
    vp_pack_ctx_finish(&ctx);

    vp_pack_ctx_begin(&ctx, "b", 1, 0);
    vp_pack_ctx_append(&ctx, (const uint8_t *)"BBBB", 4);
    vp_pack_ctx_finish(&ctx);

    uint64_t offsets[8];
    assert(vp_pack_ctx_build_offset_table(&ctx, offsets) == VP_OK);
    assert(offsets[0] == 32);            /* first section starts after header */
    assert(offsets[1] == 32 + 24 + 3);  /* second section starts after first */

    /* Emit using the explicit table. */
    uint8_t *out = NULL;
    size_t   out_len = 0;
    assert(vp_pack_ctx_emit_with_table(&ctx, offsets, 2, &out, &out_len) == VP_OK);
    assert(out_len > 0);
    assert(memcmp(out, "VPAK", 4) == 0);

    /* Read it back through the range reader + view. */
    vp_range_io_t r;
    vp_range_io_init(&r, out, out_len);
    vp_archive_view_t v;
    vp_archive_view_init(&v);
    assert(vp_archive_view_open(&v, &r) == VP_OK);
    assert(vp_archive_view_count(&v) == 2);

    vp_archive_view_destroy(&v);
    free(out);
    vp_pack_ctx_clear_all(&ctx);
    vp_pack_ctx_destroy(&ctx);
    printf("test_emit_index_v2: ok\n");
}

static void test_swap_remove(void)
{
    vp_pack_ctx_t ctx;
    vp_pack_ctx_init(&ctx);
    vp_pack_ctx_begin(&ctx, "alpha", 1, 0);
    vp_pack_ctx_append(&ctx, (const uint8_t *)"first", 5);
    vp_pack_ctx_finish(&ctx);
    vp_pack_ctx_begin(&ctx, "beta", 1, 0);
    vp_pack_ctx_append(&ctx, (const uint8_t *)"second", 6);
    vp_pack_ctx_finish(&ctx);
    vp_pack_ctx_begin(&ctx, "gamma", 1, 0);
    vp_pack_ctx_append(&ctx, (const uint8_t *)"third", 5);
    vp_pack_ctx_finish(&ctx);

    assert(vp_pack_ctx_section_count(&ctx) == 3);
    assert(vp_pack_ctx_swap_sections(&ctx, 0, 2) == VP_OK);
    /* After swap, the first section is "gamma". */
    const char *first = vp_pack_ctx_current_name(&ctx);
    (void)first;
    assert(strcmp(vp_pack_section_name(&ctx.sections[0]), "gamma") == 0);

    assert(vp_pack_ctx_remove_at(&ctx, 1) == VP_OK);
    assert(vp_pack_ctx_section_count(&ctx) == 2);
    assert(vp_pack_ctx_clear_all(&ctx) == VP_OK);
    assert(vp_pack_ctx_section_count(&ctx) == 0);

    vp_pack_ctx_destroy(&ctx);
    printf("test_swap_remove: ok\n");
}

static void test_chunk_capacity_used(void)
{
    vp_pack_ctx_t ctx;
    vp_pack_ctx_init(&ctx);
    vp_pack_ctx_begin(&ctx, "x", 1, 0);
    vp_pack_ctx_append(&ctx, (const uint8_t *)"1", 1);
    vp_pack_ctx_append(&ctx, (const uint8_t *)"2", 1);
    vp_pack_ctx_append(&ctx, (const uint8_t *)"3", 1);
    vp_pack_ctx_finish(&ctx);
    assert(vp_pack_ctx_chunk_capacity_used(&ctx) == 3);
    assert(vp_pack_ctx_total_size(&ctx) == 3);
    vp_pack_ctx_destroy(&ctx);
    printf("test_chunk_capacity_used: ok\n");
}

int main(void)
{
    test_emit_index_v2();
    test_swap_remove();
    test_chunk_capacity_used();
    printf("test_pack_emit: ALL OK\n");
    return 0;
}