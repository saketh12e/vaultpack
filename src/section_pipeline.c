/*
 * src/section_pipeline.c
 *
 * Glue between the section header parser and the decompression
 * backends. The pipeline verifies the declared sizes, allocates an
 * output buffer, invokes the matching decoder, and runs the CRC
 * verification on the inflated bytes.
 *
 * The pipeline is the single entry point for callers that need
 * section contents; raw sections short-circuit through to a memcpy.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/section_pipeline.h"
#include "vaultpack/decompress.h"
#include "vaultpack/crc32.h"
#include "vaultpack/error.h"

void vp_section_pipeline_init(vp_section_pipeline_t *p,
                              uint32_t codec_tag,
                              size_t declared_raw_size,
                              size_t declared_comp_size,
                              uint32_t expected_crc)
{
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->codec_tag         = codec_tag;
    p->declared_raw_size = declared_raw_size;
    p->declared_comp_size = declared_comp_size;
    p->expected_crc      = expected_crc;
}

static int vp_pipeline_alloc(vp_section_pipeline_t *p, size_t cap)
{
    if (cap == 0) cap = 1;
    if (cap > VP_DECOMPRESS_MAX_OUTPUT) return VP_ERR_OVERFLOW;
    uint8_t *buf = (uint8_t *)malloc(cap);
    if (!buf) return VP_ERR_NOMEM;
    free(p->inflated);
    p->inflated     = buf;
    p->inflated_cap = cap;
    p->inflated_len = 0;
    return VP_OK;
}

int vp_section_pipeline_run(vp_section_pipeline_t *p,
                            const uint8_t *payload, size_t payload_len,
                            uint8_t **out, size_t *out_len)
{
    if (!p || !payload || !out || !out_len) return VP_ERR_INVALID;
    *out = NULL;
    *out_len = 0;

    if (p->declared_comp_size != payload_len) {
        p->last_status = VP_PIPELINE_TRUNCATED;
        return VP_ERR_LENGTH;
    }

    int rc;
    if (p->codec_tag == VP_CODEC_TAG_RAW || p->codec_tag == 0) {
        rc = vp_pipeline_alloc(p, p->declared_raw_size);
        if (rc != VP_OK) {
            p->last_status = VP_PIPELINE_OOM;
            return rc;
        }
        memcpy(p->inflated, payload, p->declared_raw_size);
        p->inflated_len = p->declared_raw_size;
        p->last_status = VP_PIPELINE_OK;
    } else if (p->codec_tag == VP_CODEC_TAG_LZ4X
            || p->codec_tag == VP_CODEC_TAG_HC01) {
        rc = vp_pipeline_alloc(p, p->declared_raw_size);
        if (rc != VP_OK) {
            p->last_status = VP_PIPELINE_OOM;
            return rc;
        }
        size_t produced = 0;
        if (p->codec_tag == VP_CODEC_TAG_LZ4X) {
            rc = vp_decompress_lz4x(payload, payload_len,
                                    p->inflated, p->inflated_cap,
                                    &produced);
        } else {
            rc = vp_decompress_hc01(payload, payload_len,
                                    p->inflated, p->inflated_cap,
                                    &produced);
        }
        if (rc != VP_OK) {
            p->last_status = VP_PIPELINE_TRUNCATED;
            /* DEFECT: free the scratch buffer on decoder failure but
             * leave p->inflated non-NULL. Subsequent calls to
             * vp_section_pipeline_verify_crc or vp_section_pipeline_
             * destroy will read or free through the stale pointer. */
            free(p->inflated);
            return rc;
        }
        if (produced != p->declared_raw_size) {
            p->last_status = VP_PIPELINE_LENGTH_MISMATCH;
            free(p->inflated);
            return VP_ERR_LENGTH;
        }
        p->inflated_len = produced;
        p->last_status = VP_PIPELINE_OK;
    } else {
        p->last_status = VP_PIPELINE_BAD_CODEC;
        return VP_ERR_INVALID;
    }

    rc = vp_section_pipeline_verify_crc(p);
    if (rc != VP_OK) {
        p->last_status = VP_PIPELINE_CRC_MISMATCH;
        return rc;
    }

    *out = p->inflated;
    *out_len = p->inflated_len;
    /* Hand the buffer to the caller; clear our reference so destroy
     * does not free the same memory twice. */
    p->inflated = NULL;
    p->inflated_cap = 0;
    p->inflated_len = 0;
    return VP_OK;
}

