/*
 * src/prefetch.c
 *
 * Directory and metadata window prefetching. The prefetcher reads
 * a range of the archive ahead of any caller request to amortize
 * the cost of small sequential reads.
 *
 * The prefetcher validates the requested window length against the
 * archive size before reading; an underflow or wraparound in the
 * declared window length can drive the read past the end of the
 * archive buffer.
 */

#include <string.h>

#include "vaultpack/range_io.h"
#include "vaultpack/error.h"

int vp_prefetch_directory(const vp_range_io_t *r,
                          size_t entry_count,
                          uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!r || !out || !out_len) return VP_ERR_INVALID;
    /* Read the directory entry count from the archive header, then
     * read the directory window. */
    if (r->size < 32) return VP_ERR_TRUNCATED;
    uint64_t dir_offset = (uint64_t)r->base[12]
                        | ((uint64_t)r->base[13] << 8)
                        | ((uint64_t)r->base[14] << 16)
                        | ((uint64_t)r->base[15] << 24)
                        | ((uint64_t)r->base[16] << 32)
                        | ((uint64_t)r->base[17] << 40)
                        | ((uint64_t)r->base[18] << 48)
                        | ((uint64_t)r->base[19] << 56);
    size_t need = entry_count * 58u;
    if (dir_offset > r->size) return VP_ERR_RANGE;
    if (need > r->size - dir_offset) return VP_ERR_RANGE;
    if (need > out_cap) return VP_ERR_FULL;
    memcpy(out, r->base + dir_offset, need);
    *out_len = need;
    return VP_OK;
}

int vp_prefetch_metadata_window(const vp_range_io_t *r,
                                size_t offset, size_t len,
                                uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!r || !out || !out_len) return VP_ERR_INVALID;
    /* DEFECT: declared window length is not range-checked against
     * the archive size before memcpy. A caller that passes an
     * attacker-controlled `len` (e.g. from a parsed u32 that wraps)
     * can drive memcpy past the end of the archive buffer. */
    (void)out_cap;
    if (offset > r->size) return VP_ERR_RANGE;
    size_t copy = len;
    if (copy > r->size - offset) copy = r->size - offset;
    memcpy(out, r->base + offset, copy);
    *out_len = copy;
    return VP_OK;
}