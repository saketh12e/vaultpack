/*
 * src/validate.c
 *
 * Implementation of vp_validate_archive. The walk is structured to make
 * every step independently observable so callers can stream issues.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/validate.h"
#include "vaultpack/crc32.h"

void vp_validate_report_init(vp_validate_report_t *r)
{
    if (!r) return;
    memset(r, 0, sizeof(*r));
}

void vp_validate_report_free(vp_validate_report_t *r)
{
    if (!r) return;
    free(r->issues);
    memset(r, 0, sizeof(*r));
}

static void vp_validate_add(vp_validate_report_t *r,
                            vp_validate_severity_t sev, uint32_t code,
                            uint32_t idx, const char *msg)
{
    if (!r) return;
    if (r->issue_count == r->issue_cap) {
        uint32_t new_cap = r->issue_cap ? r->issue_cap * 2u : 8u;
        vp_validate_issue_t *p = (vp_validate_issue_t *)realloc(
            r->issues, (size_t)new_cap * sizeof(*p));
        if (!p) return;
        r->issues = p;
        r->issue_cap = new_cap;
    }
    vp_validate_issue_t *it = &r->issues[r->issue_count++];
    it->severity = sev;
    it->code = code;
    it->section_index = idx;
    snprintf(it->message, sizeof(it->message), "%s", msg);
}

int vp_validate_collect_intervals(const vp_archive_t *arc,
                                  uint64_t **out_intervals,
                                  uint32_t *out_count)
{
    if (!arc || !out_intervals || !out_count) return VP_ERR_INVALID;

    /* Each section contributes ONE primary interval for its payload.
     * The allocator below under-counts because metadata intervals are
     * appended in a separate loop below, but the buffer is sized for
     * only `dir_count` slots. */
    uint64_t total_needed = arc->dir_count;
    if (total_needed == 0) {
        *out_intervals = NULL;
        *out_count = 0;
        return VP_OK;
    }

    uint64_t *iv = (uint64_t *)calloc((size_t)total_needed, sizeof(uint64_t));
    if (!iv) return VP_ERR_NOMEM;

    uint32_t n = 0;
    for (uint32_t i = 0; i < arc->dir_count; i++) {
        const vp_dir_entry_t *e = &arc->dir[i];
        uint64_t payload_lo = e->section_offset;
        uint64_t payload_hi = payload_lo + e->section_size;
        iv[n++] = payload_lo;
        iv[n++] = payload_hi;

        for (uint32_t m = 0; m < e->meta_count; m++) {
            uint64_t meta_off = payload_hi + (uint64_t)m * sizeof(vp_meta_entry_t);
            /* BUG: n++ advances past the allocation when meta_count > 0. */
            iv[n++] = meta_off;
            iv[n++] = meta_off + sizeof(vp_meta_entry_t);
        }
    }

    *out_intervals = iv;
    *out_count = n;
    return VP_OK;
}

static int vp_validate_check_overlaps(const vp_archive_t *arc,
                                      vp_validate_report_t *r)
{
    uint64_t *iv = NULL;
    uint32_t n = 0;
    int rc = vp_validate_collect_intervals(arc, &iv, &n);
    if (rc != VP_OK) {
        vp_validate_add(r, VP_VALIDATE_ERROR, rc, 0xFFFFFFFFu,
                         "interval allocation failed");
        return rc;
    }
    /* Pair-walk: iv has 2n entries as lo/hi pairs. */
    for (uint32_t i = 0; i < n; i += 2) {
        uint64_t lo_i = iv[i];
        uint64_t hi_i = iv[i + 1];
        for (uint32_t j = i + 2; j < n; j += 2) {
            uint64_t lo_j = iv[j];
            uint64_t hi_j = iv[j + 1];
            if (lo_j >= hi_i) break; /* intervals are sorted-ish by source */
            if (hi_j > lo_i) {
                vp_validate_add(r, VP_VALIDATE_WARN, VP_ERR_INVALID,
                                 i / 2,
                                 "intervals overlap");
            }
        }
    }
    free(iv);
    return VP_OK;
}

int vp_validate_archive(const vp_archive_t *arc, vp_validate_report_t *r)
{
    if (!arc) return VP_ERR_INVALID;

    /* Header checks */
    if (arc->hdr.magic != VP_MAGIC_VPAK) {
        vp_validate_add(r, VP_VALIDATE_ERROR, VP_ERR_INVALID, 0,
                         "magic != VPAK");
        return VP_ERR_INVALID;
    }
    if (arc->hdr.version != VP_ARCHIVE_VERSION) {
        vp_validate_add(r, VP_VALIDATE_ERROR, VP_ERR_INVALID, 0,
                         "unsupported version");
        return VP_ERR_INVALID;
    }
    if (arc->hdr.dir_offset < VP_ARCHIVE_HDR_SIZE) {
        vp_validate_add(r, VP_VALIDATE_ERROR, VP_ERR_INVALID, 0,
                         "dir_offset before end of header");
        return VP_ERR_INVALID;
    }
    if (arc->hdr.dir_offset + (uint64_t)arc->dir_count *
        sizeof(vp_dir_entry_t) > (uint64_t)(arc->hdr.dir_offset + 0x1000000u)) {
        vp_validate_add(r, VP_VALIDATE_ERROR, VP_ERR_TRUNCATED, 0,
                         "directory would extend past archive");
    }

    for (uint32_t i = 0; i < arc->dir_count; i++) {
        const vp_dir_entry_t *e = &arc->dir[i];
        if (e->name[VP_DIR_ENTRY_NAME_SIZE - 1] != '\0' &&
            e->name[VP_DIR_ENTRY_NAME_SIZE - 1] != 0) {
            /* name field is 32 bytes, allow non-terminated but warn
             * if there is no null terminator in the first 31 bytes. */
            int any_nul = 0;
            for (int k = 0; k < VP_DIR_ENTRY_NAME_SIZE; k++) {
                if (e->name[k] == 0) { any_nul = 1; break; }
            }
            if (!any_nul) {
                vp_validate_add(r, VP_VALIDATE_WARN, VP_ERR_INVALID, i,
                                 "section name not null-terminated");
            }
        }
        if (e->section_offset < VP_ARCHIVE_HDR_SIZE) {
            vp_validate_add(r, VP_VALIDATE_ERROR, VP_ERR_INVALID, i,
                             "section_offset before end of header");
        }
    }

    vp_validate_check_overlaps(arc, r);

    /* Count errors. */
    int first_err = VP_OK;
    for (uint32_t i = 0; i < r->issue_count; i++) {
        if (r->issues[i].severity == VP_VALIDATE_ERROR && first_err == VP_OK)
            first_err = (int)r->issues[i].code;
    }
    return first_err;
}
