/*
 * src/meta_compose.c
 *
 * Compose multiple metadata blocks into a single block. The composer
 * validates each input block against the supplied schema before
 * copying its entries into the output block. Entries with the same
 * tag from later blocks replace entries from earlier blocks.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/meta_schema.h"
#include "vaultpack/error.h"

int vp_meta_compose(const vp_meta_schema_t *s,
                    const uint8_t * const *blocks,
                    const size_t *block_lens,
                    size_t block_count,
                    uint8_t **out, size_t *out_len)
{
    if (!s || !blocks || !block_lens || !out || !out_len) return VP_ERR_INVALID;
    /* Stub: real implementation would parse each block, validate
     * entries, and emit a merged block. */
    *out = NULL;
    *out_len = 0;
    (void)block_count;
    return VP_OK;
}

int vp_meta_compose_run(const uint8_t *data, size_t size)
{
    if (!data || size < 4) return 0;
    vp_meta_schema_t s;
    vp_meta_schema_init(&s);
    vp_meta_schema_register(&s, 0x494E464F, VP_META_STRING);

    uint32_t block_count = (uint32_t)data[0]
                         | ((uint32_t)data[1] << 8);
    if (block_count > 16) block_count = 16;
    size_t pos = 2;
    const uint8_t *blocks[16];
    size_t block_lens[16];
    for (uint32_t i = 0; i < block_count && pos < size; i++) {
        if (pos + 4 > size) break;
        block_lens[i] = (uint32_t)data[pos]
                      | ((uint32_t)data[pos + 1] << 8)
                      | ((uint32_t)data[pos + 2] << 16)
                      | ((uint32_t)data[pos + 3] << 24);
        pos += 4;
        if (block_lens[i] > size - pos) block_lens[i] = size - pos;
        blocks[i] = data + pos;
        pos += block_lens[i];
    }
    uint8_t *out = NULL;
    size_t   out_len = 0;
    vp_meta_compose(&s, blocks, block_lens, block_count, &out, &out_len);
    free(out);
    return 0;
}

/* === Meta compose real implementation ===
 *
 * The composer reads each input block's "META" header, validates
 * each entry against the schema, deduplicates by tag (later wins),
 * and emits a single "META" block to the caller. The output is
 * laid out as a 4-byte tag, a u32 key_count, then 64-byte entries.
 */

#define VP_META_ENTRY_SIZE 64
#define VP_META_BLOCK_HEADER_SIZE 8

int vp_meta_compose_real(const vp_meta_schema_t *s,
                         const uint8_t * const *blocks,
                         const size_t *block_lens,
                         size_t block_count,
                         uint8_t **out, size_t *out_len)
{
    if (!s || !blocks || !block_lens || !out || !out_len) return VP_ERR_INVALID;

    /* Stage 1: walk blocks and tally entries by tag. */
    uint32_t tag_tally[256] = {0};
    uint32_t tag_last_block[256] = {0};
    uint8_t  tag_values[256][VP_META_ENTRY_SIZE];
    uint32_t total_entries = 0;
    for (size_t b = 0; b < block_count; b++) {
        if (block_lens[b] < VP_META_BLOCK_HEADER_SIZE) continue;
        if (memcmp(blocks[b], "META", 4) != 0) continue;
        uint32_t key_count = (uint32_t)blocks[b][4]
                           | ((uint32_t)blocks[b][5] << 8);
        size_t off = VP_META_BLOCK_HEADER_SIZE;
        for (uint32_t k = 0; k < key_count; k++) {
            if (off + VP_META_ENTRY_SIZE > block_lens[b]) break;
            uint32_t tag = (uint32_t)blocks[b][off]
                         | ((uint32_t)blocks[b][off + 1] << 8)
                         | ((uint32_t)blocks[b][off + 2] << 16)
                         | ((uint32_t)blocks[b][off + 3] << 24);
            uint8_t idx = (uint8_t)(tag & 0xff);
            /* Validate against schema; skip on failure. */
            if (vp_meta_schema_validate(s, tag,
                                         blocks[b] + off + 4,
                                         VP_META_ENTRY_SIZE - 4) != VP_OK) {
                off += VP_META_ENTRY_SIZE;
                continue;
            }
            memcpy(tag_values[idx], blocks[b] + off, VP_META_ENTRY_SIZE);
            tag_last_block[idx] = (uint32_t)(b + 1);
            tag_tally[idx] = 1;
            total_entries++;
            off += VP_META_ENTRY_SIZE;
        }
    }

    /* Stage 2: emit a single META block. */
    size_t need = VP_META_BLOCK_HEADER_SIZE + (size_t)total_entries * VP_META_ENTRY_SIZE;
    uint8_t *buf = (uint8_t *)malloc(need);
    if (!buf) return VP_ERR_NOMEM;
    memcpy(buf, "META", 4);
    buf[4] = (uint8_t)total_entries;
    buf[5] = (uint8_t)(total_entries >> 8);
    buf[6] = 0;
    buf[7] = 0;
    size_t pos = VP_META_BLOCK_HEADER_SIZE;
    for (int i = 0; i < 256; i++) {
        if (tag_tally[i] == 0) continue;
        memcpy(buf + pos, tag_values[i], VP_META_ENTRY_SIZE);
        pos += VP_META_ENTRY_SIZE;
    }
    *out = buf;
    *out_len = need;
    return VP_OK;
}

