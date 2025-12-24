#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/extract.h"
#include "vaultpack/section.h"
#include "vaultpack/error.h"

static void test_extract_ok(void)
{
    uint8_t hdr[VP_SECTION_HDR_SIZE + 8] = {0};
    memcpy(hdr, VP_MAGIC_SECT_BYTES, 4);
    hdr[4] = VP_SECTION_RAW;
    hdr[7] = 0x00;

    uint32_t comp_size = 8;
    hdr[12] = (uint8_t)(comp_size);
    hdr[13] = (uint8_t)(comp_size >> 8);
    hdr[14] = (uint8_t)(comp_size >> 16);
    hdr[15] = (uint8_t)(comp_size >> 24);

    memcpy(hdr + VP_SECTION_HDR_SIZE, "ABCDEFGH", 8);

    vp_extract_ctx_t ctx = {0};
    ctx.out_size = 4096;

    int rc = vp_extract_section(&ctx, hdr, sizeof(hdr));
    assert(rc == VP_OK);
    assert(ctx.out_len == 8);
    assert(memcmp(ctx.out_buf, "ABCDEFGH", 8) == 0);

    vp_extract_cleanup(&ctx);
    assert(ctx.out_buf == NULL);
}

int main(void)
{
    test_extract_ok();
    printf("test_extract: all assertions passed\n");
    return 0;
}
