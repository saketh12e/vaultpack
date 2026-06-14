/*
 * tests/test_offset_remap.c
 *
 * Tests for the offset remap table used by archive merge.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/merge.h"

static void test_push_get(void)
{
    vp_offset_remap_t r;
    vp_offset_remap_init(&r, 1000);
    assert(vp_offset_remap_push(&r, 32) == VP_OK);
    assert(vp_offset_remap_push(&r, 64) == VP_OK);
    assert(vp_offset_remap_push(&r, 128) == VP_OK);
    assert(vp_offset_remap_count(&r) == 3);
    /* The table stores offsets relative to base; get returns absolute. */
    assert(vp_offset_remap_get(&r, 0) == 32);
    assert(vp_offset_remap_get(&r, 1) == 64);
    assert(vp_offset_remap_get(&r, 2) == 128);
    vp_offset_remap_destroy(&r);
    printf("test_push_get: ok\n");
}

static void test_clear_reserve(void)
{
    vp_offset_remap_t r;
    vp_offset_remap_init(&r, 0);
    for (int i = 0; i < 100; i++) assert(vp_offset_remap_push(&r, i) == VP_OK);
    assert(vp_offset_remap_count(&r) == 100);
    assert(vp_offset_remap_clear(&r) == VP_OK);
    assert(vp_offset_remap_count(&r) == 0);
    vp_offset_remap_destroy(&r);
    printf("test_clear_reserve: ok\n");
}

static void test_clone(void)
{
    vp_offset_remap_t a, b;
    vp_offset_remap_init(&a, 500);
    vp_offset_remap_push(&a, 8);
    vp_offset_remap_push(&a, 16);
    assert(vp_offset_remap_clone(&a, &b) == VP_OK);
    assert(vp_offset_remap_count(&b) == 2);
    assert(vp_offset_remap_base(&b) == 500);
    vp_offset_remap_destroy(&a);
    vp_offset_remap_destroy(&b);
    printf("test_clone: ok\n");
}

static void test_sum(void)
{
    vp_offset_remap_t r;
    vp_offset_remap_init(&r, 0);
    vp_offset_remap_push(&r, 10);
    vp_offset_remap_push(&r, 20);
    vp_offset_remap_push(&r, 30);
    assert(vp_offset_remap_sum(&r) == 60);
    vp_offset_remap_destroy(&r);
    printf("test_sum: ok\n");
}

int main(void)
{
    test_push_get();
    test_clear_reserve();
    test_clone();
    test_sum();
    printf("test_offset_remap: ALL OK\n");
    return 0;
}