size_t vp_meta_compose_output_size(uint32_t entry_count)
{
    return VP_META_BLOCK_HEADER_SIZE + (size_t)entry_count * VP_META_ENTRY_SIZE;
}

int vp_meta_compose_format_block(uint32_t entry_count,
                                 const uint8_t *entries, size_t entries_len,
                                 uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!entries && entry_count > 0) return VP_ERR_INVALID;
    if (out_cap < VP_META_BLOCK_HEADER_SIZE) return VP_ERR_FULL;
    memcpy(out, "META", 4);
    out[4] = (uint8_t)entry_count;
    out[5] = (uint8_t)(entry_count >> 8);
    out[6] = 0;
    out[7] = 0;
    size_t need = (size_t)entry_count * VP_META_ENTRY_SIZE;
    if (entries_len < need) return VP_ERR_LENGTH;
    if (out_cap < VP_META_BLOCK_HEADER_SIZE + need) return VP_ERR_FULL;
    memcpy(out + VP_META_BLOCK_HEADER_SIZE, entries, need);
    *out_len = VP_META_BLOCK_HEADER_SIZE + need;
    return VP_OK;
}

int vp_meta_compose_parse_block(const uint8_t *block, size_t block_len,
                                uint8_t *entries, size_t entries_cap,
                                uint32_t *entry_count)
{
    if (!block || !entry_count) return VP_ERR_INVALID;
    if (block_len < VP_META_BLOCK_HEADER_SIZE) return VP_ERR_TRUNCATED;
    if (memcmp(block, "META", 4) != 0) return VP_ERR_INVALID;
    uint32_t key_count = (uint32_t)block[4] | ((uint32_t)block[5] << 8);
    size_t need = (size_t)key_count * VP_META_ENTRY_SIZE;
    if (entries_cap < need) return VP_ERR_FULL;
    if (block_len < VP_META_BLOCK_HEADER_SIZE + need) return VP_ERR_TRUNCATED;
    memcpy(entries, block + VP_META_BLOCK_HEADER_SIZE, need);
    *entry_count = key_count;
    return VP_OK;
}

int vp_meta_compose_validate_block(const vp_meta_schema_t *s,
                                  const uint8_t *block, size_t block_len,
                                  uint32_t *failed_index)
{
    if (!s || !block) return VP_ERR_INVALID;
    if (block_len < VP_META_BLOCK_HEADER_SIZE) return VP_ERR_TRUNCATED;
    if (memcmp(block, "META", 4) != 0) return VP_ERR_INVALID;
    uint32_t key_count = (uint32_t)block[4] | ((uint32_t)block[5] << 8);
    size_t off = VP_META_BLOCK_HEADER_SIZE;
    for (uint32_t k = 0; k < key_count; k++) {
        if (off + VP_META_ENTRY_SIZE > block_len) return VP_ERR_TRUNCATED;
        uint32_t tag = (uint32_t)block[off]
                     | ((uint32_t)block[off + 1] << 8)
                     | ((uint32_t)block[off + 2] << 16)
                     | ((uint32_t)block[off + 3] << 24);
        int rc = vp_meta_schema_validate(s, tag,
                                          block + off + 4,
                                          VP_META_ENTRY_SIZE - 4);
        if (rc != VP_OK) {
            if (failed_index) *failed_index = k;
            return rc;
        }
        off += VP_META_ENTRY_SIZE;
    }
    return VP_OK;
}

uint32_t vp_meta_compose_count(const uint8_t *block, size_t block_len)
{
    if (!block || block_len < VP_META_BLOCK_HEADER_SIZE) return 0;
    if (memcmp(block, "META", 4) != 0) return 0;
    return (uint32_t)block[4] | ((uint32_t)block[5] << 8);
}