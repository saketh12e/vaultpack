/*
 * include/vaultpack/section_pipeline.h
 *
 * Higher-level pipeline that takes a section payload, verifies its
 * declared compressed/raw sizes, optionally decompresses the payload,
 * verifies the trailing CRC, and exposes the inflated bytes to the
 * caller.
 *
 * The pipeline is the integration point between the section header
 * parser (src/section.c) and the decompression backends. Existing
 * sections that advertise a "raw" payload skip decompression entirely.
 */
#ifndef VAULTPACK_SECTION_PIPELINE_H
#define VAULTPACK_SECTION_PIPELINE_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum vp_section_pipeline_status {
    VP_PIPELINE_OK             = 0,
    VP_PIPELINE_NEED_INPUT     = 1,
    VP_PIPELINE_TRUNCATED      = 2,
    VP_PIPELINE_BAD_CODEC      = 3,
    VP_PIPELINE_CRC_MISMATCH   = 4,
    VP_PIPELINE_LENGTH_MISMATCH = 5,
    VP_PIPELINE_OOM            = 6
} vp_section_pipeline_status_t;

typedef struct vp_section_pipeline {
    uint32_t         codec_tag;
    size_t           declared_raw_size;
    size_t           declared_comp_size;
    uint32_t         expected_crc;
    uint8_t         *inflated;          /* owned; NULL until first step */
    size_t           inflated_len;
    size_t           inflated_cap;
    int              crc_validated;
    vp_section_pipeline_status_t last_status;
} vp_section_pipeline_t;

void vp_section_pipeline_init(vp_section_pipeline_t *p,
                              uint32_t codec_tag,
                              size_t declared_raw_size,
                              size_t declared_comp_size,
                              uint32_t expected_crc);

/* Run the pipeline on a payload. Returns VP_OK on success and writes
 * the inflated payload into *out (caller frees with free()). On error
 * returns the appropriate error code; *out may be NULL. */
int vp_section_pipeline_run(vp_section_pipeline_t *p,
                            const uint8_t *payload, size_t payload_len,
                            uint8_t **out, size_t *out_len);

/* Validate the inflated payload against the expected CRC. Returns VP_OK
 * on match, VP_ERR_CHECKSUM on mismatch. */
int vp_section_pipeline_verify_crc(vp_section_pipeline_t *p);

void vp_section_pipeline_destroy(vp_section_pipeline_t *p);

/* Section pipeline advanced operations. */
uint32_t vp_section_pipeline_codec(const vp_section_pipeline_t *p);
size_t vp_section_pipeline_declared_raw(const vp_section_pipeline_t *p);
size_t vp_section_pipeline_declared_comp(const vp_section_pipeline_t *p);
uint32_t vp_section_pipeline_expected_crc(const vp_section_pipeline_t *p);
int vp_section_pipeline_is_validated(const vp_section_pipeline_t *p);
size_t vp_section_pipeline_inflated_len(const vp_section_pipeline_t *p);
vp_section_pipeline_status_t vp_section_pipeline_last_status(const vp_section_pipeline_t *p);
const char *vp_section_pipeline_status_str(vp_section_pipeline_status_t st);
int vp_section_pipeline_reset(vp_section_pipeline_t *p);
int vp_section_pipeline_set_expected_crc(vp_section_pipeline_t *p, uint32_t crc);
int vp_section_pipeline_set_codec(vp_section_pipeline_t *p, uint32_t codec);
int vp_section_pipeline_check_lengths(const vp_section_pipeline_t *p,
                                     size_t actual_payload_len);
int vp_section_pipeline_compute_crc(const vp_section_pipeline_t *p, uint32_t *out_crc);

#ifdef __cplusplus
}
#endif

#endif /* VAULTPACK_SECTION_PIPELINE_H */