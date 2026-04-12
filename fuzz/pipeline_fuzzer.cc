/*
 * fuzz/pipeline_fuzzer.cc
 *
 * Section decompression pipeline harness. Feeds the input buffer as
 * either a raw section payload or a chained LZ4X/HC01 block stream,
 * runs it through the pipeline, and lets the decoder surface any
 * truncation, length, or reference validation failures.
 */
#include <cstdint>
#include <cstddef>
#include <cstdlib>

#include "vaultpack/section_pipeline.h"
#include "vaultpack/decompress.h"
#include "vaultpack/crc32.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!data || size < 8) return 0;

    uint32_t codec = (uint32_t)data[0]
                   | ((uint32_t)data[1] << 8)
                   | ((uint32_t)data[2] << 16)
                   | ((uint32_t)data[3] << 24);
    uint32_t raw_size = (uint32_t)data[4]
                      | ((uint32_t)data[5] << 8)
                      | ((uint32_t)data[6] << 16)
                      | ((uint32_t)data[7] << 24);
    if (raw_size == 0 || raw_size > (1u << 20)) return 0;

    uint8_t *out = (uint8_t *)malloc(raw_size);
    if (!out) return 0;

    size_t produced = 0;
    int rc;
    if (codec == VP_CODEC_TAG_LZ4X) {
        rc = vp_decompress_lz4x(data, size, out, raw_size, &produced);
    } else if (codec == VP_CODEC_TAG_HC01) {
        rc = vp_decompress_hc01(data, size, out, raw_size, &produced);
    } else {
        rc = 0;
    }

    /* Pipeline integration: also run a full pipeline with the same
     * payload and a synthetic CRC. We do not care about the CRC
     * matching, only that the decoder portion does not crash. */
    vp_section_pipeline_t p;
    vp_section_pipeline_init(&p, codec, raw_size, size - 8, 0);
    uint8_t *pout = nullptr;
    size_t   plen = 0;
    int prc = vp_section_pipeline_run(&p, data + 8, size - 8, &pout, &plen);
    free(pout);
    /* On error paths the pipeline frees its scratch but leaves
     * the pointer non-NULL; destroy will free it again. */
    vp_section_pipeline_destroy(&p);
    (void)prc;

    free(out);
    (void)rc;
    return 0;
}