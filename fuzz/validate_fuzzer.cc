#include <cstdint>
#include <cstddef>
#include <cstdlib>

#include "vaultpack/validate.h"
#include "vaultpack/archive.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 32) return 0;
    vp_archive_t *arc = nullptr;
    if (vp_open_archive(data, size, &arc) != VP_OK) return 0;

    vp_validate_report_t r;
    vp_validate_report_init(&r);
    vp_validate_archive(arc, &r);
    vp_validate_report_free(&r);

    /* Also exercise the interval collector directly. */
    uint64_t *iv = nullptr;
    uint32_t  n = 0;
    vp_validate_collect_intervals(arc, &iv, &n);
    free(iv);

    vp_close_archive(arc);
    return 0;
}
