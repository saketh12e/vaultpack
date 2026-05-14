/*
 * tests/test_archive_repair.c
 *
 * Tests for the archive repair scanner.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/repair.h"

static void test_repair_scan_finds_sect(void)
{
    uint8_t blob[64];
    memset(blob, 0, sizeof(blob));
    /* Place a SECT header at offset 0. */
    memcpy(blob, "SECT", 4);
    blob[8] = 0;    /* raw_size = 0 */
    blob[12] = 0;   /* comp_size = 0 */
    vp_repair_ctx_t r;
    vp_repair_ctx_init(&r, blob, sizeof(blob));
    assert(vp_repair_scan(&r) == VP_OK);
    assert(r.count == 1);
    vp_repair_ctx_destroy(&r);
    printf("test_repair_scan_finds_sect: ok\n");
}

static void test_repair_scan_skips_garbage(void)
{
    uint8_t blob[32];
    memset(blob, 'X', sizeof(blob));
    vp_repair_ctx_t r;
    vp_repair_ctx_init(&r, blob, sizeof(blob));
    assert(vp_repair_scan(&r) == VP_OK);
    assert(r.count == 0);
    vp_repair_ctx_destroy(&r);
    printf("test_repair_scan_skips_garbage: ok\n");
}

int main(void)
{
    test_repair_scan_finds_sect();
    test_repair_scan_skips_garbage();
    printf("test_archive_repair: ALL OK\n");
    return 0;
}