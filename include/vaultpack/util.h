#ifndef VAULTPACK_UTIL_H
#define VAULTPACK_UTIL_H

#include <stdint.h>
#include <stddef.h>

uint32_t vp_read_le32(const uint8_t *p);
uint64_t vp_read_le64(const uint8_t *p);
uint16_t vp_read_le16(const uint8_t *p);

int vp_checked_add_u32(uint32_t a, uint32_t b, uint32_t *out);

#endif
