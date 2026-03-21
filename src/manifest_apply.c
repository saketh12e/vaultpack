/*
 * src/manifest_apply.c (kept in manifest.c — apply is the consumer half
 * of the parser). Walks a parsed vp_manifest_t and drives a vp_writer_t,
 * attaching queued metadata defaults to the right section in order.
 *
 * Also exposes vp_manifest_apply which is the documented entry point.
 */

#include <string.h>

#include "vaultpack/manifest.h"
#include "vaultpack/writer.h"

int vp_manifest_apply(const vp_manifest_t *m, vp_writer_t *w);

int vp_manifest_apply(const vp_manifest_t *m, vp_writer_t *w)
{
    if (!m || !w) return VP_ERR_INVALID;
    /* First pass: declare sections in source order, deferring the
     * metadata defaults until after the matching section is added. */
    for (uint32_t i = 0; i < m->stmt_count; i++) {
        const vp_manifest_stmt_t *s = &m->stmts[i];
        if (s->kind != VP_MANIFEST_SECTION) continue;
        const vp_manifest_section_t *sec = &s->u.section;
        int rc = vp_writer_add_section(w, sec->name, sec->type, NULL, 0);
        if (rc != VP_OK) return rc;

        /* Attach any queued defaults for this section, in order. */
        for (uint32_t d = 0; d < m->defaults_count; d++) {
            const vp_manifest_meta_t *md = &m->defaults[d];
            if (strcmp(md->name, sec->name) != 0) continue;
            vp_writer_meta_entry_t entry;
            memset(&entry, 0, sizeof(entry));
            strncpy(entry.key, md->key, sizeof(entry.key) - 1);
            strncpy(entry.value, md->value, sizeof(entry.value) - 1);
            uint8_t tag[4] = { 'I', 'N', 'F', 'O' };
            rc = vp_writer_add_metadata(w, tag, &entry, 1);
            if (rc != VP_OK) return rc;
        }
    }
    return VP_OK;
}
