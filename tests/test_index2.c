/*
 * tests/test_index2.c
 *
 * Tests for the v2 index insert + compaction.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/index2.h"

static void test_index2_insert_lookup(void)
{
    vp_index2_t idx;
    vp_index2_init(&idx);

    vp_index2_slot_t s;
    memset(&s, 0, sizeof(s));
    strcpy(s.name, "alpha");
    s.offset = 32;
    s.size = 16;
    s.type = 1;
    assert(vp_index2_insert(&idx, &s) == VP_OK);

    vp_index2_slot_t out;
    assert(vp_index2_lookup(&idx, "alpha", &out) == VP_OK);
    assert(out.offset == 32);

    vp_index2_destroy(&idx);
    printf("test_index2_insert_lookup: ok\n");
}

static void test_index2_compact_merges(void)
{
    vp_index2_t idx;
    vp_index2_init(&idx);

    vp_index2_slot_t s1 = {"a", 0, 16, 1};
    vp_index2_slot_t s2 = {"b", 16, 16, 1};
    vp_index2_insert(&idx, &s1);
    vp_index2_insert(&idx, &s2);
    assert(vp_index2_compact(&idx) == VP_OK);
    /* Two contiguous slots should merge into one. */
    assert(idx.count == 1);
    vp_index2_destroy(&idx);
    printf("test_index2_compact_merges: ok\n");
}

int main(void)
{
    test_index2_insert_lookup();
    test_index2_compact_merges();
    printf("test_index2: ALL OK\n");
    return 0;
}