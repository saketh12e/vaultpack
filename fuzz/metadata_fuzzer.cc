#include <cstdint>
#include <cstddef>

#include "vaultpack/metadata.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < (int)sizeof(vp_meta_block_hdr_t))
        return 0;
    vp_meta_block_t *block = nullptr;
    if (vp_read_metadata_block(data, size, &block) == VP_OK) {
        if (block->key_count > 0)
            vp_metadata_lookup(block,
                               block->entries[block->key_count - 1].key);
        vp_process_meta_block(block, nullptr);
        vp_free_metadata_block(block);
    }
    return 0;
}
