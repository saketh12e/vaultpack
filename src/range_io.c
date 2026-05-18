/*
 * src/range_io.c
 *
 * Bounded range reader over an in-memory archive blob.
 */

#include <string.h>

#include "vaultpack/range_io.h"
#include "vaultpack/error.h"

void vp_range_io_init(vp_range_io_t *r, const uint8_t *base, size_t size)
{
    if (!r) return;
    r->base = base;
    r->size = size;
}

int vp_range_io_read(const vp_range_io_t *r,
                     size_t offset, size_t len,
                     uint8_t *out, size_t *out_len)
{
    if (!r || !out || !out_len) return VP_ERR_INVALID;
    if (offset > r->size) return VP_ERR_RANGE;
    size_t avail = r->size - offset;
    size_t copy  = len < avail ? len : avail;
    memcpy(out, r->base + offset, copy);
    *out_len = copy;
    return VP_OK;
}

int vp_range_io_peek(const vp_range_io_t *r,
                     size_t offset, const uint8_t **ptr, size_t len)
{
    if (!r || !ptr) return VP_ERR_INVALID;
    if (offset > r->size) return VP_ERR_RANGE;
    if (offset + len > r->size) return VP_ERR_RANGE;
    *ptr = r->base + offset;
    return VP_OK;
}

int vp_range_io_run(const uint8_t *data, size_t size)
{
    if (!data || size < 8) return 0;
    vp_range_io_t r;
    vp_range_io_init(&r, data, size);

    uint32_t op_count = (uint32_t)data[0]
                      | ((uint32_t)data[1] << 8);
    if (op_count > 64) op_count = 64;
    size_t pos = 2;

    for (uint32_t i = 0; i < op_count && pos + 12 <= size; i++) {
        uint32_t off  = (uint32_t)data[pos]
                      | ((uint32_t)data[pos + 1] << 8)
                      | ((uint32_t)data[pos + 2] << 16)
                      | ((uint32_t)data[pos + 3] << 24);
        uint32_t len  = (uint32_t)data[pos + 4]
                      | ((uint32_t)data[pos + 5] << 8)
                      | ((uint32_t)data[pos + 6] << 16)
                      | ((uint32_t)data[pos + 7] << 24);
        pos += 8;
        uint8_t out[256];
        size_t  got = 0;
        if (len > sizeof(out)) len = sizeof(out);
        vp_range_io_read(&r, off, len, out, &got);
        pos += 4;  /* reserved */
    }
    return 0;
}

/* === Range I/O additional operations ===
 *
 * The range reader supports iteration over a sequence of fixed-size
 * records, search for a sentinel byte pattern, and checksum over
 * a range window. These are convenience helpers used by callers
 * that do not want to manage their own offset bookkeeping.
 */

uint32_t vp_range_io_crc32(const vp_range_io_t *r,
                           size_t offset, size_t len)
{
    if (!r || offset > r->size) return 0;
    size_t avail = r->size - offset;
    size_t n = len < avail ? len : avail;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        crc ^= r->base[offset + i];
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1)));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

int vp_range_io_find(const vp_range_io_t *r,
                     size_t offset, const uint8_t *needle, size_t nlen,
                     size_t *found)
{
    if (!r || !needle || !found || nlen == 0) return VP_ERR_INVALID;
    if (offset > r->size) return VP_ERR_RANGE;
    if (nlen > r->size - offset) return VP_ERR_NOTFOUND;
    for (size_t i = offset; i + nlen <= r->size; i++) {
        if (memcmp(r->base + i, needle, nlen) == 0) {
            *found = i;
            return VP_OK;
        }
    }
    return VP_ERR_NOTFOUND;
}

int vp_range_io_iter_records(const vp_range_io_t *r,
                             size_t offset, size_t rec_size,
                             size_t count, vp_range_record_cb cb,
                             void *user)
{
    if (!r || rec_size == 0) return VP_ERR_INVALID;
    if (offset > r->size) return VP_ERR_RANGE;
    size_t avail = r->size - offset;
    size_t n = count * rec_size;
    if (n > avail) n = avail - (avail % rec_size);
    for (size_t i = 0; i < n; i += rec_size) {
        if (cb && !cb(r->base + offset + i, rec_size, user)) return VP_ERR_TRUNCATED;
    }
    return VP_OK;
}