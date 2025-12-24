#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/archive.h"
#include "vaultpack/error.h"

static void test_open_bad_magic(void)
{
    uint8_t buf[VP_ARCHIVE_HDR_SIZE] = {0};
    vp_archive_t *arc = NULL;
    int rc = vp_open_archive(buf, sizeof(buf), &arc);
    assert(rc == VP_ERR_INVALID);
}

static void test_open_truncated(void)
{
    uint8_t buf[10] = {0};
    vp_archive_t *arc = NULL;
    int rc = vp_open_archive(buf, sizeof(buf), &arc);
    assert(rc == VP_ERR_TRUNCATED);
}

static void test_open_zero_dir(void)
{
    uint8_t buf[VP_ARCHIVE_HDR_SIZE] = {0};
    memcpy(buf, "VPAK", 4);
    buf[4] = 0x01;
    buf[5] = 0x00;
    buf[8] = 0x00; buf[9] = 0x00; buf[10] = 0x00; buf[11] = 0x00;

    vp_archive_t *arc = NULL;
    int rc = vp_open_archive(buf, sizeof(buf), &arc);
    assert(rc == VP_OK);
    assert(arc != NULL);
    assert(arc->dir_count == 0);

    vp_close_archive(arc);
}

int main(void)
{
    test_open_bad_magic();
    test_open_truncated();
    test_open_zero_dir();
    printf("test_archive: all assertions passed\n");
    return 0;
}
