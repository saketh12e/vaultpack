/*
 * tests/test_archive_merge.c
 *
 * Tests for the archive merger.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/merge.h"

static void test_merge_run_minimal(void)
{
    /* Two minimal archives: each has 32-byte header only. */
    uint8_t a[32] = {0};
    uint8_t b[32] = {0};
    memcpy(a, "VPAK", 4);
    memcpy(b, "VPAK", 4);
    a[8] = 0; a[9] = 0; a[10] = 0; a[11] = 0;
    b[8] = 0; b[9] = 0; b[10] = 0; b[11] = 0;

    vp_merge_ctx_t m;
    vp_merge_ctx_init(&m, a, sizeof(a), b, sizeof(b));
    assert(vp_merge_run(&m) == VP_OK);
    vp_merge_ctx_destroy(&m);
    printf("test_merge_run_minimal: ok\n");
}

static void test_merge_rejects_bad_header(void)
{
    uint8_t a[32] = {0};
    uint8_t b[32] = {0};
    /* "VPAK" magic missing in a */
    vp_merge_ctx_t m;
    vp_merge_ctx_init(&m, a, sizeof(a), b, sizeof(b));
    int rc = vp_merge_run(&m);
    assert(rc != VP_OK);
    vp_merge_ctx_destroy(&m);
    printf("test_merge_rejects_bad_header: ok\n");
}

int main(void)
{
    test_merge_run_minimal();
    test_merge_rejects_bad_header();
    printf("test_archive_merge: ALL OK\n");
    return 0;
}