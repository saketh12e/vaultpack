#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/vaultpack.h"

static void test_writer_basic_roundtrip(void)
{
    vp_writer_t w;
    assert(vp_writer_init(&w) == VP_OK);

    const char *payload = "hello world";
    assert(vp_writer_add_section(&w, "boot", VP_SECTION_RAW,
                                 payload, strlen(payload)) == VP_OK);

    uint8_t *bytes;
    size_t len;
    assert(vp_writer_emit(&w, &bytes, &len) == VP_OK);
    assert(len >= 32);
    assert(memcmp(bytes, "VPAK", 4) == 0);
    free(bytes);

    vp_writer_destroy(&w);
    printf("test_writer_basic_roundtrip: ok\n");
}

static void test_writer_multi_section(void)
{
    vp_writer_t w;
    vp_writer_init(&w);

    assert(vp_writer_add_section(&w, "a", VP_SECTION_RAW, "AAAAA", 5) == VP_OK);
    assert(vp_writer_add_section(&w, "b", VP_SECTION_RAW, "BBBBB", 5) == VP_OK);
    assert(vp_writer_add_section(&w, "c", VP_SECTION_RAW, "CCCCC", 5) == VP_OK);

    uint8_t *bytes;
    size_t len;
    assert(vp_writer_emit(&w, &bytes, &len) == VP_OK);

    /* Round-trip parse with vp_open_archive. */
    vp_archive_t *arc = NULL;
    int rc = vp_open_archive(bytes, len, &arc);
    assert(rc == VP_OK);
    assert(arc->dir_count == 3);
    vp_close_archive(arc);

    free(bytes);
    vp_writer_destroy(&w);
    printf("test_writer_multi_section: ok\n");
}

static void test_writer_duplicate_rejected(void)
{
    vp_writer_t w;
    vp_writer_init(&w);

    assert(vp_writer_add_section(&w, "x", VP_SECTION_RAW, "x", 1) == VP_OK);
    int rc = vp_writer_add_section(&w, "x", VP_SECTION_RAW, "y", 1);
    assert(rc == VP_ERR_INVALID);

    vp_writer_destroy(&w);
    printf("test_writer_duplicate_rejected: ok\n");
}

static void test_writer_with_metadata(void)
{
    vp_writer_t w;
    vp_writer_init(&w);

    assert(vp_writer_add_section(&w, "boot", VP_SECTION_RAW, "boot", 4) == VP_OK);

    uint8_t tag[4] = { 'I', 'N', 'F', 'O' };
    vp_writer_meta_entry_t entries[2];
    memset(entries, 0, sizeof(entries));
    strncpy(entries[0].key, "board", 23);
    strncpy(entries[0].value, "sensor-a", 39);
    strncpy(entries[1].key, "version", 23);
    strncpy(entries[1].value, "7", 39);
    assert(vp_writer_add_metadata(&w, tag, entries, 2) == VP_OK);

    uint8_t *bytes;
    size_t len;
    assert(vp_writer_emit(&w, &bytes, &len) == VP_OK);

    vp_archive_t *arc = NULL;
    int rc = vp_open_archive(bytes, len, &arc);
    assert(rc == VP_OK);
    assert(arc->meta_total == 2);
    vp_close_archive(arc);

    free(bytes);
    vp_writer_destroy(&w);
    printf("test_writer_with_metadata: ok\n");
}

static void test_writer_reset(void)
{
    vp_writer_t w;
    vp_writer_init(&w);

    assert(vp_writer_add_section(&w, "a", VP_SECTION_RAW, "a", 1) == VP_OK);
    assert(vp_writer_add_section(&w, "b", VP_SECTION_RAW, "b", 1) == VP_OK);
    vp_writer_reset(&w);

    /* After reset, can re-add with no leak. */
    assert(vp_writer_add_section(&w, "x", VP_SECTION_RAW, "xx", 2) == VP_OK);

    vp_writer_destroy(&w);
    printf("test_writer_reset: ok\n");
}

int main(void)
{
    test_writer_basic_roundtrip();
    test_writer_multi_section();
    test_writer_duplicate_rejected();
    test_writer_with_metadata();
    test_writer_reset();
    printf("test_writer: all assertions passed\n");
    return 0;
}
