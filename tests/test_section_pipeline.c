/*
 * tests/test_section_pipeline.c
 *
 * Tests for the pipeline wrapper: raw short-circuit, codec dispatch,
 * CRC verification, and length mismatch.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/section_pipeline.h"
#include "vaultpack/decompress.h"
#include "vaultpack/crc32.h"
#include "vaultpack/error.h"

static void test_pipeline_raw_short_circuit(void)
{
    const char msg[] = "hello world";
    uint32_t crc = vp_crc32_bytes((const uint8_t *)msg, sizeof(msg) - 1);

    vp_section_pipeline_t p;
    vp_section_pipeline_init(&p, VP_CODEC_TAG_RAW,
                             sizeof(msg) - 1, sizeof(msg) - 1, crc);

    uint8_t *out = NULL;
    size_t   out_len = 0;
    int rc = vp_section_pipeline_run(&p, (const uint8_t *)msg,
                                     sizeof(msg) - 1, &out, &out_len);
    assert(rc == VP_OK);
    assert(out_len == sizeof(msg) - 1);
    assert(memcmp(out, msg, sizeof(msg) - 1) == 0);
    free(out);
    vp_section_pipeline_destroy(&p);
    printf("test_pipeline_raw_short_circuit: ok\n");
}

static void test_pipeline_crc_mismatch(void)
{
    const char msg[] = "hello world";
    vp_section_pipeline_t p;
    vp_section_pipeline_init(&p, VP_CODEC_TAG_RAW,
                             sizeof(msg) - 1, sizeof(msg) - 1, 0xDEADBEEF);

    uint8_t *out = NULL;
    size_t   out_len = 0;
    int rc = vp_section_pipeline_run(&p, (const uint8_t *)msg,
                                     sizeof(msg) - 1, &out, &out_len);
    assert(rc == VP_ERR_CHECKSUM);
    free(out);
    vp_section_pipeline_destroy(&p);
    printf("test_pipeline_crc_mismatch: ok\n");
}

static void test_pipeline_lz4x_round_trip(void)
{
    /* Build a "ZL4X" payload that decodes to "hello". */
    uint8_t payload[] = {
        'Z', 'L', '4', 'X',
        0x05, 0x00, 0x00, 0x00,
        0x50, 'h', 'e', 'l', 'l', 'o'
    };
    /* Compute CRC of decoded bytes. */
    uint32_t crc = vp_crc32_bytes((const uint8_t *)"hello", 5);

    vp_section_pipeline_t p;
    vp_section_pipeline_init(&p, VP_CODEC_TAG_LZ4X,
                             5, sizeof(payload), crc);

    uint8_t *out = NULL;
    size_t   out_len = 0;
    int rc = vp_section_pipeline_run(&p, payload, sizeof(payload),
                                     &out, &out_len);
    assert(rc == VP_OK);
    assert(out_len == 5);
    assert(memcmp(out, "hello", 5) == 0);
    free(out);
    vp_section_pipeline_destroy(&p);
    printf("test_pipeline_lz4x_round_trip: ok\n");
}

static void test_pipeline_bad_codec(void)
{
    vp_section_pipeline_t p;
    vp_section_pipeline_init(&p, 0xCAFEBABE, 4, 4, 0);
    uint8_t *out = NULL;
    size_t   out_len = 0;
    int rc = vp_section_pipeline_run(&p, (const uint8_t *)"abcd", 4,
                                     &out, &out_len);
    assert(rc == VP_ERR_INVALID);
    vp_section_pipeline_destroy(&p);
    printf("test_pipeline_bad_codec: ok\n");
}

int main(void)
{
    test_pipeline_raw_short_circuit();
    test_pipeline_crc_mismatch();
    test_pipeline_lz4x_round_trip();
    test_pipeline_bad_codec();
    printf("test_section_pipeline: ALL OK\n");
    return 0;
}