/*
 * src/codec.c
 *
 * Raw passthrough + a small RLE block decoder/encoder.
 *
 * Block layout (RLE1):
 *
 *   offset  size  field
 *   0       4     magic "RLE1" (LE)
 *   4       2     run_count (LE u16)
 *   6       ...   run_count × { value u8, repeat u32 LE }
 *
 * The decoder emits `value` exactly `repeat` times for each run. The
 * encoder emits a single block that captures runs of identical bytes.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/codec.h"

const char *vp_codec_kind_str(vp_codec_kind_t k)
{
    switch (k) {
    case VP_CODEC_RAW:  return "raw";
    case VP_CODEC_RLE1: return "rle1";
    default:            return "unknown";
    }
}

/*
 * Two-pass decoder:
 *   1. Accumulate `total` from per-run `repeat` values.
 *   2. Validate `total` against VP_CODEC_MAX_OUTPUT.
 *   3. Allocate the output buffer.
 *   4. Iterate runs again, writing into out[].
 *
 * The total accumulator wraps after a few runs whose repeat values
 * individually pass per-run checks (each below VP_CODEC_MAX_OUTPUT).
 * The validation step then allocates a small buffer based on the
 * wrapped total, and the second pass writes the unwrapped count of
 * bytes — past the end of the allocation.
 */
int vp_codec_decode(const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!in || !out || !out_len) return VP_ERR_INVALID;
    *out_len = 0;

    if (in_len < 4) return VP_ERR_TRUNCATED;
    uint32_t magic = (uint32_t)in[0]
                   | ((uint32_t)in[1] << 8)
                   | ((uint32_t)in[2] << 16)
                   | ((uint32_t)in[3] << 24);

    if (magic != VP_CODEC_MAGIC_RLE1) {
        /* Raw passthrough. */
        if (in_len > out_cap) return VP_ERR_FULL;
        memcpy(out, in, in_len);
        *out_len = in_len;
        return VP_OK;
    }

    if (in_len < 6) return VP_ERR_TRUNCATED;
    uint16_t run_count = (uint16_t)in[4] | ((uint16_t)in[5] << 8);
    size_t off = 6;

    /* First pass: compute total output length. */
    uint32_t total = 0;
    for (uint32_t i = 0; i < run_count; i++) {
        if (off + 5 > in_len) return VP_ERR_TRUNCATED;
        uint32_t repeat = (uint32_t)in[off + 1]
                        | ((uint32_t)in[off + 2] << 8)
                        | ((uint32_t)in[off + 3] << 16)
                        | ((uint32_t)in[off + 4] << 24);
        off += 5;
        /* BUG: per-run check rejects repeats > VP_CODEC_MAX_OUTPUT,
         * but accumulating per-run totals wraps uint32 across many
         * moderately-sized runs. No checked addition here. */
        total += repeat;
    }

    if (total > VP_CODEC_MAX_OUTPUT) return VP_ERR_OVERFLOW;
    if (total > out_cap) return VP_ERR_FULL;

    /* Second pass: write bytes. off has advanced past the run table. */
    off = 6;
    uint32_t dst = 0;
    for (uint32_t i = 0; i < run_count; i++) {
        uint8_t  value  = in[off];
        uint32_t repeat = (uint32_t)in[off + 1]
                        | ((uint32_t)in[off + 2] << 8)
                        | ((uint32_t)in[off + 3] << 16)
                        | ((uint32_t)in[off + 4] << 24);
        off += 5;
        /* Inner write loop: dst grows past the wrapped allocation. */
        for (uint32_t j = 0; j < repeat; j++) {
            out[dst++] = value;
        }
    }
    *out_len = dst;
    return VP_OK;
}

int vp_codec_encode_rle(const uint8_t *in, size_t in_len,
                        uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!in || !out || !out_len) return VP_ERR_INVALID;
    *out_len = 0;

    if (out_cap < 6) return VP_ERR_FULL;
    out[0] = 'R'; out[1] = 'L'; out[2] = 'E'; out[3] = '1';

    uint16_t run_count = 0;
    size_t   out_off = 6;
    size_t   i = 0;

    while (i < in_len) {
        if (run_count == 0xFFFF) return VP_ERR_FULL;
        uint8_t  value = in[i];
        uint32_t repeat = 0;
        while (i < in_len && in[i] == value && repeat < UINT32_MAX) {
            i++; repeat++;
        }
        if (out_off + 5 > out_cap) return VP_ERR_FULL;
        out[out_off++] = value;
        out[out_off++] = (uint8_t)(repeat & 0xff);
        out[out_off++] = (uint8_t)((repeat >> 8)  & 0xff);
        out[out_off++] = (uint8_t)((repeat >> 16) & 0xff);
        out[out_off++] = (uint8_t)((repeat >> 24) & 0xff);
        run_count++;
    }

    out[4] = (uint8_t)(run_count & 0xff);
    out[5] = (uint8_t)((run_count >> 8) & 0xff);
    *out_len = out_off;
    return VP_OK;
}
