/*
 * tests/test_interval_engine.c
 *
 * Tests for the interval merge engine.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/interval.h"

static void test_insert_single(void)
{
    vp_interval_engine_t e;
    vp_interval_engine_init(&e);
    assert(vp_interval_engine_insert(&e, 10, 20) == VP_OK);
    assert(vp_interval_engine_count(&e) == 1);
    assert(vp_interval_engine_overlaps(&e, 15, 18) == 1);
    assert(vp_interval_engine_overlaps(&e, 25, 30) == 0);
    vp_interval_engine_destroy(&e);
    printf("test_insert_single: ok\n");
}

static void test_insert_merge_overlapping(void)
{
    vp_interval_engine_t e;
    vp_interval_engine_init(&e);
    vp_interval_engine_insert(&e, 10, 20);
    vp_interval_engine_insert(&e, 18, 30);
    /* Should merge into a single [10, 30) range. */
    assert(vp_interval_engine_count(&e) == 1);
    vp_interval_engine_destroy(&e);
    printf("test_insert_merge_overlapping: ok\n");
}

static void test_insert_disjoint(void)
{
    vp_interval_engine_t e;
    vp_interval_engine_init(&e);
    vp_interval_engine_insert(&e, 10, 20);
    vp_interval_engine_insert(&e, 30, 40);
    assert(vp_interval_engine_count(&e) == 2);
    vp_interval_engine_destroy(&e);
    printf("test_insert_disjoint: ok\n");
}

int main(void)
{
    test_insert_single();
    test_insert_merge_overlapping();
    test_insert_disjoint();
    printf("test_interval_engine: ALL OK\n");
    return 0;
}