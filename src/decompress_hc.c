/*
 * src/decompress_hc.c
 *
 * HC01 high-compression block decoder. Block layout:
 *
 *   offset 0: uint32_t tag (must be VP_CODEC_TAG_HC01)
 *   offset 4: uint32_t decoded_size (LE)
 *   offset 8: token stream
 *
 * Token stream:
 *   - Each token byte T:
 *       high nibble = code class (0 = literal, 1 = repeat-short, 2 = repeat-long)
 *       low  nibble = length minus 1 (0..15)
 *     Literal class: T = 0x00..0x0F, next byte is the literal value.
 *     Repeat-short: T = 0x10..0x1F, copies the previous output byte
 *       (length + 1) times.
 *     Repeat-long:  T = 0x20..0x2F, next two bytes are a length-minus-1
 *       extension (LE), then (length + 1) bytes of fill data.
 *
 * Decoded length must equal the declared size.
 */

#include <string.h>

#include "vaultpack/decompress.h"
#include "vaultpack/error.h"

#define HC01_HEADER 8

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

int vp_decompress_hc01(const uint8_t *in, size_t in_len,
                       uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!in || !out || !out_len) return VP_ERR_INVALID;
    if (in_len < HC01_HEADER) return VP_ERR_TRUNCATED;

    uint32_t tag = read_u32_le(in);
    if (tag != VP_CODEC_TAG_HC01) return VP_ERR_INVALID;

    uint32_t declared = read_u32_le(in + 4);
    if (declared > VP_DECOMPRESS_MAX_OUTPUT) return VP_ERR_OVERFLOW;
    if (declared > out_cap) return VP_ERR_FULL;

    size_t pos = HC01_HEADER;
    size_t produced = 0;

    while (pos < in_len && produced < declared) {
        uint8_t t = in[pos++];
        uint8_t cls = (uint8_t)(t >> 4);
        size_t base = (size_t)(t & 0x0F) + 1;

        if (cls == 0) {
            /* Literal: read one byte, emit base copies. */
            if (pos >= in_len) return VP_ERR_TRUNCATED;
            uint8_t v = in[pos++];
            if (produced + base > declared) return VP_ERR_LENGTH;
            if (produced + base > out_cap) return VP_ERR_FULL;
            memset(out + produced, v, base);
            produced += base;
        } else if (cls == 1) {
            /* Repeat-short: copy previous byte base times. */
            if (produced == 0) return VP_ERR_INVALID;
            if (produced + base > declared) return VP_ERR_LENGTH;
            if (produced + base > out_cap) return VP_ERR_FULL;
            uint8_t v = out[produced - 1];
            memset(out + produced, v, base);
            produced += base;
        } else if (cls == 2) {
            /* Repeat-long: read length extension (LE u16), then fill
             * bytes follow inline. */
            if (pos + 2 > in_len) return VP_ERR_TRUNCATED;
            uint32_t ext = (uint32_t)in[pos]
                         | ((uint32_t)in[pos + 1] << 8);
            pos += 2;
            size_t total = base + (size_t)ext * 16u;
            if (pos + total > in_len) return VP_ERR_TRUNCATED;
            if (produced + total > declared) return VP_ERR_LENGTH;
            if (produced + total > out_cap) return VP_ERR_FULL;
            memcpy(out + produced, in + pos, total);
            pos += total;
            produced += total;
        } else {
            return VP_ERR_INVALID;
        }
    }

    if (produced != declared) return VP_ERR_LENGTH;
    *out_len = produced;
    return VP_OK;
}
