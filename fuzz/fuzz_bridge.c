/*
 * fuzz/fuzz_bridge.c
 *
 * C bridge that exercises each fuzz target's payload as plain C functions.
 * On the VPS, the upstream scoring script links these together with the
 * core vaultpack objects and a `main()` driver that reads a `.bin` file and
 * calls one bridge function per harness.
 */

#include <string.h>
#include <stdlib.h>

#include "vaultpack/vaultpack.h"
#include "vaultpack/stream.h"
#include "vaultpack/decompress.h"
#include "vaultpack/section_pipeline.h"

#include "fuzz_bridge.h"

void vp_bridge_archive(const uint8_t *data, size_t size)
{
    if (!data || size < 32)
        return;
    vp_archive_t *arc = NULL;
    int rc = vp_open_archive(data, size, &arc);
    if (rc == VP_OK && arc)
        vp_close_archive(arc);
}

void vp_bridge_section(const uint8_t *data, size_t size)
{
    if (!data || size < 20)
        return;
    vp_section_t sec;
    memset(&sec, 0, sizeof(sec));
    vp_parse_section_header(data, size, &sec);
    vp_validate_section(&sec);
    vp_section_summary(&sec);
    vp_section_cleanup(&sec);
}

void vp_bridge_metadata(const uint8_t *data, size_t size)
{
    if (!data || size < (size_t)sizeof(vp_meta_block_hdr_t))
        return;
    vp_meta_block_t *block = NULL;
    if (vp_read_metadata_block(data, size, &block) != VP_OK)
        return;
    if (block->key_count > 0)
        vp_metadata_lookup(block, block->entries[block->key_count - 1].key);
    vp_process_meta_block(block, NULL);
    vp_free_metadata_block(block);
}

void vp_bridge_index(const uint8_t *data, size_t size)
{
    if (!data || size < 8)
        return;
    uint16_t width  = (uint16_t)(data[0]     | ((uint16_t)data[1] << 8));
    uint16_t depth  = (uint16_t)(data[2]     | ((uint16_t)data[3] << 8));
    if (width == 0 || depth == 0)
        return;

    vp_index_t idx;
    memset(&idx, 0, sizeof(idx));
    if (vp_build_index(&idx, width, depth) != VP_OK)
        return;

    /* Parse dir entries after the 4-byte width/depth prefix. */
    const uint8_t *p = data + 4;
    size_t remaining = size - 4;
    uint32_t max_entries = (uint32_t)(remaining / 58);
    if (max_entries > 1024)
        max_entries = 1024;
    if (max_entries > 64)
        max_entries = 64;

    vp_dir_entry_t dir[64];
    uint32_t n = max_entries;
    for (uint32_t i = 0; i < n; i++) {
        memset(&dir[i], 0, sizeof(dir[i]));
        memcpy(dir[i].name, p, 32);                 p += 32;
        dir[i].section_offset = 0;
        for (int k = 0; k < 8; k++)
            dir[i].section_offset |= ((uint64_t)p[k]) << (k * 8);
        p += 8;
        dir[i].section_size = (uint32_t)p[0]
                            | ((uint32_t)p[1] << 8)
                            | ((uint32_t)p[2] << 16)
                            | ((uint32_t)p[3] << 24);
        p += 4;
        dir[i].flags = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
        p += 2;
        dir[i].type = p[0];
        dir[i].reserved = p[1];
        p += 2;
        dir[i].meta_count = (uint32_t)p[0]
                          | ((uint32_t)p[1] << 8)
                          | ((uint32_t)p[2] << 16)
                          | ((uint32_t)p[3] << 24);
        p += 4;
        dir[i].sub_count = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
        p += 2;
        dir[i].sub_stride = (uint32_t)p[0]
                          | ((uint32_t)p[1] << 8)
                          | ((uint32_t)p[2] << 16)
                          | ((uint32_t)p[3] << 24);
        p += 4;
    }

    vp_index_populate(&idx, dir, n);
    vp_index_destroy(&idx);
}

void vp_bridge_extract(const uint8_t *data, size_t size)
{
    if (!data || size < 20)
        return;
    vp_extract_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.out_size = 4096;
    vp_extract_section(&ctx, data, size);
    vp_extract_cleanup(&ctx);
}

void vp_bridge_stream(const uint8_t *data, size_t size)
{
    if (!data || size == 0) return;
    vp_stream_t s;
    vp_stream_init(&s, data, size);

    (void)vp_stream_read_u8(&s);
    (void)vp_stream_read_u16(&s);
    (void)vp_stream_read_u32(&s);
    (void)vp_stream_read_u64(&s);

    vp_stream_bookmark_t bm = vp_stream_bookmark(&s);
    (void)vp_stream_read_bytes(&s, NULL, 0);
    vp_stream_restore(&s, bm);

    vp_stream_window_t w = vp_stream_window(&s, 8);
    if (w.ptr && vp_stream_remaining(&s) >= 8) {
        vp_stream_window_t w2 = vp_stream_window(&s, 256);
        (void)w2;
        uint8_t buf[8];
        vp_stream_replay_window(&s, &w, buf, sizeof(buf));
    } else {
        uint8_t buf[8];
        vp_stream_replay_window(&s, &w, buf, sizeof(buf));
    }
}

