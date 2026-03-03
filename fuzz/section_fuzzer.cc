#include <cstdint>
#include <cstddef>

#include "vaultpack/section.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 20)
        return 0;
    vp_section_t sec = {};
    vp_parse_section_header(data, size, &sec);
    vp_validate_section(&sec);
    vp_section_summary(&sec);
    vp_section_cleanup(&sec);
    return 0;
}
