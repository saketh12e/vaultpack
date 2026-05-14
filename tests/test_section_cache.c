/*
 * tests/test_section_cache.c
 *
 * Tests for the section payload LRU cache.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/section_cache.h"

static void test_cache_put_get(void)
{
    vp_section_cache_t c;
    vp_section_cache_init(&c);
    const char *msg = "hello world";
    assert(vp_section_cache_put(&c, "alpha", (const uint8_t *)msg, 11) == VP_OK);

    const uint8_t *out = NULL;
    size_t out_len = 0;
    assert(vp_section_cache_get(&c, "alpha", &out, &out_len) == VP_OK);
    assert(out_len == 11);
    assert(memcmp(out, msg, 11) == 0);
    vp_section_cache_destroy(&c);
    printf("test_cache_put_get: ok\n");
}

static void test_cache_pin_unpin(void)
{
    vp_section_cache_t c;
    vp_section_cache_init(&c);
    assert(vp_section_cache_put(&c, "x", (const uint8_t *)"abc", 3) == VP_OK);
    assert(vp_section_cache_pin(&c, "x") == VP_OK);
    assert(vp_section_cache_unpin(&c, "x") == VP_OK);
    vp_section_cache_destroy(&c);
    printf("test_cache_pin_unpin: ok\n");
}

static void test_cache_evicts_lru(void)
{
    vp_section_cache_t c;
    vp_section_cache_init(&c);
    /* Fill all 8 slots. */
    char name[2] = {0, 0};
    for (int i = 0; i < VP_SECTION_CACHE_SLOTS; i++) {
        name[0] = 'a' + i;
        assert(vp_section_cache_put(&c, name, (const uint8_t *)"x", 1) == VP_OK);
    }
    /* Add a 9th — should evict the LRU. */
    assert(vp_section_cache_put(&c, "z", (const uint8_t *)"y", 1) == VP_OK);
    /* "a" should be evicted (it was the earliest). */
    const uint8_t *p = NULL;
    size_t pl = 0;
    assert(vp_section_cache_get(&c, "a", &p, &pl) == VP_ERR_NOTFOUND);
    vp_section_cache_destroy(&c);
    printf("test_cache_evicts_lru: ok\n");
}

int main(void)
{
    test_cache_put_get();
    test_cache_pin_unpin();
    test_cache_evicts_lru();
    printf("test_section_cache: ALL OK\n");
    return 0;
}