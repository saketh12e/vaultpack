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

/* Repair advanced operations. */
uint32_t vp_repair_count(const vp_repair_ctx_t *r);
int vp_repair_at(const vp_repair_ctx_t *r, uint32_t idx, vp_repair_record_t *out);
uint32_t vp_repair_total_raw(const vp_repair_ctx_t *r);
uint32_t vp_repair_total_comp(const vp_repair_ctx_t *r);
int vp_repair_find_at_offset(const vp_repair_ctx_t *r, uint64_t offset);

typedef int (*vp_repair_iter_cb)(const vp_repair_record_t *rec, void *user);

int vp_repair_iter(const vp_repair_ctx_t *r, vp_repair_iter_cb cb, void *user);
int vp_repair_emit_directory(const vp_repair_ctx_t *r,
                             uint8_t *out, size_t out_cap, size_t *out_len);
int vp_repair_rescan(vp_repair_ctx_t *r);
int vp_repair_validate(const vp_repair_ctx_t *r);

/* Full repair implementation. */
int vp_repair_scan_full(vp_repair_ctx_t *r);
int vp_repair_record_validate(const vp_repair_ctx_t *r, uint32_t idx);
int vp_repair_record_payload(const vp_repair_ctx_t *r, uint32_t idx,
                             uint8_t *out, size_t out_cap, size_t *out_len);
int vp_repair_emit_archive(const vp_repair_ctx_t *r,
                          uint8_t *out, size_t out_cap, size_t *out_len);

/* Test entry point used by the bridge. */
int vp_archive_repair_run(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* VAULTPACK_REPAIR_H */