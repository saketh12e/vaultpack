/*
 * tests/test_range_io.c
 *
 * Tests for the bounded range reader.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/range_io.h"

static void test_range_io_read_within_bounds(void)
{
    uint8_t buf[64];
    for (int i = 0; i < 64; i++) buf[i] = (uint8_t)i;
    vp_range_io_t r;
    vp_range_io_init(&r, buf, sizeof(buf));

    uint8_t out[16];
    size_t  got = 0;
    assert(vp_range_io_read(&r, 8, 16, out, &got) == VP_OK);
    assert(got == 16);
    assert(out[0] == 8 && out[15] == 23);
    printf("test_range_io_read_within_bounds: ok\n");
}

static void test_range_io_read_past_end_truncates(void)
{
    uint8_t buf[16] = {0};
    vp_range_io_t r;
    vp_range_io_init(&r, buf, sizeof(buf));
    uint8_t out[32];
    size_t  got = 0;
    assert(vp_range_io_read(&r, 8, 32, out, &got) == VP_OK);
    assert(got == 8);   /* truncated */
    printf("test_range_io_read_past_end_truncates: ok\n");
}

static void test_range_io_peek(void)
{
    uint8_t buf[16] = {0xAB, 0xCD};
    vp_range_io_t r;
    vp_range_io_init(&r, buf, sizeof(buf));
    const uint8_t *p = NULL;
    assert(vp_range_io_peek(&r, 0, &p, 2) == VP_OK);
    assert(p[0] == 0xAB && p[1] == 0xCD);
    printf("test_range_io_peek: ok\n");
}

int main(void)
{
    test_range_io_read_within_bounds();
    test_range_io_read_past_end_truncates();
    test_range_io_peek();
    printf("test_range_io: ALL OK\n");
    return 0;
}