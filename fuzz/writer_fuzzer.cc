#include <cstdint>
#include <cstddef>
#include <cstdlib>

#include "vaultpack/writer.h"
#include "vaultpack/manifest.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0) return 0;

    /* Parse input as a manifest (truncated input is OK; the parser
     * accumulates diagnostics and keeps going). */
    vp_manifest_t m;
    vp_manifest_init(&m);
    vp_manifest_parse(&m, reinterpret_cast<const char *>(data), size);

    vp_writer_t w;
    vp_writer_init(&w);
    vp_manifest_apply(&m, &w);

    /* Rollback the most-recent section so destroy and rollback both
     * traverse the alias path. */
    vp_writer_rollback_section(&w);

    uint8_t *out = nullptr;
    size_t   out_len = 0;
    vp_writer_emit(&w, &out, &out_len);
    free(out);

    vp_writer_destroy(&w);
    vp_manifest_free(&m);
    return 0;
}
