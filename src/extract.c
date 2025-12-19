#include <stdlib.h>
#include <string.h>

#include "vaultpack/extract.h"
#include "vaultpack/section.h"
#include "vaultpack/error.h"

int vp_extract_section(vp_extract_ctx_t *ctx, const uint8_t *data,
                       size_t size)
{
    if (!ctx || !data)
        return VP_ERR_INVALID;

    ctx->out_buf = (uint8_t *)malloc(ctx->out_size);
    if (!ctx->out_buf)
        return VP_ERR_NOMEM;

    vp_section_t sec;
    int rc = vp_parse_section_header(data, size, &sec);
    if (rc != VP_OK) {
        free(ctx->out_buf);
        return rc;
    }

    sec.data = ctx->out_buf;

    rc = vp_fill_section_data(&sec,
                              data + sizeof(vp_section_hdr_t),
                              size > sizeof(vp_section_hdr_t)
                                  ? size - sizeof(vp_section_hdr_t)
                                  : 0);
    if (rc != VP_OK) {
        vp_section_cleanup(&sec);
        return rc;
    }

    ctx->out_len = sec.data_len;
    return VP_OK;
}

void vp_extract_cleanup(vp_extract_ctx_t *ctx)
{
    if (!ctx)
        return;
    if (ctx->out_buf) {
        free(ctx->out_buf);
        ctx->out_buf = NULL;
    }
    ctx->out_len = 0;
}
