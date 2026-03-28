#include <cstdint>
#include <cstddef>

#include "vaultpack/stream.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!data || size == 0) return 0;
    vp_stream_t s;
    vp_stream_init(&s, data, size);

    /* Drive a few primitives that exercise the bounded-cursor paths. */
    (void)vp_stream_read_u8(&s);
    (void)vp_stream_read_u16(&s);
    (void)vp_stream_read_u32(&s);
    (void)vp_stream_read_u64(&s);

    vp_stream_bookmark_t bm = vp_stream_bookmark(&s);
    (void)vp_stream_read_bytes(&s, nullptr, 0);
    vp_stream_restore(&s, bm);

    /* Scratch-window round-trip. */
    vp_stream_window_t w = vp_stream_window(&s, 16);
    uint8_t buf[16];
    vp_stream_replay_window(&s, &w, buf, sizeof(buf));
    return 0;
}
