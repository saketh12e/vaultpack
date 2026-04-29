/*
 * include/vaultpack/repair.h
 *
 * Archive repair for partially truncated or damaged archives. The
 * repair scanner walks the archive byte-by-byte looking for valid
 * SECT headers and records each one it finds. Sections whose
 * declared payload does not fit within the archive boundary are
 * skipped.
 */
#ifndef VAULTPACK_REPAIR_H
#define VAULTPACK_REPAIR_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vp_repair_record {
    uint64_t offset;
    uint32_t raw_size;
    uint32_t comp_size;
    uint32_t crc;
    uint32_t flags;
} vp_repair_record_t;

typedef struct vp_repair_ctx {
    const uint8_t        *blob;
    size_t                blob_len;
    vp_repair_record_t   *records;
    uint32_t              count;
    uint32_t              cap;
} vp_repair_ctx_t;

void vp_repair_ctx_init(vp_repair_ctx_t *r, const uint8_t *blob, size_t len);
void vp_repair_ctx_destroy(vp_repair_ctx_t *r);

int  vp_repair_scan(vp_repair_ctx_t *r);

/* Test entry point used by the bridge. */
int vp_archive_repair_run(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* VAULTPACK_REPAIR_H */