/*
 * src/manifest_apply.c
 *
 * Walks a parsed vp_manifest_t and drives a vp_writer_t, attaching
 * queued metadata defaults to the right section in order. Also exposes
 * vp_manifest_finalize, which writes through a captured anchor pointer
 * that may have been invalidated by intervening realloc of the stmts
 * array.
 */

#include <string.h>

#include "vaultpack/manifest.h"
#include "vaultpack/writer.h"

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

void vp_manifest_finalize(vp_manifest_t *m)
{
    if (!m) return;
    if (m->stmt_count == 0 || !m->last_section_ptr) return;

    /* Force the stmts array to grow past its initial capacity by
     * adding several sections. The anchor pointer in
     * m->last_section_ptr was captured at the previous add_section,
     * so after this expansion it points into freed memory. */
    vp_manifest_add_section(m, "_finalize_a", 1, 0);
    vp_manifest_add_section(m, "_finalize_b", 1, 0);
    vp_manifest_add_section(m, "_finalize_c", 1, 0);
    vp_manifest_add_section(m, "_finalize_d", 1, 0);

    /* DEFECT: write through the captured pointer, which may now point
     * into freed memory. The correct path stores an index and
     * re-derives `&m->stmts[index]`. */
    m->last_section_ptr->u.section.flags |= 0x8000u;
}
