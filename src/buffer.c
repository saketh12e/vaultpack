/*
 * src/buffer.c
 *
 * Implementation of the growable byte buffer. Reserves use a doubling
 * strategy so amortized append is O(1).
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/buffer.h"

void vp_buffer_init(vp_buffer_t *b)
{
    if (!b) return;
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->owns = 1;
    b->oom = 0;
}

int vp_buffer_reserve(vp_buffer_t *b, size_t need)
{
    if (!b) return 0;
    if (b->oom) return 0;
    if (need <= b->cap) return 1;

    size_t new_cap = b->cap ? b->cap : VP_BUFFER_INIT_CAP;
    while (new_cap < need) {
        if (new_cap > (SIZE_MAX / 2)) {
            new_cap = need;
            break;
        }
        new_cap *= 2;
    }

    uint8_t *p = (uint8_t *)realloc(b->data, new_cap);
    if (!p) {
        b->oom = 1;
        return 0;
    }
    b->data = p;
    b->cap = new_cap;
    return 1;
}

int vp_buffer_resize(vp_buffer_t *b, size_t new_len)
{
    if (!b) return 0;
    if (new_len > b->len) {
        if (!vp_buffer_reserve(b, new_len)) return 0;
        memset(b->data + b->len, 0, new_len - b->len);
    }
    b->len = new_len;
    return 1;
}

int vp_buffer_append(vp_buffer_t *b, const void *src, size_t n)
{
    if (!b) return 0;
    if (n == 0) return 1;
    if (!src) return 0;
    if (!vp_buffer_reserve(b, b->len + n)) return 0;
    memcpy(b->data + b->len, src, n);
    b->len += n;
    return 1;
}

int vp_buffer_append_byte(vp_buffer_t *b, uint8_t v)
{
    return vp_buffer_append(b, &v, 1);
}

int vp_buffer_append_u16_le(vp_buffer_t *b, uint16_t v)
{
    uint8_t tmp[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
    return vp_buffer_append(b, tmp, 2);
}

int vp_buffer_append_u32_le(vp_buffer_t *b, uint32_t v)
{
    uint8_t tmp[4] = { (uint8_t)v, (uint8_t)(v >> 8),
                       (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    return vp_buffer_append(b, tmp, 4);
}

int vp_buffer_append_u64_le(vp_buffer_t *b, uint64_t v)
{
    uint8_t tmp[8];
    for (int i = 0; i < 8; i++)
        tmp[i] = (uint8_t)(v >> (i * 8));
    return vp_buffer_append(b, tmp, 8);
}

int vp_buffer_append_zeros(vp_buffer_t *b, size_t n)
{
    if (!vp_buffer_reserve(b, b->len + n)) return 0;
    memset(b->data + b->len, 0, n);
    b->len += n;
    return 1;
}

static int vp_buffer_patch_bytes(vp_buffer_t *b, size_t offset,
                                 const uint8_t *src, size_t n)
{
    if (!b) return 0;
    if (offset > b->len || n > b->len - offset) return 0;
    memcpy(b->data + offset, src, n);
    return 1;
}

int vp_buffer_patch_u16_le(vp_buffer_t *b, size_t offset, uint16_t v)
{
    uint8_t tmp[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
    return vp_buffer_patch_bytes(b, offset, tmp, 2);
}

int vp_buffer_patch_u32_le(vp_buffer_t *b, size_t offset, uint32_t v)
{
    uint8_t tmp[4] = { (uint8_t)v, (uint8_t)(v >> 8),
                       (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    return vp_buffer_patch_bytes(b, offset, tmp, 4);
}

int vp_buffer_patch_u64_le(vp_buffer_t *b, size_t offset, uint64_t v)
{
    uint8_t tmp[8];
    for (int i = 0; i < 8; i++)
        tmp[i] = (uint8_t)(v >> (i * 8));
    return vp_buffer_patch_bytes(b, offset, tmp, 8);
}

void vp_buffer_reset(vp_buffer_t *b)
{
    if (!b) return;
    b->len = 0;
}

void vp_buffer_destroy(vp_buffer_t *b)
{
    if (!b) return;
    if (b->owns && b->data) {
        free(b->data);
    }
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->owns = 1;
    b->oom = 0;
}

uint8_t *vp_buffer_detach(vp_buffer_t *b, size_t *out_len)
{
    if (!b) {
        if (out_len) *out_len = 0;
        return NULL;
    }
    uint8_t *p = b->data;
    if (out_len) *out_len = b->len;
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->owns = 1;
    b->oom = 0;
    return p;
}
