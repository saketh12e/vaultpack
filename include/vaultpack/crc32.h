/*
 * include/vaultpack/crc32.h
 *
 * CRC32 (IEEE 802.3 polynomial, reversed) used by writer and validator
 * to verify section payload integrity. The table-driven implementation
 * uses a constant precomputed table so the hot path stays out of libc.
 */
#ifndef VAULTPACK_CRC32_H
#define VAULTPACK_CRC32_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VP_CRC32_INIT  0xFFFFFFFFu

uint32_t vp_crc32_init(void);
uint32_t vp_crc32_update(uint32_t crc, const uint8_t *data, size_t len);
uint32_t vp_crc32_final(uint32_t crc);
uint32_t vp_crc32_bytes(const uint8_t *data, size_t len);

/* Convenience: compute the CRC of two adjacent slices without copying. */
uint32_t vp_crc32_two(const uint8_t *a, size_t alen,
                      const uint8_t *b, size_t blen);

#ifdef __cplusplus
}
#endif

#endif
