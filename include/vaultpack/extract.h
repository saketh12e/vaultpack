#ifndef VAULTPACK_EXTRACT_H
#define VAULTPACK_EXTRACT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vp_extract_ctx {
    uint8_t *out_buf;
    size_t   out_size;
    size_t   out_len;
} vp_extract_ctx_t;

int vp_extract_section(vp_extract_ctx_t *ctx, const uint8_t *data,
                       size_t size);

#ifdef __cplusplus
}
#endif

#endif
