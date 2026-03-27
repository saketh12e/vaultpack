#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/vaultpack.h"

static void test_manifest_parse_simple(void)
{
    const char *m =
        "# simple manifest\n"
        "section boot raw\n"
        "section config metadata\n"
        "meta config board=sensor-a\n"
        "meta config version=7\n";
    vp_manifest_t mf;
    vp_manifest_init(&mf);
    int rc = vp_manifest_parse(&mf, m, strlen(m));
    assert(rc == VP_OK);
    /* 2 section statements; meta defaults are queued separately. */
    assert(mf.stmt_count == 2);
    assert(mf.defaults_count == 2);
    assert(mf.diag_count == 0);
    vp_manifest_free(&mf);
    printf("test_manifest_parse_simple: ok\n");
}

static void test_manifest_parse_diag(void)
{
    /* Bad type produces a diagnostic but parsing continues. */
    const char *m =
        "section foo bogus\n"
        "section bar raw\n";
    vp_manifest_t mf;
    vp_manifest_init(&mf);
    int rc = vp_manifest_parse(&mf, m, strlen(m));
    assert(rc == VP_OK);
    assert(mf.diag_count >= 1);
    assert(mf.stmt_count == 2);
    vp_manifest_free(&mf);
    printf("test_manifest_parse_diag: ok\n");
}

static void test_manifest_apply_roundtrip(void)
{
    const char *m =
        "section boot raw\n"
        "section cfg metadata\n"
        "meta cfg board=sensor-b\n";
    vp_manifest_t mf;
    vp_manifest_init(&mf);
    int rc = vp_manifest_parse(&mf, m, strlen(m));
    assert(rc == VP_OK);

    vp_writer_t w;
    vp_writer_init(&w);
    rc = vp_manifest_apply(&mf, &w);
    assert(rc == VP_OK);
    vp_manifest_free(&mf);

    uint8_t *bytes; size_t len;
    rc = vp_writer_emit(&w, &bytes, &len);
    assert(rc == VP_OK);

    vp_archive_t *arc = NULL;
    rc = vp_open_archive(bytes, len, &arc);
    assert(rc == VP_OK);
    assert(arc->dir_count == 2);
    /* 'cfg' has meta_count > 0 from the queued default. */
    int found_cfg = -1;
    for (uint32_t i = 0; i < arc->dir_count; i++) {
        if (strcmp(arc->dir[i].name, "cfg") == 0) {
            found_cfg = (int)arc->dir[i].meta_count;
            break;
        }
    }
    assert(found_cfg == 1);

    vp_close_archive(arc);
    free(bytes);
    vp_writer_destroy(&w);
    printf("test_manifest_apply_roundtrip: ok\n");
}

static void test_extract_section(void)
{
    vp_writer_t w;
    vp_writer_init(&w);
    const char *msg = "payload-bytes";
    vp_writer_add_section(&w, "hello", VP_SECTION_RAW, msg, strlen(msg));
    uint8_t *bytes; size_t len;
    vp_writer_emit(&w, &bytes, &len);

    vp_archive_t *arc = NULL;
    int rc = vp_open_archive(bytes, len, &arc);
    assert(rc == VP_OK);

    /* Find the section */
    const vp_dir_entry_t *e = NULL;
    for (uint32_t i = 0; i < arc->dir_count; i++) {
        if (strcmp(arc->dir[i].name, "hello") == 0) { e = &arc->dir[i]; break; }
    }
    assert(e != NULL);
    assert(e->section_size == strlen(msg));
    assert(memcmp(bytes + e->section_offset, msg, e->section_size) == 0);

    vp_close_archive(arc);
    free(bytes);
    vp_writer_destroy(&w);
    printf("test_extract_section: ok\n");
}

int main(void)
{
    test_manifest_parse_simple();
    test_manifest_parse_diag();
    test_manifest_apply_roundtrip();
    test_extract_section();
    printf("test_manifest: all assertions passed\n");
    return 0;
}
