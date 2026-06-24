/*
 * include/vaultpack/stream.h
 *
 * Bounded read cursor for little-endian archive parsing. The stream never
 * owns the underlying buffer; it borrows a (data, size) window and tracks
 * a read position. Out-of-bounds reads set an error state and return zero
 * values, so callers can keep parsing on malformed input.
 */
#ifndef VAULTPACK_STREAM_H
#define VAULTPACK_STREAM_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum vp_stream_status {
    VP_STREAM_OK = 0,
    VP_STREAM_ERR_TRUNCATED,
    VP_STREAM_ERR_INVALID,
    VP_STREAM_ERR_BOUNDS
} vp_stream_status_t;

/*
 * Scratch pool: a stream can also use an internal scratch buffer for
 * transient views (windows). Windows are borrowed pointers into the
 * scratch buffer; if the scratch buffer is later grown, those pointers
 * become invalid. Callers who need long-lived views must copy.
 */
typedef struct vp_stream_scratch {
    uint8_t *data;
    size_t   len;
    size_t   cap;
    uint32_t generation;
} vp_stream_scratch_t;

typedef struct vp_stream_window {
    uint8_t *ptr;     /* may be NULL */
    size_t   len;
    size_t   offset;
    uint32_t generation;
} vp_stream_window_t;

typedef struct vp_stream {
    const uint8_t   *base;
    size_t           size;
    size_t           pos;
    vp_stream_status_t status;
    vp_stream_scratch_t scratch;
} vp_stream_t;

void vp_stream_init(vp_stream_t *s, const uint8_t *data, size_t size);
void vp_stream_reset(vp_stream_t *s);
void vp_stream_seek_abs(vp_stream_t *s, size_t pos);
size_t vp_stream_tell(const vp_stream_t *s);
size_t vp_stream_remaining(const vp_stream_t *s);
int vp_stream_at_end(const vp_stream_t *s);
vp_stream_status_t vp_stream_status(const vp_stream_t *s);
const char *vp_stream_status_str(vp_stream_status_t st);

uint8_t  vp_stream_read_u8(vp_stream_t *s);
uint16_t vp_stream_read_u16(vp_stream_t *s);
uint32_t vp_stream_read_u32(vp_stream_t *s);
uint64_t vp_stream_read_u64(vp_stream_t *s);

int vp_stream_read_bytes(vp_stream_t *s, uint8_t *out, size_t n);
int vp_stream_skip(vp_stream_t *s, size_t n);
int vp_stream_slice(vp_stream_t *s, const uint8_t **out, size_t n);

/*
 * Bookmark/restore: capture the current position and restore it later.
 * Useful for speculative parses (try, fail, rewind).
 */
typedef struct vp_stream_bookmark {
    size_t pos;
    vp_stream_status_t status;
} vp_stream_bookmark_t;

vp_stream_bookmark_t vp_stream_bookmark(const vp_stream_t *s);
void vp_stream_restore(vp_stream_t *s, vp_stream_bookmark_t bm);

/*
 * Scratch windows: borrow a region of the stream's scratch buffer. The
 * returned window is valid until the scratch buffer is grown again.
 */
int vp_stream_reserve_scratch(vp_stream_t *s, size_t need);
vp_stream_window_t vp_stream_window(vp_stream_t *s, size_t len);
int vp_stream_replay_window(vp_stream_t *s, const vp_stream_window_t *win,
                            uint8_t *out, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif
