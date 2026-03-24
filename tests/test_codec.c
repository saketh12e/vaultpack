#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/codec.h"

static void test_codec_raw(void)
{
    const char *msg = "hello world";
    uint8_t out[64];
    size_t out_len = 0;
    int rc = vp_codec_decode((const uint8_t *)msg, strlen(msg),
                             out, sizeof(out), &out_len);
    assert(rc == VP_OK);
    assert(out_len == strlen(msg));
    assert(memcmp(out, msg, out_len) == 0);
    printf("test_codec_raw: ok\n");
}

static void test_codec_rle_simple(void)
{
    uint8_t enc[64];
    size_t enc_len = 0;
    int rc = vp_codec_encode_rle((const uint8_t *)"aaabbc", 5,
                                 enc, sizeof(enc), &enc_len);
    assert(rc == VP_OK);
    assert(enc_len > 6);

    uint8_t dec[64];
    size_t dec_len = 0;
    rc = vp_codec_decode(enc, enc_len, dec, sizeof(dec), &dec_len);
    assert(rc == VP_OK);
    assert(dec_len == 5);
    assert(memcmp(dec, "aaabbc", 5) == 0);
    printf("test_codec_rle_simple: ok\n");
}

static void test_codec_rle_truncated(void)
{
    /* Block header says run_count=5 but only 1 run is present. */
    uint8_t bad[] = { 'R','L','E','1', 0x05, 0x00, 'a', 1, 0,0,0 };
    uint8_t out[64];
    size_t out_len = 0;
    int rc = vp_codec_decode(bad, sizeof(bad), out, sizeof(out), &out_len);
    assert(rc == VP_ERR_TRUNCATED);
    printf("test_codec_rle_truncated: ok\n");
}

static void test_codec_rle_bad_magic_falls_back_to_raw(void)
{
    /* Magic != "RLE1" → treated as raw passthrough. */
    uint8_t msg[4] = { 'a', 'b', 'c', 'd' };
    uint8_t out[8];
    size_t out_len = 0;
    int rc = vp_codec_decode(msg, 4, out, sizeof(out), &out_len);
    assert(rc == VP_OK);
    assert(out_len == 4);
    assert(memcmp(out, msg, 4) == 0);
    printf("test_codec_rle_bad_magic: ok\n");
}

int main(void)
{
    test_codec_raw();
    test_codec_rle_simple();
    test_codec_rle_truncated();
    test_codec_rle_bad_magic_falls_back_to_raw();
    printf("test_codec: all assertions passed\n");
    return 0;
}
