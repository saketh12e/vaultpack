/*
 * include/vaultpack/validate.h
 *
 * Whole-archive consistency walk. The validator reads a parsed archive
 * and reports:
 *   - header field sanity
 *   - directory offset and bounds
 *   - section overlap detection
 *   - metadata block layout
 *   - checksum verification (when a CRC is provided)
 *
 * Reports are returned as a list of issues so callers can show all
 * problems instead of failing at the first one.
 */
#ifndef VAULTPACK_VALIDATE_H
#define VAULTPACK_VALIDATE_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/archive.h"
#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum vp_validate_severity {
    VP_VALIDATE_INFO = 0,
    VP_VALIDATE_WARN,
    VP_VALIDATE_ERROR
} vp_validate_severity_t;

typedef struct vp_validate_issue {
    vp_validate_severity_t severity;
    uint32_t code;            /* VP_ERR_* */
    char     message[160];
    uint32_t section_index;
} vp_validate_issue_t;

typedef struct vp_validate_report {
    vp_validate_issue_t *issues;
    uint32_t              issue_count;
    uint32_t              issue_cap;
} vp_validate_report_t;

void vp_validate_report_init(vp_validate_report_t *r);
void vp_validate_report_free(vp_validate_report_t *r);

/* Run the validator. Returns VP_OK if no errors found; otherwise the
 * first error code (the report still contains every issue). */
int vp_validate_archive(const vp_archive_t *arc, vp_validate_report_t *r);

/* Internal: build the interval table used for overlap detection. */
int vp_validate_collect_intervals(const vp_archive_t *arc,
                                  uint64_t **out_intervals,
                                  uint32_t *out_count);

#ifdef __cplusplus
}
#endif

#endif
