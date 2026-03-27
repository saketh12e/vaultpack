/*
 * src/manifest.c
 *
 * Tokenizer + parser for VaultPack text manifests. The parser keeps a
 * list of statements and diagnostics; it never aborts on bad input.
 * The apply step (manifest_apply) is implemented as a small consumer
 * inside the validator module — the manifest module itself is just
 * parsing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "vaultpack/manifest.h"
#include "vaultpack/section.h"

void vp_manifest_init(vp_manifest_t *m)
{
    if (!m) return;
    memset(m, 0, sizeof(*m));
}

static int vp_manifest_reserve_stmts(vp_manifest_t *m, uint32_t need)
{
    if (need <= m->stmt_cap) return 1;
    uint32_t new_cap = m->stmt_cap ? m->stmt_cap * 2u : 16u;
    while (new_cap < need) new_cap *= 2u;
    vp_manifest_stmt_t *p = (vp_manifest_stmt_t *)realloc(
        m->stmts, (size_t)new_cap * sizeof(*p));
    if (!p) return 0;
    m->stmts = p;
    m->stmt_cap = new_cap;
    return 1;
}

static int vp_manifest_reserve_defaults(vp_manifest_t *m, uint32_t need)
{
    if (need <= m->defaults_cap) return 1;
    uint32_t new_cap = m->defaults_cap ? m->defaults_cap * 2u : 8u;
    while (new_cap < need) new_cap *= 2u;
    vp_manifest_meta_t *p = (vp_manifest_meta_t *)realloc(
        m->defaults, (size_t)new_cap * sizeof(*p));
    if (!p) return 0;
    m->defaults = p;
    m->defaults_cap = new_cap;
    return 1;
}

static int vp_manifest_reserve_diags(vp_manifest_t *m, uint32_t need)
{
    if (need <= m->diag_cap) return 1;
    uint32_t new_cap = m->diag_cap ? m->diag_cap * 2u : 8u;
    while (new_cap < need) new_cap *= 2u;
    vp_manifest_diag_t *p = (vp_manifest_diag_t *)realloc(
        m->diags, (size_t)new_cap * sizeof(*p));
    if (!p) return 0;
    m->diags = p;
    m->diag_cap = new_cap;
    return 1;
}

static void vp_manifest_add_diag(vp_manifest_t *m, uint32_t line, uint32_t col,
                                 int code, const char *msg)
{
    if (!vp_manifest_reserve_diags(m, m->diag_count + 1)) return;
    vp_manifest_diag_t *d = &m->diags[m->diag_count++];
    d->line = line;
    d->col = col;
    d->code = code;
    snprintf(d->message, sizeof(d->message), "%s", msg);
}

uint32_t vp_manifest_diag_count(const vp_manifest_t *m)
{
    return m ? m->diag_count : 0;
}

const vp_manifest_diag_t *vp_manifest_diag(const vp_manifest_t *m, uint32_t i)
{
    if (!m || i >= m->diag_count) return NULL;
    return &m->diags[i];
}

void vp_manifest_free(vp_manifest_t *m)
{
    if (!m) return;
    free(m->stmts);
    free(m->defaults);
    free(m->diags);
    memset(m, 0, sizeof(*m));
}

int vp_manifest_add_section(vp_manifest_t *m, const char *name,
                            uint8_t type, uint16_t flags)
{
    if (!m || !name) return VP_ERR_INVALID;
    if (!vp_manifest_reserve_stmts(m, m->stmt_count + 1)) return VP_ERR_NOMEM;
    vp_manifest_stmt_t *s = &m->stmts[m->stmt_count++];
    memset(s, 0, sizeof(*s));
    s->kind = VP_MANIFEST_SECTION;
    s->u.section.type = type;
    s->u.section.flags = flags;
    strncpy(s->u.section.name, name, sizeof(s->u.section.name) - 1);
    s->line = 0;
    s->col = 0;
    return VP_OK;
}

int vp_manifest_add_default_meta(vp_manifest_t *m, const char *section,
                                 const char *key, const char *value)
{
    if (!m || !section || !key || !value) return VP_ERR_INVALID;
    if (!vp_manifest_reserve_defaults(m, m->defaults_count + 1)) return VP_ERR_NOMEM;
    vp_manifest_meta_t *d = &m->defaults[m->defaults_count++];
    memset(d, 0, sizeof(*d));
    strncpy(d->name, section, sizeof(d->name) - 1);
    strncpy(d->key, key, sizeof(d->key) - 1);
    strncpy(d->value, value, sizeof(d->value) - 1);
    return VP_OK;
}

/* --- Lexer --- */

typedef struct vp_manifest_lex {
    const char *p;
    const char *end;
    uint32_t    line;
    uint32_t    col;
} vp_manifest_lex_t;

static void vp_lex_init(vp_manifest_lex_t *l, const char *p, size_t n)
{
    l->p = p; l->end = p + n; l->line = 1; l->col = 1;
}

static int vp_lex_eof(vp_manifest_lex_t *l) { return l->p >= l->end; }

static void vp_lex_skip_ws_and_comments(vp_manifest_lex_t *l)
{
    while (!vp_lex_eof(l)) {
        char c = *l->p;
        if (c == ' ' || c == '\t' || c == '\r') {
            l->p++; l->col++;
        } else if (c == '\n') {
            l->p++; l->line++; l->col = 1;
        } else if (c == '#') {
            while (!vp_lex_eof(l) && *l->p != '\n') {
                l->p++; l->col++;
            }
        } else {
            break;
        }
    }
}

