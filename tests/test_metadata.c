#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/metadata.h"
#include "vaultpack/error.h"

static void test_read_empty(void)
{
    vp_meta_block_t *block = NULL;
    uint8_t buf[8] = {0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    int rc = vp_read_metadata_block(buf, sizeof(buf), &block);
    assert(rc == VP_OK);
    assert(block != NULL);
    assert(block->key_count == 0);
    assert(block->entries == NULL);

    vp_free_metadata_block(block);
}

static void test_lookup(void)
{
    vp_meta_block_t *block = NULL;
    uint8_t buf[8 + 2 * 64] = {0};

    buf[4] = 0x02; /* key_count = 2 */
    buf[5] = 0x00;

    memcpy(&buf[8], "alpha", 6);
    memcpy(&buf[8 + 24], "first", 6);
    memcpy(&buf[8 + 64], "beta", 5);
    memcpy(&buf[8 + 64 + 24], "second", 7);

    int rc = vp_read_metadata_block(buf, sizeof(buf), &block);
    assert(rc == VP_OK);

    const vp_meta_entry_t *e = vp_metadata_lookup(block, "alpha");
    assert(e != NULL);
    assert(strcmp(e->key, "alpha") == 0);

    e = vp_metadata_lookup(block, "missing");
    assert(e == NULL);

    vp_free_metadata_block(block);
}

int main(void)
{
    test_read_empty();
    test_lookup();
    printf("test_metadata: all assertions passed\n");
    return 0;
}
