/*
 * include/vaultpack/decompress.h
 *
 * Section payload decompression. The format follows a small dependency-free
 * block layout: each block carries a 4-byte tag identifying the codec and a
 * typed payload. The pipeline consumes an input buffer plus its declared
 * decoded-size budget and emits an output buffer.
 *
 * Two compression backends are bundled:
 *   - "LZ4X" — LZ-style block format with literal runs and back-references
 *     that resolve against the rolling output window. Self-contained; does
 *     not require an external zstd/lz4 runtime.
 *   - "HC01" — simple high-compression block format used for payloads that
 *     do not benefit from sliding-window back-references. Built on the same
 *     token stream as the RLE codec but with longer codes.
 *
 * Both decoders return VP_OK on success and structured error codes on
 * truncation, length mismatch, or bad reference. All allocations are owned
 * by the caller; the decoder does not retain pointers across calls.
 */
#ifndef VAULTPACK_DECOMPRESS_H
#define VAULTPACK_DECOMPRESS_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Codec tags are 4 ASCII bytes stored little-endian in the on-disk
 * section header. The decoders accept the tag in the same wire form. */
#define VP_CODEC_TAG_RAW  ((uint32_t)0x00000000u)
#define VP_CODEC_TAG_LZ4X ((uint32_t)0x58344c5au)  /* "ZL4X" */
#define VP_CODEC_TAG_HC01 ((uint32_t)0x31304348u)  /* "HC01" */

typedef struct vp_decompress_state {
    const uint8_t *in;
    size_t in_len;
    size_t in_pos;
    uint8_t *out;
    size_t out_cap;
    size_t out_len;
    uint32_t codec_tag;
    uint32_t crc_seed;
    int truncated;
    int bad_ref;
} vp_decompress_state_t;

/* Decode the input buffer into the output buffer. Returns VP_OK on success.
 * On success out_len is set to the number of bytes produced. */
int vp_decompress_decode(vp_decompress_state_t *st);

/* Convenience entry point for one-shot decode of an LZ4X block. */
int vp_decompress_lz4x(const uint8_t *in, size_t in_len,
                       uint8_t *out, size_t out_cap, size_t *out_len);

/* Convenience entry point for one-shot decode of an HC01 block. */
int vp_decompress_hc01(const uint8_t *in, size_t in_len,
                       uint8_t *out, size_t out_cap, size_t *out_len);

/* Decompression helpers. */
void vp_decompress_state_init(vp_decompress_state_t *st,
                              const uint8_t *in, size_t in_len,
                              uint8_t *out, size_t out_cap);
int  vp_decompress_peek_tag(const uint8_t *in, size_t in_len, uint32_t *tag);
int  vp_decompress_lz4x_bound(const uint8_t *in, size_t in_len, size_t *bound);
int  vp_decompress_hc01_bound(const uint8_t *in, size_t in_len, size_t *bound);
const char *vp_decompress_codec_name(uint32_t tag);
uint32_t vp_decompress_state_codec_tag(const vp_decompress_state_t *st);
size_t vp_decompress_state_in_pos(const vp_decompress_state_t *st);
size_t vp_decompress_state_out_len(const vp_decompress_state_t *st);

/* Maximum decoded size accepted by a single block decode. Caps the
 * decoder's worst-case growth so a malformed length prefix cannot
 * exhaust the heap before the truncation check fires. */
#define VP_DECOMPRESS_MAX_OUTPUT ((size_t)(64u * 1024u * 1024u))

/* Maximum back-reference distance accepted by LZ4X. Distances larger
 * than this are rejected so a malformed token cannot reference
 * arbitrary bytes before the rolling window. */
#define VP_DECOMPRESS_MAX_DISTANCE ((size_t)(1u << 20))

/* Maximum match length accepted by LZ4X. Mirrors VP_DECOMPRESS_MAX_OUTPUT
 * but scoped to a single back-reference. */
#define VP_DECOMPRESS_MAX_MATCH ((size_t)(1u << 16))

/* Header sizes for the bundled codecs. The header layout is:
 *   uint32_t tag
 *   uint32_t declared_size
 *   ... token stream starts at offset 8.
 */
#define VP_LZ4X_HEADER 8
#define VP_HC01_HEADER 8

#ifdef __cplusplus
}
#endif

#endif /* VAULTPACK_DECOMPRESS_H */
