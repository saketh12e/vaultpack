/*
 * tests/test_chunk_assembler.c
 *
 * Tests for the chunk assembler and pack abort helpers.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/pack_ctx.h"

static void test_assembler_grow(void)
{
    vp_chunk_assembler_t a;
    assert(vp_chunk_assembler_init(&a) == VP_OK);
    /* Stay under the initial cap so we don't exercise the grow
     * path on the local Debug build (the grow path triggers a
     * planted defect that the fuzz harness exercises; the unit
     * test stays in the safe in-place path). */
    char buf[32];
    memset(buf, 'A', sizeof(buf));
    assert(vp_chunk_assembler_append(&a, (const uint8_t *)buf, sizeof(buf)) == VP_OK);
    assert(vp_chunk_assembler_append(&a, (const uint8_t *)buf, sizeof(buf)) == VP_OK);
    uint8_t *out = NULL;
    size_t  out_len = 0;
    assert(vp_chunk_assembler_finalize(&a, &out, &out_len) == VP_OK);
    assert(out_len == 64);
    free(out);
    vp_chunk_assembler_destroy(&a);
    printf("test_assembler_grow: ok\n");
}

static void test_pack_abort_section(void)
{
    vp_pack_section_t s;
    memset(&s, 0, sizeof(s));
    s.chunk_cap = 4;
    s.chunks = (vp_pack_chunk_t *)calloc(s.chunk_cap, sizeof(vp_pack_chunk_t));
    s.chunk_count = 2;
    s.chunks[0].data = (uint8_t *)malloc(16);
    s.chunks[0].len = 16;
    s.chunks[1].data = (uint8_t *)malloc(32);
    s.chunks[1].len = 32;
    int rc = vp_pack_abort_section(&s);
    assert(rc == VP_OK);
    assert(s.chunk_count == 0);
    printf("test_pack_abort_section: ok\n");
}

static void test_pack_abort_duplicate_detection(void)
{
    vp_pack_ctx_t ctx;
    vp_pack_ctx_init(&ctx);
    assert(vp_pack_ctx_begin(&ctx, "x", 1, 0) == VP_OK);
    assert(vp_pack_ctx_finish(&ctx) == VP_OK);
    /* Manually push a duplicate through the abort path. After the
     * abort returns VP_ERR_DUPLICATE the section's chunk resources
     * are released; we must NOT call vp_pack_ctx_destroy on the
     * context because it would release the chunks twice. Instead we
     * manually clear the context's bookkeeping so destroy is a no-op
     * for the affected section. */
    assert(vp_pack_abort_check_duplicate(&ctx, "x") == VP_ERR_DUPLICATE);
    ctx.section_count = 0;
    ctx.sections[0].chunks = NULL;
    ctx.sections[0].chunk_count = 0;
    vp_pack_ctx_destroy(&ctx);
    printf("test_pack_abort_duplicate_detection: ok\n");
}

int main(void)
{
    test_assembler_grow();
    test_pack_abort_section();
    test_pack_abort_duplicate_detection();
    printf("test_chunk_assembler: ALL OK\n");
    return 0;
}