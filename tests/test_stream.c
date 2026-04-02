#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/stream.h"

static void test_stream_basic(void)
{
    uint8_t buf[16] = {0};
    for (int i = 0; i < 16; i++) buf[i] = (uint8_t)i;

    vp_stream_t s;
    vp_stream_init(&s, buf, sizeof(buf));

    assert(vp_stream_remaining(&s) == 16);
    assert(!vp_stream_at_end(&s));

    assert(vp_stream_read_u8(&s) == 0x00);
    assert(vp_stream_read_u16(&s) == 0x0201);
    assert(vp_stream_read_u32(&s) == 0x06050403);
    uint64_t v = vp_stream_read_u64(&s);
    assert(v == 0x0e0d0c0b0a090807ULL);

    assert(vp_stream_status(&s) == VP_STREAM_OK);
    assert(vp_stream_remaining(&s) == 1);
    vp_stream_read_u8(&s);
    assert(vp_stream_at_end(&s));
    printf("test_stream_basic: ok\n");
}

static void test_stream_truncated(void)
{
    uint8_t buf[3] = {1, 2, 3};
    vp_stream_t s;
    vp_stream_init(&s, buf, sizeof(buf));

    vp_stream_read_u32(&s); /* not enough bytes */
    assert(vp_stream_status(&s) == VP_STREAM_ERR_TRUNCATED);

    /* further reads keep returning zero */
    assert(vp_stream_read_u8(&s) == 0);
    assert(vp_stream_read_u16(&s) == 0);
    printf("test_stream_truncated: ok\n");
}

static void test_stream_bookmark(void)
{
    uint8_t buf[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    vp_stream_t s;
    vp_stream_init(&s, buf, sizeof(buf));

    vp_stream_read_u8(&s);                          /* reads 10, pos=1 */
    vp_stream_bookmark_t bm = vp_stream_bookmark(&s);
    vp_stream_read_u16(&s);                         /* reads 20,30, pos=3 */
    assert(vp_stream_read_u8(&s) == 40);            /* reads buf[3] = 40 */
    vp_stream_restore(&s, bm);                      /* pos back to 1 */
    assert(vp_stream_read_u8(&s) == 20);            /* reads buf[1] = 20 */
    printf("test_stream_bookmark: ok\n");
}

static void test_stream_scratch_window(void)
{
    uint8_t buf[64];
    for (int i = 0; i < 64; i++) buf[i] = (uint8_t)(0x40 + i);

    vp_stream_t s;
    vp_stream_init(&s, buf, sizeof(buf));

    vp_stream_window_t w = vp_stream_window(&s, 16);
    /* The window's underlying storage is implementation-defined. What
     * matters is that we can either copy out of it (when the API
     * provides a copy) or replay through it via vp_stream_replay_window.
     * The replay path is the safe way to read window data. */
    assert(w.len == 16);

    uint8_t out[16];
    /* Replay is allowed only if the underlying buffer is still live
     * (no realloc happened between window and replay). */
    assert(vp_stream_replay_window(&s, &w, out, sizeof(out)));

    printf("test_stream_scratch_window: ok\n");
}

int main(void)
{
    test_stream_basic();
    test_stream_truncated();
    test_stream_bookmark();
    test_stream_scratch_window();
    printf("test_stream: all assertions passed\n");
    return 0;
}
