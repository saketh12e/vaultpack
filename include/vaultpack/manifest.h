/*
 * include/vaultpack/manifest.h
 *
 * Parser for a text manifest that describes a VaultPack archive. The
 * grammar is small and explicit; comments and whitespace are skipped.
 *
 *   manifest := line*
 *   line     := statement '\n'
 *   statement := section_stmt | meta_stmt
 *   section_stmt := 'section' NAME type [flag*]
 *   meta_stmt    := 'meta' NAME key '=' value
 *
 * Type tokens: raw | compressed | metadata.
 * Flag tokens: any case-insensitive identifier (stored verbatim).
 *
 * Errors are accumulated in a diagnostics list and reported via the
 * `vp_manifest_apply` consumer. The parser is line-oriented and never
 * aborts; malformed input produces a partial parse plus diagnostics.
 */
#ifndef VAULTPACK_MANIFEST_H
#define VAULTPACK_MANIFEST_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum vp_manifest_kind {
    VP_MANIFEST_NONE = 0,
    VP_MANIFEST_SECTION,
    VP_MANIFEST_META
} vp_manifest_kind_t;

typedef struct vp_manifest_section {
    char name[64];
    uint8_t type;            /* VP_SECTION_RAW / COMPRESSED / METADATA */
    uint16_t flags;
} vp_manifest_section_t;

typedef struct vp_manifest_meta {
    char name[64];
    char key[24];
    char value[40];
} vp_manifest_meta_t;

typedef struct vp_manifest_stmt {
    vp_manifest_kind_t kind;
    union {
        vp_manifest_section_t section;
        vp_manifest_meta_t meta;
    } u;
    uint32_t line;
    uint32_t col;
} vp_manifest_stmt_t;

typedef struct vp_manifest_diag {
    uint32_t line;
    uint32_t col;
    int      code;            /* VP_ERR_* */
    char     message[128];
} vp_manifest_diag_t;

typedef struct vp_manifest {
    vp_manifest_stmt_t  *stmts;
    uint32_t             stmt_count;
    uint32_t             stmt_cap;

    /* Per-section metadata accumulator. When a meta stmt appears for
     * a section, its key/value are appended here. */
    vp_manifest_meta_t  *defaults;
    uint32_t             defaults_count;
    uint32_t             defaults_cap;

    vp_manifest_diag_t  *diags;
    uint32_t             diag_count;
    uint32_t             diag_cap;
} vp_manifest_t;

void vp_manifest_init(vp_manifest_t *m);
void vp_manifest_free(vp_manifest_t *m);
int  vp_manifest_parse(vp_manifest_t *m, const char *text, size_t len);

/* Convenience: count diagnostics, fetch by index. */
uint32_t vp_manifest_diag_count(const vp_manifest_t *m);
const vp_manifest_diag_t *vp_manifest_diag(const vp_manifest_t *m, uint32_t i);

/* Add a section manually (used by tests and by programmatic callers). */
int vp_manifest_add_section(vp_manifest_t *m,
                            const char *name, uint8_t type, uint16_t flags);

/* Add a metadata default for a section. If the section has not been
 * declared yet, the default is queued until the section is added. */
int vp_manifest_add_default_meta(vp_manifest_t *m,
                                 const char *section,
                                 const char *key, const char *value);

#ifdef __cplusplus
}
#endif

#endif
