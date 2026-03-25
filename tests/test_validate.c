#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vaultpack/vaultpack.h"

static void test_validate_ok(void) {
    vp_writer_t w; vp_writer_init(&w);
    vp_writer_add_section(&w, "ok", VP_SECTION_RAW, "abc", 3);
    uint8_t *bytes; size_t len;
    vp_writer_emit(&w, &bytes, &len);
    vp_archive_t *arc = NULL;
    int rc = vp_open_archive(bytes, len, &arc);
    assert(rc == VP_OK);
    vp_validate_report_t r; vp_validate_report_init(&r);
    rc = vp_validate_archive(arc, &r);
    assert(rc == VP_OK);
    assert(r.issue_count == 0);
    vp_validate_report_free(&r);
    vp_close_archive(arc);
    free(bytes);
    vp_writer_destroy(&w);
    printf("test_validate_ok: ok\n");
}

int main(void) {
    test_validate_ok();
    printf("test_validate: all assertions passed\n");
    return 0;
}
