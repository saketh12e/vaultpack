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
    vp_stream_window_t w = vp_stream_window(&s, 8);
    if (w.ptr && vp_stream_remaining(&s) >= 8) {
        /* Force the scratch buffer to grow. The second window is larger
         * than the first so vp_stream_reserve_scratch must realloc.
         * If realloc relocates the buffer, w.ptr now points into the
         * freed old allocation; replay_window will read freed memory. */
        vp_stream_window_t w2 = vp_stream_window(&s, 256);
        (void)w2;
        uint8_t buf[8];
        vp_stream_replay_window(&s, &w, buf, sizeof(buf));
    } else {
        uint8_t buf[8];
        vp_stream_replay_window(&s, &w, buf, sizeof(buf));
    }
    return 0;
}
