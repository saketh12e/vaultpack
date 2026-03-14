/*
 * src/stream.c
 *
 * Implementation of the bounded read cursor. The stream never grows the
 * borrower's input; it only mutates its position counter and error flag.
 * The scratch pool is the only thing this module allocates.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/stream.h"

#define VP_STREAM_INITIAL_SCRATCH 64

const char *vp_stream_status_str(vp_stream_status_t st)
{
    switch (st) {
    case VP_STREAM_OK:           return "ok";
    case VP_STREAM_ERR_TRUNCATED:return "truncated";
    case VP_STREAM_ERR_INVALID:  return "invalid";
    case VP_STREAM_ERR_BOUNDS:   return "out of bounds";
    default:                     return "unknown";
    }
}

void vp_stream_init(vp_stream_t *s, const uint8_t *data, size_t size)
{
    if (!s) return;
    s->base = data;
    s->size = size;
    s->pos = 0;
    s->status = VP_STREAM_OK;
    s->scratch.data = NULL;
    s->scratch.len = 0;
    s->scratch.cap = 0;
    s->scratch.generation = 0;
}

void vp_stream_reset(vp_stream_t *s)
{
    if (!s) return;
    s->pos = 0;
    s->status = VP_STREAM_OK;
}

void vp_stream_seek_abs(vp_stream_t *s, size_t pos)
{
    if (!s) return;
    if (pos > s->size) {
        s->status = VP_STREAM_ERR_BOUNDS;
        s->pos = s->size;
        return;
    }
    s->pos = pos;
}

size_t vp_stream_tell(const vp_stream_t *s)
{
    return s ? s->pos : 0;
}

size_t vp_stream_remaining(const vp_stream_t *s)
{
    if (!s) return 0;
    if (s->pos >= s->size) return 0;
    return s->size - s->pos;
}

int vp_stream_at_end(const vp_stream_t *s)
{
    return !s || s->pos >= s->size;
}

vp_stream_status_t vp_stream_status(const vp_stream_t *s)
{
    return s ? s->status : VP_STREAM_ERR_INVALID;
}

static int vp_stream_check(vp_stream_t *s, size_t n)
{
    if (!s) return 0;
    if (s->status != VP_STREAM_OK) return 0;
    if (n > s->size || s->pos > s->size - n) {
        s->status = VP_STREAM_ERR_TRUNCATED;
        return 0;
    }
    return 1;
}

uint8_t vp_stream_read_u8(vp_stream_t *s)
{
    if (!vp_stream_check(s, 1)) return 0;
    return s->base[s->pos++];
}

uint16_t vp_stream_read_u16(vp_stream_t *s)
{
    if (!vp_stream_check(s, 2)) return 0;
    uint16_t v = (uint16_t)s->base[s->pos]
               | ((uint16_t)s->base[s->pos + 1] << 8);
    s->pos += 2;
    return v;
}

uint32_t vp_stream_read_u32(vp_stream_t *s)
{
    if (!vp_stream_check(s, 4)) return 0;
    uint32_t v = (uint32_t)s->base[s->pos]
               | ((uint32_t)s->base[s->pos + 1] << 8)
               | ((uint32_t)s->base[s->pos + 2] << 16)
               | ((uint32_t)s->base[s->pos + 3] << 24);
    s->pos += 4;
    return v;
}

uint64_t vp_stream_read_u64(vp_stream_t *s)
{
    if (!vp_stream_check(s, 8)) return 0;
    uint64_t v = (uint64_t)vp_stream_read_u32(s)
               | ((uint64_t)vp_stream_read_u32(s) << 32);
    return v;
}

int vp_stream_read_bytes(vp_stream_t *s, uint8_t *out, size_t n)
{
    if (!out && n > 0) {
        if (s) s->status = VP_STREAM_ERR_INVALID;
        return 0;
    }
    if (!vp_stream_check(s, n)) return 0;
    if (n > 0) memcpy(out, s->base + s->pos, n);
    s->pos += n;
    return 1;
}

int vp_stream_skip(vp_stream_t *s, size_t n)
{
    return vp_stream_check(s, n) ? (s->pos += n, 1) : 0;
}

int vp_stream_slice(vp_stream_t *s, const uint8_t **out, size_t n)
{
    if (!out) {
        if (s) s->status = VP_STREAM_ERR_INVALID;
        return 0;
    }
    if (!vp_stream_check(s, n)) return 0;
    *out = s->base + s->pos;
    s->pos += n;
    return 1;
}

vp_stream_bookmark_t vp_stream_bookmark(const vp_stream_t *s)
{
    vp_stream_bookmark_t bm;
    bm.pos = s ? s->pos : 0;
    bm.status = s ? s->status : VP_STREAM_ERR_INVALID;
    return bm;
}

void vp_stream_restore(vp_stream_t *s, vp_stream_bookmark_t bm)
{
    if (!s) return;
    s->pos = bm.pos;
    s->status = bm.status;
}

int vp_stream_reserve_scratch(vp_stream_t *s, size_t need)
{
    if (!s) return 0;
    if (need <= s->scratch.cap) return 1;

    size_t new_cap = s->scratch.cap ? s->scratch.cap : VP_STREAM_INITIAL_SCRATCH;
    while (new_cap < need) {
        if (new_cap > (SIZE_MAX / 2)) {
            new_cap = need;
            break;
        }
        new_cap *= 2;
    }

    uint8_t *p = (uint8_t *)realloc(s->scratch.data, new_cap);
    if (!p) {
        s->status = VP_STREAM_ERR_INVALID;
        return 0;
    }
    if (p != s->scratch.data) {
        /* If the buffer moved, bump generation so any outstanding
         * windows can be detected as stale by the caller. */
        s->scratch.generation++;
    }
    s->scratch.data = p;
    s->scratch.cap = new_cap;
    return 1;
}

vp_stream_window_t vp_stream_window(vp_stream_t *s, size_t len)
{
    vp_stream_window_t w;
    w.ptr = NULL;
    w.len = 0;
    w.offset = 0;
    w.generation = 0;
    if (!s || len == 0) return w;
    if (!vp_stream_reserve_scratch(s, len)) return w;
    if (s->pos + len > s->size) {
        s->status = VP_STREAM_ERR_TRUNCATED;
        return w;
    }
    /* Copy bytes into scratch so the window survives any later realloc. */
    memcpy(s->scratch.data, s->base + s->pos, len);
    s->pos += len;
    w.ptr = s->scratch.data;
    w.len = len;
    w.offset = 0;
    w.generation = s->scratch.generation;
    s->scratch.len = len;
    return w;
}

int vp_stream_replay_window(vp_stream_t *s, const vp_stream_window_t *win,
                            uint8_t *out, size_t out_len)
{
    if (!s || !win || !win->ptr) return 0;
    if (win->generation != s->scratch.generation) {
        s->status = VP_STREAM_ERR_INVALID;
        return 0;
    }
    if (out_len < win->len) return 0;
    memcpy(out, win->ptr, win->len);
    return 1;
}