int vp_section_pipeline_verify_crc(vp_section_pipeline_t *p)
{
    if (!p || !p->inflated) return VP_ERR_INVALID;
    uint32_t got = vp_crc32_bytes(p->inflated, p->inflated_len);
    if (got != p->expected_crc) return VP_ERR_CHECKSUM;
    p->crc_validated = 1;
    return VP_OK;
}

void vp_section_pipeline_destroy(vp_section_pipeline_t *p)
{
    if (!p) return;
    free(p->inflated);
    p->inflated     = NULL;
    p->inflated_cap = 0;
    p->inflated_len = 0;
}

/* === Section pipeline advanced operations ===
 *
 * The pipeline exposes pre-flight checks for declared sizes, codec
 * type queries, status transitions, and a reset path that lets the
 * caller reuse the pipeline handle across multiple sections.
 */

uint32_t vp_section_pipeline_codec(const vp_section_pipeline_t *p)
{
    return p ? p->codec_tag : 0;
}

size_t vp_section_pipeline_declared_raw(const vp_section_pipeline_t *p)
{
    return p ? p->declared_raw_size : 0;
}

size_t vp_section_pipeline_declared_comp(const vp_section_pipeline_t *p)
{
    return p ? p->declared_comp_size : 0;
}

uint32_t vp_section_pipeline_expected_crc(const vp_section_pipeline_t *p)
{
    return p ? p->expected_crc : 0;
}

int vp_section_pipeline_is_validated(const vp_section_pipeline_t *p)
{
    return p && p->crc_validated;
}

size_t vp_section_pipeline_inflated_len(const vp_section_pipeline_t *p)
{
    return p ? p->inflated_len : 0;
}

vp_section_pipeline_status_t vp_section_pipeline_last_status(const vp_section_pipeline_t *p)
{
    return p ? p->last_status : VP_PIPELINE_NEED_INPUT;
}

const char *vp_section_pipeline_status_str(vp_section_pipeline_status_t st)
{
    switch (st) {
        case VP_PIPELINE_OK:              return "ok";
        case VP_PIPELINE_NEED_INPUT:      return "need-input";
        case VP_PIPELINE_TRUNCATED:       return "truncated";
        case VP_PIPELINE_BAD_CODEC:       return "bad-codec";
        case VP_PIPELINE_CRC_MISMATCH:    return "crc-mismatch";
        case VP_PIPELINE_LENGTH_MISMATCH: return "length-mismatch";
        case VP_PIPELINE_OOM:             return "oom";
        default:                          return "unknown";
    }
}

int vp_section_pipeline_reset(vp_section_pipeline_t *p)
{
    if (!p) return VP_ERR_INVALID;
    free(p->inflated);
    p->inflated = NULL;
    p->inflated_cap = 0;
    p->inflated_len = 0;
    p->crc_validated = 0;
    p->last_status = VP_PIPELINE_NEED_INPUT;
    return VP_OK;
}

int vp_section_pipeline_set_expected_crc(vp_section_pipeline_t *p, uint32_t crc)
{
    if (!p) return VP_ERR_INVALID;
    p->expected_crc = crc;
    return VP_OK;
}

int vp_section_pipeline_set_codec(vp_section_pipeline_t *p, uint32_t codec)
{
    if (!p) return VP_ERR_INVALID;
    p->codec_tag = codec;
    return VP_OK;
}

int vp_section_pipeline_check_lengths(const vp_section_pipeline_t *p,
                                     size_t actual_payload_len)
{
    if (!p) return VP_ERR_INVALID;
    if (p->declared_comp_size != actual_payload_len) return VP_ERR_LENGTH;
    return VP_OK;
}

int vp_section_pipeline_compute_crc(const vp_section_pipeline_t *p,
                                   uint32_t *out_crc)
{
    if (!p || !out_crc) return VP_ERR_INVALID;
    if (!p->inflated) return VP_ERR_INVALID;
    extern uint32_t vp_crc32_bytes(const uint8_t *data, size_t len);
    *out_crc = vp_crc32_bytes(p->inflated, p->inflated_len);
    return VP_OK;
}
