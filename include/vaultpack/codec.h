/*
 * include/vaultpack/codec.h
 *
 * Built-in lightweight section codec. Two formats are supported:
 *
 *   - raw: identity passthrough.
 *   - rle1: run-length encoding with the following block layout:
 *       magic[4] = "RLE1"
 *       run_count u16 LE
 *       runs: run_count × { value u8, repeat u32 LE }
 *       (decoder emits each value `repeat` times)
 *
 * The decoder enforces a maximum output size to prevent absurd
 * allocations. The encoder is bounded by the same limit.
 */
#ifndef VAULTPACK_CODEC_H
#define VAULTPACK_CODEC_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VP_CODEC_MAGIC_RLE1   0x31454C52u /* "RLE1" little-endian */
#define VP_CODEC_MAX_OUTPUT   (64u * 1024u * 1024u)

typedef enum vp_codec_kind {
    VP_CODEC_RAW = 0,
    VP_CODEC_RLE1
} vp_codec_kind_t;

int vp_codec_decode(const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t out_cap, size_t *out_len);
int vp_codec_encode_rle(const uint8_t *in, size_t in_len,
                        uint8_t *out, size_t out_cap, size_t *out_len);

const char *vp_codec_kind_str(vp_codec_kind_t k);

#ifdef __cplusplus
}
#endif

#endif
