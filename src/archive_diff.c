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
#include <stdio.h>

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

/* === Production diff: parse two archives and compare ===
 *
 * Reads the central directory of each archive blob, builds the
 * per-archive section tables, and emits a sorted diff record
 * list. Returns the number of diff entries written into the
 * diff context.
 */

static int vp_diff_parse_directory(const uint8_t *src, size_t src_len,
                                  vp_dir_entry_desc_t **out, uint32_t *out_count)
{
    if (!src || src_len < 32) return VP_ERR_TRUNCATED;
    if (memcmp(src, "VPAK", 4) != 0) return VP_ERR_INVALID;
    uint32_t entry_count = (uint32_t)src[8]
                         | ((uint32_t)src[9] << 8)
                         | ((uint32_t)src[10] << 16)
                         | ((uint32_t)src[11] << 24);
    uint64_t dir_offset = (uint64_t)src[12]
                        | ((uint64_t)src[13] << 8)
                        | ((uint64_t)src[14] << 16)
                        | ((uint64_t)src[15] << 24)
                        | ((uint64_t)src[16] << 32)
                        | ((uint64_t)src[17] << 40)
                        | ((uint64_t)src[18] << 48)
                        | ((uint64_t)src[19] << 56);
    if (dir_offset > src_len) return VP_ERR_RANGE;
    if (dir_offset + (uint64_t)entry_count * 58u > src_len) return VP_ERR_TRUNCATED;
    if (entry_count == 0) {
        *out = NULL;
        *out_count = 0;
        return VP_OK;
    }
    vp_dir_entry_desc_t *tab = (vp_dir_entry_desc_t *)calloc(
        entry_count, sizeof(*tab));
    if (!tab) return VP_ERR_NOMEM;
    for (uint32_t i = 0; i < entry_count; i++) {
        const uint8_t *p = src + dir_offset + (size_t)i * 58u;
        vp_dir_entry_desc_t *e = &tab[i];
        memcpy(e->name, p, 32);
        e->name[31] = '\0';
        e->offset = (uint64_t)p[32]
                  | ((uint64_t)p[33] << 8)
                  | ((uint64_t)p[34] << 16)
                  | ((uint64_t)p[35] << 24)
                  | ((uint64_t)p[36] << 32)
                  | ((uint64_t)p[37] << 40)
                  | ((uint64_t)p[38] << 48)
                  | ((uint64_t)p[39] << 56);
        e->size = (uint32_t)p[40]
                | ((uint32_t)p[41] << 8)
                | ((uint32_t)p[42] << 16)
                | ((uint32_t)p[43] << 24);
        e->type = p[46];
    }
    /* Sort by name for merge-style comparison. */
    for (uint32_t i = 1; i < entry_count; i++) {
        vp_dir_entry_desc_t key = tab[i];
        uint32_t j = i;
        while (j > 0 && strcmp(tab[j - 1].name, key.name) > 0) {
            tab[j] = tab[j - 1];
            j--;
        }
        tab[j] = key;
    }
    *out = tab;
    *out_count = entry_count;
    return VP_OK;
}

int vp_diff_run_full(vp_diff_ctx_t *d,
                     const uint8_t *a, size_t a_len,
                     const uint8_t *b, size_t b_len)
{
    if (!d) return VP_ERR_INVALID;
    vp_diff_ctx_init(d);
    vp_dir_entry_desc_t *tab_a = NULL, *tab_b = NULL;
    uint32_t ca = 0, cb = 0;
    int rc = vp_diff_parse_directory(a, a_len, &tab_a, &ca);
    if (rc != VP_OK) return rc;
    rc = vp_diff_parse_directory(b, b_len, &tab_b, &cb);
    if (rc != VP_OK) { free(tab_a); return rc; }
    rc = vp_diff_compare_sections(d, tab_a, ca, tab_b, cb);
    free(tab_a);
    free(tab_b);
    return rc;
}

int vp_diff_format_text(const vp_diff_ctx_t *d,
                        char *out, size_t out_cap, size_t *out_len)
{
    if (!d || !out || out_cap == 0) return VP_ERR_INVALID;
    size_t pos = 0;
    for (uint32_t i = 0; i < d->count; i++) {
        const vp_diff_entry_t *e = &d->entries[i];
        const char *kind = "?";
        switch (e->kind) {
            case VP_DIFF_ADDED:   kind = "added";   break;
            case VP_DIFF_REMOVED: kind = "removed"; break;
            case VP_DIFF_CHANGED: kind = "changed"; break;
            default: break;
        }
        int n = snprintf(out + pos, out_cap - pos, "%s\t%s\n", kind, e->name);
        if (n < 0 || (size_t)n >= out_cap - pos) return VP_ERR_FULL;
        pos += (size_t)n;
    }
    if (out_len) *out_len = pos;
    return VP_OK;
}

int vp_diff_format_json(const vp_diff_ctx_t *d,
                        char *out, size_t out_cap, size_t *out_len)
{
    if (!d || !out || out_cap == 0) return VP_ERR_INVALID;
    size_t pos = 0;
    int n = snprintf(out + pos, out_cap - pos, "{\"diff\":[");
    if (n < 0 || (size_t)n >= out_cap - pos) return VP_ERR_FULL;
    pos += (size_t)n;
    for (uint32_t i = 0; i < d->count; i++) {
        const vp_diff_entry_t *e = &d->entries[i];
        const char *kind = "?";
        switch (e->kind) {
            case VP_DIFF_ADDED:   kind = "added";   break;
            case VP_DIFF_REMOVED: kind = "removed"; break;
            case VP_DIFF_CHANGED: kind = "changed"; break;
            default: break;
        }
        n = snprintf(out + pos, out_cap - pos,
                     "%s{\"kind\":\"%s\",\"name\":\"%s\"}",
                     i == 0 ? "" : ",", kind, e->name);
        if (n < 0 || (size_t)n >= out_cap - pos) return VP_ERR_FULL;
        pos += (size_t)n;
    }
    n = snprintf(out + pos, out_cap - pos, "]}");
    if (n < 0 || (size_t)n >= out_cap - pos) return VP_ERR_FULL;
    pos += (size_t)n;
    if (out_len) *out_len = pos;
    return VP_OK;
}