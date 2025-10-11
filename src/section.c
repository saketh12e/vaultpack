#include <string.h>

#include "vaultpack/section.h"
#include "vaultpack/error.h"

int vp_parse_section_header(const uint8_t *data, size_t size,
                            vp_section_t *section);

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
    return VP_OK;
}
