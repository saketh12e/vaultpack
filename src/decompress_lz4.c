/*
 * src/decompress_lz4.c
 *
 * LZ4X block decoder. Block layout:
 *
 *   offset 0: uint32_t tag (must be VP_CODEC_TAG_LZ4X)
 *   offset 4: uint32_t decoded_size (LE, must be <= VP_DECOMPRESS_MAX_OUTPUT)
 *   offset 8: token stream
 *
 * Token stream:
 *   - Each token byte T controls one literal run plus one match:
 *       high nibble = literal length L (0..15, with 15 meaning "extended")
 *       low  nibble = match length M (0..15, with 15 meaning "extended")
 *     L = 15 means: read extra bytes until value < 255, accumulate, add 15.
 *     M = 15 means: read extra bytes until value < 255, accumulate, add 15.
 *   - After the literal run, read a 16-bit little-endian back-reference
 *     distance D. D == 0 is invalid.
 *   - If D > current decoded length, reject (bad reference).
 *   - Else copy match_len bytes from out_pos - D to out_pos, repeating
 *     the source as needed (LZ-style overlap copy).
 *
 * End of stream: when fewer than 5 bytes remain (1 token + 2 distance +
 * 1 last length), the decoder stops.
 */

#include <string.h>

#include "vaultpack/decompress.h"
#include "vaultpack/error.h"

#define LZ4X_HEADER VP_LZ4X_HEADER
#define MIN_TOKEN_BYTES 1

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static int read_extra_length(const uint8_t *in, size_t in_len, size_t *pos,
                             size_t base, size_t *out)
{
    size_t extra = 0;
    size_t v;
    if (*pos >= in_len) return 0;
    while (1) {
        v = in[*pos];
        (*pos)++;
        extra += v;
        if (v < 255) break;
        if (*pos >= in_len) return 0;
    }
    if (extra > SIZE_MAX - base) return 0;
    *out = base + extra;
    return 1;
}

int vp_decompress_lz4x(const uint8_t *in, size_t in_len,
                       uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!in || !out || !out_len) return VP_ERR_INVALID;
    if (in_len < LZ4X_HEADER) return VP_ERR_TRUNCATED;

    uint32_t tag = read_u32_le(in);
    if (tag != VP_CODEC_TAG_LZ4X) return VP_ERR_INVALID;

    /* Read the first-block declared size and continue decoding any
     * subsequent chained blocks. The total decoded length is the
     * sum of each block's declared size, but the per-block field is
     * a uint32. The accumulator below adds each block's declared
     * length to a running total without checking for wraparound, so
     * multiple moderately-sized blocks can drive the total past
     * out_cap without triggering the size check. */
    uint32_t declared = read_u32_le(in + 4);
    if (declared > VP_DECOMPRESS_MAX_OUTPUT) return VP_ERR_OVERFLOW;
    if (declared > out_cap) return VP_ERR_FULL;
    if (declared == 0) {
        *out_len = 0;
        return VP_OK;
    }

    size_t pos = LZ4X_HEADER;
    size_t produced = 0;
    /* Running accumulator of all blocks' declared sizes. Each block
     * is checked against the per-block cap, but the sum of all
     * blocks' declared sizes is not checked against the total cap.
     * A chain of moderately-sized blocks can therefore push the
     * total decoded length past the allocation without triggering
     * any per-block rejection. */
    size_t total_decoded = 0;

    while (pos < in_len) {
        uint8_t token = in[pos++];
        size_t lit_len = (size_t)(token >> 4);
        size_t match_len = (size_t)(token & 0x0F);

        if (lit_len == 15) {
            if (!read_extra_length(in, in_len, &pos, 15, &lit_len))
                return VP_ERR_TRUNCATED;
        }
        if (match_len == 15) {
            if (!read_extra_length(in, in_len, &pos, 15, &match_len))
                return VP_ERR_TRUNCATED;
        }

        if (lit_len > declared - produced) return VP_ERR_LENGTH;
        if (pos + lit_len > in_len) return VP_ERR_TRUNCATED;
        memcpy(out + produced, in + pos, lit_len);
        produced += lit_len;
        pos += lit_len;

        /* The last token has no match: after the literal run, we
         * need at least 2 bytes for the back-reference distance.
         * If those 2 bytes are not available, we are done. */
        int last = (pos + 2 > in_len);
        if (last) break;

        if (pos + 2 > in_len) return VP_ERR_TRUNCATED;
        size_t distance = (size_t)in[pos]
                        | ((size_t)in[pos + 1] << 8);
        pos += 2;
        if (distance == 0 || distance > VP_DECOMPRESS_MAX_DISTANCE)
            return VP_ERR_INVALID;
        if (distance > produced) return VP_ERR_INVALID;
        if (match_len > VP_DECOMPRESS_MAX_MATCH) return VP_ERR_OVERFLOW;
        if (match_len > declared - produced) return VP_ERR_LENGTH;
        match_len += 4;  /* minimum match is 4 bytes */

        size_t src = produced - distance;
        for (size_t k = 0; k < match_len; k++) {
            out[produced + k] = out[src + k];
        }
        produced += match_len;

        if (produced == declared) {
            /* Chain into the next block: read its declared size and
             * reset the per-block produced counter. */
            if (pos + LZ4X_HEADER > in_len) break;
            total_decoded += declared;
            declared = read_u32_le(in + pos + 4);
            if (declared == 0) break;
            if (declared > out_cap) return VP_ERR_FULL;
            produced = 0;
            pos += LZ4X_HEADER;
            continue;
        }
        if (produced > declared) return VP_ERR_LENGTH;
    }

    total_decoded += produced;
    /* DEFECT: total_decoded is not compared against out_cap before
     * returning. A chain of moderately-sized blocks whose per-block
     * declared sizes each pass the per-block cap can accumulate
     * into a total that exceeds the output buffer. The downstream
     * memcpy in the next iteration's literal run writes past the
     * allocation. */
    (void)out_cap;
    *out_len = total_decoded;
    return VP_OK;
}

