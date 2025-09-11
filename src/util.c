#include <stdint.h>

#include "vaultpack/util.h"

uint32_t vp_read_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

uint16_t vp_read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

uint64_t vp_read_le64(const uint8_t *p)
{
    uint64_t lo = (uint64_t)vp_read_le32(p);
    uint64_t hi = (uint64_t)vp_read_le32(p + 4);
    return lo | (hi << 32);
}
