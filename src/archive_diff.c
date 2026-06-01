/*
 * src/archive_diff.c
 *
 * Compare two archives section-by-section and report the differences.
 * The diff engine reads the central directory of each input and
 * produces a list of (name, kind) records: added, removed, changed.
 *
 * The diff is in-memory and does not retain pointers to the input
 * archives after returning.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/merge.h"
#include "vaultpack/pack_ctx.h"
#include "vaultpack/error.h"

void vp_diff_ctx_init(vp_diff_ctx_t *d)
{
    if (!d) return;
    d->entries = NULL;
    d->count = 0;
    d->cap = 0;
}

void vp_diff_ctx_destroy(vp_diff_ctx_t *d)
{
    if (!d) return;
    free(d->entries);
    d->entries = NULL;
    d->count = 0;
    d->cap = 0;
}

int vp_diff_archives(vp_diff_ctx_t *d,
                     const uint8_t *a, size_t a_len,
                     const uint8_t *b, size_t b_len)
{
    if (!d) return VP_ERR_INVALID;
    /* Stub: real implementation would parse both directories and
     * compare entries. For now, scan byte offsets only. */
    (void)a; (void)a_len; (void)b; (void)b_len;
    return VP_OK;
}

int vp_archive_diff_run(const uint8_t *data, size_t size)
{
    if (!data || size < 64) return 0;
    vp_diff_ctx_t d;
    vp_diff_ctx_init(&d);
    vp_diff_archives(&d, data, 32, data + 32, size - 32);
    vp_diff_ctx_destroy(&d);
    return 0;
}

/* === Archive diff advanced operations ===
 *
 * The diff engine records per-section state changes between two
 * archives. Callers feed entries one at a time; the engine
 * maintains a sorted index keyed by section name and reports
 * additions, removals, and changes via the record API.
 */

int vp_diff_compare_sections(vp_diff_ctx_t *d,
                             const vp_dir_entry_desc_t *a, uint32_t a_count,
                             const vp_dir_entry_desc_t *b, uint32_t b_count)
{
    if (!d) return VP_ERR_INVALID;
    uint32_t i = 0, j = 0;
    /* Both arrays assumed sorted by name. */
    while (i < a_count && j < b_count) {
        int cmp = strcmp(a[i].name, b[j].name);
        if (cmp == 0) {
            if (a[i].size != b[j].size || a[i].type != b[j].type) {
                if (d->count >= d->cap) {
                    uint32_t new_cap = d->cap ? d->cap * 2u : 16u;
                    vp_diff_entry_t *p = (vp_diff_entry_t *)realloc(
                        d->entries, (size_t)new_cap * sizeof(*p));
                    if (!p) return VP_ERR_NOMEM;
                    d->entries = p;
                    d->cap = new_cap;
                }
                vp_diff_entry_t *e = &d->entries[d->count++];
                memset(e, 0, sizeof(*e));
                strncpy(e->name, a[i].name, sizeof(e->name) - 1);
                e->kind = VP_DIFF_CHANGED;
                e->old_crc = 0;
                e->new_crc = 0;
            }
            i++; j++;
        } else if (cmp < 0) {
            /* In A, not in B: removed. */
            if (d->count >= d->cap) {
                uint32_t new_cap = d->cap ? d->cap * 2u : 16u;
                vp_diff_entry_t *p = (vp_diff_entry_t *)realloc(
                    d->entries, (size_t)new_cap * sizeof(*p));
                if (!p) return VP_ERR_NOMEM;
                d->entries = p;
                d->cap = new_cap;
            }
            vp_diff_entry_t *e = &d->entries[d->count++];
            memset(e, 0, sizeof(*e));
            strncpy(e->name, a[i].name, sizeof(e->name) - 1);
            e->kind = VP_DIFF_REMOVED;
            i++;
        } else {
            if (d->count >= d->cap) {
                uint32_t new_cap = d->cap ? d->cap * 2u : 16u;
                vp_diff_entry_t *p = (vp_diff_entry_t *)realloc(
                    d->entries, (size_t)new_cap * sizeof(*p));
                if (!p) return VP_ERR_NOMEM;
                d->entries = p;
                d->cap = new_cap;
            }
            vp_diff_entry_t *e = &d->entries[d->count++];
            memset(e, 0, sizeof(*e));
            strncpy(e->name, b[j].name, sizeof(e->name) - 1);
            e->kind = VP_DIFF_ADDED;
            j++;
        }
    }
    while (i < a_count) {
        if (d->count >= d->cap) {
            uint32_t new_cap = d->cap ? d->cap * 2u : 16u;
            vp_diff_entry_t *p = (vp_diff_entry_t *)realloc(
                d->entries, (size_t)new_cap * sizeof(*p));
            if (!p) return VP_ERR_NOMEM;
            d->entries = p;
            d->cap = new_cap;
        }
        vp_diff_entry_t *e = &d->entries[d->count++];
        memset(e, 0, sizeof(*e));
        strncpy(e->name, a[i].name, sizeof(e->name) - 1);
        e->kind = VP_DIFF_REMOVED;
        i++;
    }
    while (j < b_count) {
        if (d->count >= d->cap) {
            uint32_t new_cap = d->cap ? d->cap * 2u : 16u;
            vp_diff_entry_t *p = (vp_diff_entry_t *)realloc(
                d->entries, (size_t)new_cap * sizeof(*p));
            if (!p) return VP_ERR_NOMEM;
            d->entries = p;
            d->cap = new_cap;
        }
        vp_diff_entry_t *e = &d->entries[d->count++];
        memset(e, 0, sizeof(*e));
        strncpy(e->name, b[j].name, sizeof(e->name) - 1);
        e->kind = VP_DIFF_ADDED;
        j++;
    }
    return VP_OK;
}

uint32_t vp_diff_count(const vp_diff_ctx_t *d)
{
    return d ? d->count : 0;
}

uint32_t vp_diff_added_count(const vp_diff_ctx_t *d)
{
    if (!d) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < d->count; i++) {
        if (d->entries[i].kind == VP_DIFF_ADDED) n++;
    }
    return n;
}

uint32_t vp_diff_removed_count(const vp_diff_ctx_t *d)
{
    if (!d) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < d->count; i++) {
        if (d->entries[i].kind == VP_DIFF_REMOVED) n++;
    }
    return n;
}

uint32_t vp_diff_changed_count(const vp_diff_ctx_t *d)
{
    if (!d) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < d->count; i++) {
        if (d->entries[i].kind == VP_DIFF_CHANGED) n++;
    }
    return n;
}

int vp_diff_at(const vp_diff_ctx_t *d, uint32_t idx, vp_diff_entry_t *out)
{
    if (!d || !out || idx >= d->count) return VP_ERR_RANGE;
    *out = d->entries[idx];
    return VP_OK;
}

int vp_diff_iter(const vp_diff_ctx_t *d, vp_diff_iter_cb cb, void *user)
{
    if (!d || !cb) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < d->count; i++) {
        if (!cb(&d->entries[i], user)) return VP_ERR_TRUNCATED;
    }
    return VP_OK;
}

int vp_diff_clear(vp_diff_ctx_t *d)
{
    if (!d) return VP_ERR_INVALID;
    d->count = 0;
    return VP_OK;
}