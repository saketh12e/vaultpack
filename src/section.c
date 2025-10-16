#include <stdlib.h>
#include <string.h>

#include "vaultpack/section.h"
#include "vaultpack/error.h"

int vp_parse_section_header(const uint8_t *data, size_t size,
                            vp_section_t *section)
{
    if (!section || !data || size < sizeof(vp_section_hdr_t))
        return VP_ERR_TRUNCATED;

    memset(section, 0, sizeof(*section));
    memcpy(&section->hdr, data, sizeof(section->hdr));

    if (memcmp(&section->hdr.magic, VP_MAGIC_SECT_BYTES, 4) != 0)
        return VP_ERR_INVALID;

    section->type  = section->hdr.type;
    section->flags = section->hdr.flags;

    if (section->hdr.type == VP_SECTION_COMPRESSED) {
        section->comp_ctx = calloc(1, sizeof(vp_comp_ctx_t));
        if (!section->comp_ctx)
            return VP_ERR_NOMEM;
        section->comp_ctx->method = section->hdr.comp_method;
        section->comp_ctx->level  = section->hdr.comp_level;
    }

    return VP_OK;
}
