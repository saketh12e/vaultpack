#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/section.h"
#include "vaultpack/error.h"

static void test_parse_raw(void)
{
    uint8_t buf[VP_SECTION_HDR_SIZE] = {0};
    memcpy(buf, VP_MAGIC_SECT_BYTES, 4);
    buf[4] = VP_SECTION_RAW;
    buf[7] = 0x00; /* flags */

    vp_section_t sec;
    int rc = vp_parse_section_header(buf, sizeof(buf), &sec);
    assert(rc == VP_OK);
    assert(sec.type == VP_SECTION_RAW);
    assert(sec.comp_ctx == NULL);

    rc = vp_validate_section(&sec);
    assert(rc == VP_OK);

    vp_section_summary(&sec);
    vp_section_cleanup(&sec);
}

static void test_parse_compressed_ok(void)
{
    uint8_t buf[VP_SECTION_HDR_SIZE] = {0};
    memcpy(buf, VP_MAGIC_SECT_BYTES, 4);
    buf[4] = VP_SECTION_COMPRESSED;
    buf[5] = 0x01; /* lz4 */
    buf[6] = 0x03; /* level 3 */

    vp_section_t sec;
    int rc = vp_parse_section_header(buf, sizeof(buf), &sec);
    assert(rc == VP_OK);
    assert(sec.comp_ctx != NULL);
    assert(sec.comp_ctx->method == 0x01);
    assert(sec.comp_ctx->level == 0x03);

    rc = vp_validate_section(&sec);
    assert(rc == VP_OK);

    vp_section_cleanup(&sec);
}

static void test_truncated_input(void)
{
    vp_section_t sec;
    int rc = vp_parse_section_header(NULL, 0, &sec);
    assert(rc == VP_ERR_TRUNCATED);

    uint8_t buf[10] = {0};
    rc = vp_parse_section_header(buf, sizeof(buf), &sec);
    assert(rc == VP_ERR_TRUNCATED);
}

int main(void)
{
    test_parse_raw();
    test_parse_compressed_ok();
    test_truncated_input();
    printf("test_section: all assertions passed\n");
    return 0;
}
