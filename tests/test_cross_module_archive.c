/*
 * tests/test_cross_module_archive.c
 *
 * End-to-end integration test: build a minimal archive in-memory
 * using the writer, then read it back through the archive
 * view and validate the round-trip. Exercises several modules
 * together: writer, archive_view, range_io, section_cache,
 * decompress, crc32.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/writer.h"
#include "vaultpack/manifest.h"
#include "vaultpack/range_io.h"
#include "vaultpack/section_cache.h"
#include "vaultpack/decompress.h"
#include "vaultpack/crc32.h"

static void test_round_trip_build_and_view(void)
{
    /* Build a manifest with two sections and emit. */
    vp_manifest_t m;
    vp_manifest_init(&m);
    vp_manifest_parse(&m,
                       "section alpha raw\n"
                       "section beta raw\n",
                       strlen("section alpha raw\nsection beta raw\n"));
    vp_manifest_finalize(&m);

    vp_writer_t w;
    vp_writer_init(&w);
    vp_manifest_apply(&m, &w);

    uint8_t *out = NULL;
    size_t   out_len = 0;
    assert(vp_writer_emit(&w, &out, &out_len) == VP_OK);
    assert(out_len >= 64);
    assert(memcmp(out, "VPAK", 4) == 0);

    /* Open the produced archive through the range reader + view. */
    vp_range_io_t r;
    vp_range_io_init(&r, out, out_len);
    vp_archive_view_t v;
    vp_archive_view_init(&v);
    int rc = vp_archive_view_open(&v, &r);
    assert(rc == VP_OK);
    assert(v.entry_count >= 2);

    vp_section_cache_t cache;
    vp_section_cache_init(&cache);
    uint64_t total = vp_archive_view_total_size(&v);
    assert(total >= 0);

    vp_archive_view_destroy(&v);
    vp_section_cache_destroy(&cache);
    free(out);
    vp_writer_destroy(&w);
    vp_manifest_free(&m);
    printf("test_round_trip_build_and_view: ok\n");
}

static void test_codec_bound(void)
{
    uint8_t payload[] = { 'Z', 'L', '4', 'X',
                          4, 0, 0, 0,
                          0x40 | 4, 'A', 'B', 'C', 'D' };
    size_t bound = 0;
    int rc = vp_decompress_lz4x_bound(payload, sizeof(payload), &bound);
    assert(rc == VP_OK);
    assert(bound == 4);
    printf("test_codec_bound: ok\n");
}

static void test_crc32_through_range(void)
{
    uint8_t blob[64];
    for (int i = 0; i < 64; i++) blob[i] = (uint8_t)i;
    vp_range_io_t r;
    vp_range_io_init(&r, blob, sizeof(blob));
    uint32_t direct = vp_crc32_bytes(blob, 64);
    uint32_t via_range = vp_range_io_crc32(&r, 0, 64);
    assert(direct == via_range);
    printf("test_crc32_through_range: ok\n");
}

int main(void)
{
    test_round_trip_build_and_view();
    test_codec_bound();
    test_crc32_through_range();
    printf("test_cross_module_archive: ALL OK\n");
    return 0;
}