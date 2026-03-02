#include <cstdint>
#include <cstddef>

#include "vaultpack/vaultpack.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 32)
        return 0;
    vp_archive_t *arc = nullptr;
    int rc = vp_open_archive(data, size, &arc);
    if (rc == VP_OK)
        vp_close_archive(arc);
    return 0;
}
