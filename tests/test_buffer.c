#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/buffer.h"

static void test_buffer_append(void)
{
    vp_buffer_t b;
    vp_buffer_init(&b);
    assert(vp_buffer_append(&b, "hello", 5));
    assert(vp_buffer_append_byte(&b, ' '));
    assert(vp_buffer_append(&b, "world", 5));
    assert(b.len == 11);
    assert(memcmp(b.data, "hello world", 11) == 0);
    vp_buffer_destroy(&b);
    printf("test_buffer_append: ok\n");
}

static void test_buffer_grow(void)
{
    vp_buffer_t b;
    vp_buffer_init(&b);
    /* Force several grows */
    for (int i = 0; i < 1000; i++) {
        assert(vp_buffer_append_u32_le(&b, (uint32_t)i));
    }
    assert(b.len == 4000);
    /* Verify little-endian encoding */
    uint8_t *p = b.data;
    assert(p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 0);
    assert(p[4] == 1);
    vp_buffer_destroy(&b);
    printf("test_buffer_grow: ok\n");
}

static void test_buffer_patch(void)
{
    vp_buffer_t b;
    vp_buffer_init(&b);
    assert(vp_buffer_append_zeros(&b, 16));
    assert(vp_buffer_patch_u32_le(&b, 4, 0xDEADBEEFu));
    assert(vp_buffer_patch_u16_le(&b, 8, 0xBEEFu));
    assert(vp_buffer_patch_u64_le(&b, 0, 0x1122334455667788ULL));

    assert(b.data[0] == 0x88 && b.data[7] == 0x11);
    assert(b.data[8] == 0xEF && b.data[9] == 0xBE);
    assert(b.data[4] == 0xEF && b.data[5] == 0xBE && b.data[6] == 0xAD && b.data[7] == 0xDE);
    vp_buffer_destroy(&b);
    printf("test_buffer_patch: ok\n");
}

static void test_buffer_detach(void)
{
    vp_buffer_t b;
    vp_buffer_init(&b);
    vp_buffer_append(&b, "owned", 5);
    size_t len;
    uint8_t *p = vp_buffer_detach(&b, &len);
    assert(len == 5);
    assert(memcmp(p, "owned", 5) == 0);
    assert(b.data == NULL);
    assert(b.len == 0);
    free(p);
    printf("test_buffer_detach: ok\n");
}

int main(void)
{
    test_buffer_append();
    test_buffer_grow();
    test_buffer_patch();
    test_buffer_detach();
    printf("test_buffer: all assertions passed\n");
    return 0;
}
