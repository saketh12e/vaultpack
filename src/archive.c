#include <string.h>

#include "vaultpack/archive.h"
#include "vaultpack/error.h"

int vp_check_header(const uint8_t *data, size_t size)
{
    if (!data || size < VP_ARCHIVE_HDR_SIZE)
        return VP_ERR_TRUNCATED;

    if (memcmp(data, VP_MAGIC_VPAK_BYTES, 4) != 0)
        return VP_ERR_INVALID;

    uint16_t version = (uint16_t)data[4] | ((uint16_t)data[5] << 8);
    if (version != VP_ARCHIVE_VERSION)
        return VP_ERR_INVALID;

    return VP_OK;
}
