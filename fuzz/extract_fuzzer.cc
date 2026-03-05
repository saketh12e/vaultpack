#include <cstdint>
#include <cstddef>

#include "vaultpack/extract.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 20)
        return 0;
    vp_extract_ctx_t ctx = {};
    ctx.out_size = 4096;
    vp_extract_section(&ctx, data, size);
    vp_extract_cleanup(&ctx);
    return 0;
}
