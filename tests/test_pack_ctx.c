/*
 * tests/test_pack_ctx.c
 *
 * Tests for the streaming pack context API.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/pack_ctx.h"

static void test_pack_single_section(void)
{
    vp_pack_ctx_t ctx;
    vp_pack_ctx_init(&ctx);

    assert(vp_pack_ctx_begin(&ctx, "alpha", 1, 0) == VP_OK);
    const char *msg = "hello";
    assert(vp_pack_ctx_append(&ctx, (const uint8_t *)msg, 5) == VP_OK);
    assert(vp_pack_ctx_finish(&ctx) == VP_OK);

    uint8_t *out = NULL;
    size_t   out_len = 0;
    assert(vp_pack_ctx_emit(&ctx, &out, &out_len) == VP_OK);
    assert(out_len > 0);
    assert(memcmp(out, "VPAK", 4) == 0);
    free(out);
    vp_pack_ctx_destroy(&ctx);
    printf("test_pack_single_section: ok\n");
}

static void test_pack_rejects_duplicate(void)
{
    vp_pack_ctx_t ctx;
    vp_pack_ctx_init(&ctx);
    assert(vp_pack_ctx_begin(&ctx, "alpha", 1, 0) == VP_OK);
    assert(vp_pack_ctx_finish(&ctx) == VP_OK);
    assert(vp_pack_ctx_begin(&ctx, "alpha", 1, 0) == VP_ERR_DUPLICATE);
    vp_pack_ctx_destroy(&ctx);
    printf("test_pack_rejects_duplicate: ok\n");
}

static void test_chunk_assembler_basic(void)
{
    vp_chunk_assembler_t a;
    assert(vp_chunk_assembler_init(&a) == VP_OK);
    assert(vp_chunk_assembler_append(&a, (const uint8_t *)"hi", 2) == VP_OK);
    assert(vp_chunk_assembler_append(&a, (const uint8_t *)"there", 5) == VP_OK);
    uint8_t *out = NULL;
    size_t  out_len = 0;
    assert(vp_chunk_assembler_finalize(&a, &out, &out_len) == VP_OK);
    assert(out_len == 7);
    assert(memcmp(out, "hithere", 7) == 0);
    free(out);
    vp_chunk_assembler_destroy(&a);
    printf("test_chunk_assembler_basic: ok\n");
}

static void test_dir_emit_append_serialize(void)
{
    vp_dir_emit_t d;
    vp_dir_emit_init(&d);
    assert(vp_dir_emit_append(&d, "alpha", 32, 16, 1) == VP_OK);
    assert(vp_dir_emit_append(&d, "beta",  60,  8, 1) == VP_OK);
    assert(vp_dir_emit_size(&d) == 116);
    uint8_t out[256];
    size_t  out_len = 0;
    assert(vp_dir_emit_serialize(&d, out, sizeof(out), &out_len) == VP_OK);
    assert(out_len == 116);
    vp_dir_emit_destroy(&d);
    printf("test_dir_emit_append_serialize: ok\n");
}

int main(void)
{
    test_pack_single_section();
    test_pack_rejects_duplicate();
    test_chunk_assembler_basic();
    test_dir_emit_append_serialize();
    printf("test_pack_ctx: ALL OK\n");
    return 0;
}