static int vp_lex_match_kw(vp_manifest_lex_t *l, const char *kw)
{
    size_t n = strlen(kw);
    if ((size_t)(l->end - l->p) < n) return 0;
    if (strncmp(l->p, kw, n) != 0) return 0;
    /* Must be followed by whitespace or end-of-line */
    if (n < (size_t)(l->end - l->p)) {
        char c = l->p[n];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '#') return 0;
    }
    l->p += n; l->col += (uint32_t)n;
    return 1;
}

static int vp_lex_token(vp_manifest_lex_t *l, char *out, size_t out_size)
{
    vp_lex_skip_ws_and_comments(l);
    if (vp_lex_eof(l)) return 0;
    size_t i = 0;
    while (!vp_lex_eof(l)) {
        char c = *l->p;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '#') break;
        if (i + 1 < out_size) out[i++] = c;
        l->p++; l->col++;
    }
    out[i] = '\0';
    return 1;
}

static int vp_lex_token_eq(vp_manifest_lex_t *l, char *out, size_t out_size)
{
    /* Read a token that may include '=' as a separator; returns left side only. */
    vp_lex_skip_ws_and_comments(l);
    if (vp_lex_eof(l)) return 0;
    size_t i = 0;
    while (!vp_lex_eof(l)) {
        char c = *l->p;
        if (c == '=' || c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
        if (i + 1 < out_size) out[i++] = c;
        l->p++; l->col++;
    }
    out[i] = '\0';
    return 1;
}

static int vp_lex_peek_eq(vp_manifest_lex_t *l)
{
    vp_lex_skip_ws_and_comments(l);
    if (vp_lex_eof(l)) return 0;
    return *l->p == '=';
}

/* --- Parser --- */

static int vp_parse_section(vp_manifest_t *m, vp_manifest_lex_t *l)
{
    char name[64], type[32], flags[64];
    uint32_t line = l->line;
    if (!vp_lex_token(l, name, sizeof(name))) {
        vp_manifest_add_diag(m, line, l->col, VP_ERR_INVALID, "section: missing name");
        return VP_ERR_INVALID;
    }
    if (!vp_lex_token(l, type, sizeof(type))) {
        vp_manifest_add_diag(m, line, l->col, VP_ERR_INVALID, "section: missing type");
        return VP_ERR_INVALID;
    }

    uint8_t type_id = VP_SECTION_RAW;
    if (strcmp(type, "raw") == 0) type_id = VP_SECTION_RAW;
    else if (strcmp(type, "compressed") == 0) type_id = VP_SECTION_COMPRESSED;
    else if (strcmp(type, "metadata") == 0) type_id = VP_SECTION_METADATA;
    else {
        vp_manifest_add_diag(m, line, l->col, VP_ERR_INVALID,
                             "section: unknown type (expected raw|compressed|metadata)");
    }

    uint16_t fl = 0;
    /* Flag tokens are read inline; stop when we cross a newline or
     * reach end-of-input. We track the column at which we started
     * and bail if the lexer returned to a smaller column (meaning a
     * newline was consumed). */
    uint32_t start_col = l->col;
    while (!vp_lex_eof(l)) {
        vp_lex_skip_ws_and_comments(l);
        if (vp_lex_eof(l)) break;
        if (l->line != line || l->col < start_col) break;
        if (!vp_lex_token(l, flags, sizeof(flags))) break;
        if (flags[0] == '\0') break;
        fl = (uint16_t)((fl << 1) | 1u);
    }

    vp_lex_skip_ws_and_comments(l);
    if (!vp_lex_eof(l) && l->line == line && *l->p != '\n') {
        vp_manifest_add_diag(m, line, l->col, VP_ERR_INVALID,
                             "section: unexpected trailing tokens");
    }

    int r = vp_manifest_add_section(m, name, type_id, fl);
    return r;
}

static int vp_parse_meta(vp_manifest_t *m, vp_manifest_lex_t *l)
{
    char section[64], key[24], value[40];
    uint32_t line = l->line;
    if (!vp_lex_token(l, section, sizeof(section))) {
        vp_manifest_add_diag(m, line, l->col, VP_ERR_INVALID, "meta: missing section");
        return VP_ERR_INVALID;
    }
    if (!vp_lex_token_eq(l, key, sizeof(key))) {
        vp_manifest_add_diag(m, line, l->col, VP_ERR_INVALID, "meta: missing key");
        return VP_ERR_INVALID;
    }
    if (!vp_lex_peek_eq(l)) {
        vp_manifest_add_diag(m, line, l->col, VP_ERR_INVALID, "meta: missing '='");
        return VP_ERR_INVALID;
    }
    l->p++; l->col++; /* consume '=' */
    if (!vp_lex_token(l, value, sizeof(value))) {
        vp_manifest_add_diag(m, line, l->col, VP_ERR_INVALID, "meta: missing value");
        return VP_ERR_INVALID;
    }
    return vp_manifest_add_default_meta(m, section, key, value);
}

int vp_manifest_parse(vp_manifest_t *m, const char *text, size_t len)
{
    if (!m) return VP_ERR_INVALID;
    vp_manifest_lex_t l;
    vp_lex_init(&l, text, len);

    while (!vp_lex_eof(&l)) {
        vp_lex_skip_ws_and_comments(&l);
        if (vp_lex_eof(&l)) break;
        uint32_t line = l.line;
        uint32_t col = l.col;
        if (vp_lex_match_kw(&l, "section")) {
            vp_parse_section(m, &l);
        } else if (vp_lex_match_kw(&l, "meta")) {
            vp_parse_meta(m, &l);
        } else {
            /* unknown directive or stray token */
            vp_manifest_add_diag(m, line, col, VP_ERR_INVALID,
                                 "expected 'section' or 'meta' directive");
            /* skip to next line */
            while (!vp_lex_eof(&l) && *l.p != '\n') { l.p++; l.col++; }
        }
        vp_lex_skip_ws_and_comments(&l);
    }
    return VP_OK;
}
