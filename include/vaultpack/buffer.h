/*
 * include/vaultpack/buffer.h
 *
 * Growable byte buffer used by the archive writer and any code that needs
 * to emit bytes incrementally. Reserves grow geometrically. Patching
 * previously written offsets allows the writer to back-fill the central
 * directory after sections have been emitted.
 */
#ifndef VAULTPACK_BUFFER_H
#define VAULTPACK_BUFFER_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VP_BUFFER_INIT_CAP  32

typedef struct vp_buffer {
    uint8_t *data;
    size_t   len;
    size_t   cap;
    int      owns;        /* 1 = we malloc/free, 0 = borrowed */
    int      oom;         /* sticky if a reserve ever fails */
} vp_buffer_t;

void  vp_buffer_init(vp_buffer_t *b);
int   vp_buffer_reserve(vp_buffer_t *b, size_t need);
int   vp_buffer_resize(vp_buffer_t *b, size_t new_len);
int   vp_buffer_append(vp_buffer_t *b, const void *src, size_t n);
int   vp_buffer_append_byte(vp_buffer_t *b, uint8_t v);
int   vp_buffer_append_u16_le(vp_buffer_t *b, uint16_t v);
int   vp_buffer_append_u32_le(vp_buffer_t *b, uint32_t v);
int   vp_buffer_append_u64_le(vp_buffer_t *b, uint64_t v);
int   vp_buffer_append_zeros(vp_buffer_t *b, size_t n);
int   vp_buffer_patch_u32_le(vp_buffer_t *b, size_t offset, uint32_t v);
int   vp_buffer_patch_u16_le(vp_buffer_t *b, size_t offset, uint16_t v);
int   vp_buffer_patch_u64_le(vp_buffer_t *b, size_t offset, uint64_t v);
void  vp_buffer_reset(vp_buffer_t *b);
void  vp_buffer_destroy(vp_buffer_t *b);

/* Detach ownership of the byte array from the buffer. The caller takes
 * responsibility for free(). After detach, the buffer is reset to empty. */
uint8_t *vp_buffer_detach(vp_buffer_t *b, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif
