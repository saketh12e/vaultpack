/*
 * tests/test_decompress.c
 *
 * Unit tests for the LZ4X and HC01 block decoders.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/decompress.h"
#include "vaultpack/error.h"

static void test_lz4x_literal_only(void)
{
    /* "ZL4X" + declared=5 + literal-only token (lit_len=5, match_len=0). */
    uint8_t in[] = {
        0x5A, 0x4C, 0x34, 0x58,            /* "ZL4X" */
        0x05, 0x00, 0x00, 0x00,            /* declared = 5 */
        0x50,                              /* token: lit_len=5, match_len=0 */
        'h', 'e', 'l', 'l', 'o'
    };
    uint8_t out[16];
    size_t  out_len = 0;
    int rc = vp_decompress_lz4x(in, sizeof(in), out, sizeof(out), &out_len);
    assert(rc == VP_OK);
    assert(out_len == 5);
    assert(memcmp(out, "hello", 5) == 0);
    printf("test_lz4x_literal_only: ok\n");
}

static void test_lz4x_rejects_bad_distance(void)
{
    /* "ZL4X" + declared=4 + literal=4 then match with distance=0. */
    uint8_t in[] = {
        0x5A, 0x4C, 0x34, 0x58,
        0x04, 0x00, 0x00, 0x00,
        0x44,                              /* lit=4, match=4 */
        'a', 'b', 'c', 'd',
        0x00, 0x00                          /* distance = 0 -> reject */
    };
    uint8_t out[16];
    size_t  out_len = 0;
    int rc = vp_decompress_lz4x(in, sizeof(in), out, sizeof(out), &out_len);
    assert(rc == VP_ERR_INVALID);
    printf("test_lz4x_rejects_bad_distance: ok\n");
}

static void test_hc01_literal_run(void)
{
    /* "HC01" + declared=4 + literal class, base=4, value='A'. */
    uint8_t in[] = {
        0x48, 0x43, 0x30, 0x31,
        0x04, 0x00, 0x00, 0x00,
        0x03,                              /* cls=0, base=4 */
        'A'
    };
    uint8_t out[8];
    size_t  out_len = 0;
    int rc = vp_decompress_hc01(in, sizeof(in), out, sizeof(out), &out_len);
    assert(rc == VP_OK);
    assert(out_len == 4);
    assert(memcmp(out, "AAAA", 4) == 0);
    printf("test_hc01_literal_run: ok\n");
}

static void test_hc01_repeat_short(void)
{
    /* "HC01" + declared=5 + first literal then repeat-short. */
    uint8_t in[] = {
        0x48, 0x43, 0x30, 0x31,
        0x05, 0x00, 0x00, 0x00,
        0x00,                              /* cls=0, base=1 */
        'X',
        0x13,                              /* cls=1, base=4 */
    };
    uint8_t out[8];
    size_t  out_len = 0;
    int rc = vp_decompress_hc01(in, sizeof(in), out, sizeof(out), &out_len);
    assert(rc == VP_OK);
    assert(out_len == 5);
    assert(memcmp(out, "XXXXX", 5) == 0);
    printf("test_hc01_repeat_short: ok\n");
}

static void test_decompress_rejects_truncated(void)
{
    uint8_t in[] = { 0x5A, 0x4C, 0x34, 0x58, 0x10 };
    uint8_t out[8];
    size_t  out_len = 0;
    int rc = vp_decompress_lz4x(in, sizeof(in), out, sizeof(out), &out_len);
    assert(rc == VP_ERR_TRUNCATED);
    printf("test_decompress_rejects_truncated: ok\n");
}

int main(void)
{
    test_lz4x_literal_only();
    test_lz4x_rejects_bad_distance();
    test_hc01_literal_run();
    test_hc01_repeat_short();
    test_decompress_rejects_truncated();
    printf("test_decompress: ALL OK\n");
    return 0;
}