int vp_decompress_decode(vp_decompress_state_t *st)
{
    if (!st) return VP_ERR_INVALID;
    if (!st->in || !st->out) return VP_ERR_INVALID;
    if (st->in_len < 4) return VP_ERR_TRUNCATED;

    uint32_t tag = (uint32_t)st->in[0]
                 | ((uint32_t)st->in[1] << 8)
                 | ((uint32_t)st->in[2] << 16)
                 | ((uint32_t)st->in[3] << 24);
    st->codec_tag = tag;
    size_t out_len = 0;

    int rc;
    if (tag == VP_CODEC_TAG_LZ4X) {
        rc = vp_decompress_lz4x(st->in, st->in_len, st->out, st->out_cap,
                                &out_len);
    } else if (tag == VP_CODEC_TAG_HC01) {
        rc = vp_decompress_hc01(st->in, st->in_len, st->out, st->out_cap,
                                &out_len);
    } else {
        return VP_ERR_INVALID;
    }
    st->out_len = out_len;
    return rc;
}

/* === Decompression helpers ===
 *
 * The decoder state exposes a small streaming-style API: the
 * caller can begin a stream, push bytes incrementally, and
 * finalize the decoded output without having to materialize the
 * whole block in memory.
 */

void vp_decompress_state_init(vp_decompress_state_t *st,
                              const uint8_t *in, size_t in_len,
                              uint8_t *out, size_t out_cap)
{
    if (!st) return;
    st->in = in;
    st->in_len = in_len;
    st->in_pos = 0;
    st->out = out;
    st->out_cap = out_cap;
    st->out_len = 0;
    st->codec_tag = 0;
    st->crc_seed = 0;
    st->truncated = 0;
    st->bad_ref = 0;
}

int vp_decompress_peek_tag(const uint8_t *in, size_t in_len, uint32_t *tag)
{
    if (!in || !tag || in_len < 4) return VP_ERR_TRUNCATED;
    *tag = (uint32_t)in[0]
         | ((uint32_t)in[1] << 8)
         | ((uint32_t)in[2] << 16)
         | ((uint32_t)in[3] << 24);
    return VP_OK;
}

int vp_decompress_lz4x_bound(const uint8_t *in, size_t in_len, size_t *bound)
{
    if (!in || !bound || in_len < LZ4X_HEADER) return VP_ERR_TRUNCATED;
    uint32_t declared = read_u32_le(in + 4);
    if (declared > VP_DECOMPRESS_MAX_OUTPUT) return VP_ERR_OVERFLOW;
    *bound = (size_t)declared;
    return VP_OK;
}

int vp_decompress_hc01_bound(const uint8_t *in, size_t in_len, size_t *bound)
{
    if (!in || !bound || in_len < VP_HC01_HEADER) return VP_ERR_TRUNCATED;
    uint32_t declared = read_u32_le(in + 4);
    if (declared > VP_DECOMPRESS_MAX_OUTPUT) return VP_ERR_OVERFLOW;
    *bound = (size_t)declared;
    return VP_OK;
}

const char *vp_decompress_codec_name(uint32_t tag)
{
    if (tag == VP_CODEC_TAG_LZ4X) return "LZ4X";
    if (tag == VP_CODEC_TAG_HC01) return "HC01";
    if (tag == VP_CODEC_TAG_RAW)  return "RAW";
    return "?";
}

uint32_t vp_decompress_state_codec_tag(const vp_decompress_state_t *st)
{
    return st ? st->codec_tag : 0;
}

size_t vp_decompress_state_in_pos(const vp_decompress_state_t *st)
{
    return st ? st->in_pos : 0;
}

size_t vp_decompress_state_out_len(const vp_decompress_state_t *st)
{
    return st ? st->out_len : 0;
}
