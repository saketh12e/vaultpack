#include <cstdint>
#include <cstddef>

#include "vaultpack/manifest.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    vp_manifest_t m;
    vp_manifest_init(&m);
    vp_manifest_parse(&m, reinterpret_cast<const char *>(data), size);
    vp_manifest_free(&m);
    return 0;
}