void vp_bridge_manifest(const uint8_t *data, size_t size)
{
    vp_manifest_t m;
    vp_manifest_init(&m);
    vp_manifest_parse(&m, (const char *)data, size);
    vp_manifest_finalize(&m);
    vp_manifest_free(&m);
}

void vp_bridge_codec(const uint8_t *data, size_t size)
{
    if (size == 0) return;
    uint8_t out[16 * 1024 * 1024];
    size_t out_len = 0;
    vp_codec_decode(data, size, out, sizeof(out), &out_len);
}

void vp_bridge_writer(const uint8_t *data, size_t size)
{
    if (size == 0) return;
    vp_manifest_t m;
    vp_manifest_init(&m);
    vp_manifest_parse(&m, (const char *)data, size);

    vp_writer_t w;
    vp_writer_init(&w);
    vp_manifest_apply(&m, &w);

    vp_writer_rollback_section(&w);

    uint8_t *out = NULL;
    size_t out_len = 0;
    vp_writer_emit(&w, &out, &out_len);
    free(out);

    vp_writer_destroy(&w);
    vp_manifest_free(&m);
}

void vp_bridge_validate(const uint8_t *data, size_t size)
{
    if (size < 32) return;
    vp_archive_t *arc = NULL;
    if (vp_open_archive(data, size, &arc) != VP_OK) return;

    vp_validate_report_t r;
    vp_validate_report_init(&r);
    vp_validate_archive(arc, &r);
    vp_validate_report_free(&r);

    uint64_t *iv = NULL;
    uint32_t n = 0;
    vp_validate_collect_intervals(arc, &iv, &n);
    free(iv);

    vp_close_archive(arc);
}

/* === Subsystem bridges (stubs that satisfy the HARNESS_CALLS table) ===
 *
 * Each new subsystem exports a thin bridge that exercises the public
* APIs. The detailed planting surfaces live in src/<subsystem>.c; the
 * bridges here are deliberately minimal so the planted defects stay
 * observable through the fuzzer driver.  Most are lifetime or
 * ownership/lifecycle mistakes.
 */

void vp_bridge_pipeline(const uint8_t *data, size_t size)
{
    if (!data || size < 8) return;
    uint32_t codec = (uint32_t)data[0]
                   | ((uint32_t)data[1] << 8)
                   | ((uint32_t)data[2] << 16)
                   | ((uint32_t)data[3] << 24);
    uint32_t raw_size = (uint32_t)data[4]
                      | ((uint32_t)data[5] << 8)
                      | ((uint32_t)data[6] << 16)
                      | ((uint32_t)data[7] << 24);
    if (raw_size == 0 || raw_size > (1u << 20)) return;

    uint8_t *out = (uint8_t *)malloc(raw_size);
    if (!out) return;
    size_t produced = 0;
    if (codec == VP_CODEC_TAG_LZ4X) {
        vp_decompress_lz4x(data, size, out, raw_size, &produced);
    } else if (codec == VP_CODEC_TAG_HC01) {
        vp_decompress_hc01(data, size, out, raw_size, &produced);
    }
    free(out);

    vp_section_pipeline_t p;
    vp_section_pipeline_init(&p, codec, raw_size, size - 8, 0);
    uint8_t *pout = NULL;
    size_t   plen = 0;
    vp_section_pipeline_run(&p, data + 8, size - 8, &pout, &plen);
    free(pout);
    vp_section_pipeline_destroy(&p);
}

void vp_bridge_pack(const uint8_t *data, size_t size)
{
    /* Drive the streaming pack context: init, append chunks, emit. */
    extern int vp_pack_ctx_run(const uint8_t *data, size_t size);
    vp_pack_ctx_run(data, size);
}

void vp_bridge_cache(const uint8_t *data, size_t size)
{
    extern int vp_section_cache_run(const uint8_t *data, size_t size);
    vp_section_cache_run(data, size);
}

void vp_bridge_merge(const uint8_t *data, size_t size)
{
    extern int vp_archive_merge_run(const uint8_t *data, size_t size);
    vp_archive_merge_run(data, size);
}

void vp_bridge_repair(const uint8_t *data, size_t size)
{
    extern int vp_archive_repair_run(const uint8_t *data, size_t size);
    vp_archive_repair_run(data, size);
}

void vp_bridge_schema(const uint8_t *data, size_t size)
{
    extern int vp_meta_schema_run(const uint8_t *data, size_t size);
    vp_meta_schema_run(data, size);
}

void vp_bridge_index2(const uint8_t *data, size_t size)
{
    extern int vp_index2_run(const uint8_t *data, size_t size);
    vp_index2_run(data, size);
}

void vp_bridge_range(const uint8_t *data, size_t size)
{
    extern int vp_range_io_run(const uint8_t *data, size_t size);
    vp_range_io_run(data, size);
}

void vp_bridge_diff(const uint8_t *data, size_t size)
{
    extern int vp_archive_diff_run(const uint8_t *data, size_t size);
    vp_archive_diff_run(data, size);
}

void vp_bridge_compose(const uint8_t *data, size_t size)
{
    extern int vp_meta_compose_run(const uint8_t *data, size_t size);
    vp_meta_compose_run(data, size);
}
