/*
 * examples/codec_round_trip.c
 *
 * Round-trip an LZ4X and HC01 block through the codec API.
 * Demonstrates the declared-size, payload, and CRC pipeline.
 *
 * Build: cc examples/codec_round_trip.c src/*.c -I include -o /tmp/crt
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/decompress.h"
#include "vaultpack/crc32.h"

static void round_trip_lz4x(const char *msg)
{
    size_t in_len = 8 + 1 + strlen(msg);
    uint8_t *in = (uint8_t *)malloc(in_len);
    in[0] = 'Z'; in[1] = 'L'; in[2] = '4'; in[3] = 'X';
    uint32_t declared = (uint32_t)strlen(msg);
    in[4] = (uint8_t)declared;
    in[5] = (uint8_t)(declared >> 8);
    in[6] = (uint8_t)(declared >> 16);
    in[7] = (uint8_t)(declared >> 24);
    in[8] = (uint8_t)((strlen(msg) & 0x0F) << 4);  /* literal-only token */
    memcpy(in + 9, msg, strlen(msg));

    uint8_t out[256];
    size_t  out_len = 0;
    int rc = vp_decompress_lz4x(in, in_len, out, sizeof(out), &out_len);
    printf("LZ4X %s rc=%d out_len=%zu match=%d\n",
           msg, rc, out_len, rc == VP_OK && out_len == strlen(msg)
                && memcmp(out, msg, out_len) == 0);
    free(in);
}

static void round_trip_hc01(const char *msg)
{
    size_t in_len = 8 + 1 + 1 + strlen(msg);
    uint8_t *in = (uint8_t *)malloc(in_len);
    in[0] = 'H'; in[1] = 'C'; in[2] = '0'; in[3] = '1';
    uint32_t declared = (uint32_t)strlen(msg);
    in[4] = (uint8_t)declared;
    in[5] = (uint8_t)(declared >> 8);
    in[6] = (uint8_t)(declared >> 16);
    in[7] = (uint8_t)(declared >> 24);
    in[8] = (uint8_t)((strlen(msg) - 1) & 0x0F);
    memcpy(in + 9, msg, strlen(msg));

    uint8_t out[256];
    size_t  out_len = 0;
    int rc = vp_decompress_hc01(in, in_len, out, sizeof(out), &out_len);
    printf("HC01 %s rc=%d out_len=%zu match=%d\n",
           msg, rc, out_len, rc == VP_OK && out_len == strlen(msg)
                && memcmp(out, msg, out_len) == 0);
    free(in);
}

int main(void)
{
    round_trip_lz4x("VaultPack");
    round_trip_lz4x("LZ4X block decoder demo");
    round_trip_hc01("HC01");
    return 0;
}