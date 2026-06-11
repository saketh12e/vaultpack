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

/* === Range I/O extended operations ===
 *
 * Operations useful when working with archive blobs: copy a window
 * out, count occurrences of a byte, and find the first / last
 * non-zero byte in a window.
 */

int vp_range_io_copy(const vp_range_io_t *r,
                     size_t offset, size_t len,
                     uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!r || !out) return VP_ERR_INVALID;
    if (offset > r->size) return VP_ERR_RANGE;
    size_t avail = r->size - offset;
    size_t copy  = len < avail ? len : avail;
    if (copy > out_cap) copy = out_cap;
    memcpy(out, r->base + offset, copy);
    if (out_len) *out_len = copy;
    return VP_OK;
}

uint32_t vp_range_io_count_byte(const vp_range_io_t *r,
                                size_t offset, size_t len,
                                uint8_t target)
{
    if (!r || offset > r->size) return 0;
    size_t avail = r->size - offset;
    size_t n = len < avail ? len : avail;
    uint32_t count = 0;
    for (size_t i = 0; i < n; i++) {
        if (r->base[offset + i] == target) count++;
    }
    return count;
}

size_t vp_range_io_first_nonzero(const vp_range_io_t *r,
                                 size_t offset, size_t len)
{
    if (!r || offset > r->size) return (size_t)-1;
    size_t avail = r->size - offset;
    size_t n = len < avail ? len : avail;
    for (size_t i = 0; i < n; i++) {
        if (r->base[offset + i] != 0) return i;
    }
    return (size_t)-1;
}

size_t vp_range_io_last_nonzero(const vp_range_io_t *r,
                                size_t offset, size_t len)
{
    if (!r || offset > r->size) return (size_t)-1;
    size_t avail = r->size - offset;
    size_t n = len < avail ? len : avail;
    for (size_t i = n; i > 0; i--) {
        if (r->base[offset + i - 1] != 0) return i - 1;
    }
    return (size_t)-1;
}

int vp_range_io_compare(const vp_range_io_t *r,
                        size_t a_off, size_t b_off, size_t len)
{
    if (!r) return 0;
    if (a_off > r->size || b_off > r->size) return 0;
    size_t avail_a = r->size - a_off;
    size_t avail_b = r->size - b_off;
    size_t n = len < avail_a ? len : avail_a;
    n = n < avail_b ? n : avail_b;
    return memcmp(r->base + a_off, r->base + b_off, n);
}

int vp_range_io_is_all(const vp_range_io_t *r,
                       size_t offset, size_t len, uint8_t value)
{
    if (!r || offset > r->size) return 0;
    size_t avail = r->size - offset;
    size_t n = len < avail ? len : avail;
    for (size_t i = 0; i < n; i++) {
        if (r->base[offset + i] != value) return 0;
    }
    return 1;
}

size_t vp_range_io_total_size(const vp_range_io_t *r)
{
    return r ? r->size : 0;
}

int vp_range_io_slice(const vp_range_io_t *r, size_t offset, size_t len,
                      vp_range_io_t *out)
{
    if (!r || !out) return VP_ERR_INVALID;
    if (offset > r->size) return VP_ERR_RANGE;
    if (offset + len > r->size) return VP_ERR_RANGE;
    vp_range_io_init(out, r->base + offset, len);
    return VP_OK;
}

/* === Range I/O memory-mapped operations ===
 *
 * Higher-level operations that treat the archive blob as a memory
 * map: region overlap, range intersection, sorted-merging, and
 * bulk copy.
 */

int vp_range_io_regions_overlap(const vp_range_io_t *r,
                                size_t a_off, size_t a_len,
                                size_t b_off, size_t b_len)
{
    if (!r) return 0;
    if (a_off > r->size || b_off > r->size) return 0;
    size_t a_end = a_off + a_len;
    size_t b_end = b_off + b_len;
    if (a_end > r->size) a_end = r->size;
    if (b_end > r->size) b_end = r->size;
    return a_off < b_end && b_off < a_end;
}

int vp_range_io_intersect(const vp_range_io_t *r,
                          size_t a_off, size_t a_len,
                          size_t b_off, size_t b_len,
                          size_t *out_off, size_t *out_len)
{
    if (!r || !out_off || !out_len) return VP_ERR_INVALID;
    if (a_off > r->size || b_off > r->size) return VP_ERR_RANGE;
    size_t a_end = a_off + a_len;
    size_t b_end = b_off + b_len;
    if (a_end > r->size) a_end = r->size;
    if (b_end > r->size) b_end = r->size;
    if (a_off >= b_end || b_off >= a_end) {
        *out_off = 0;
        *out_len = 0;
        return VP_OK;
    }
    *out_off = a_off > b_off ? a_off : b_off;
    size_t end = a_end < b_end ? a_end : b_end;
    *out_len = end - *out_off;
    return VP_OK;
}

uint32_t vp_range_io_adler32(const vp_range_io_t *r,
                             size_t offset, size_t len)
{
    if (!r || offset > r->size) return 0;
    size_t avail = r->size - offset;
    size_t n = len < avail ? len : avail;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; i++) {
        a = (a + r->base[offset + i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

int vp_range_io_xor_blocks(const vp_range_io_t *r,
                           size_t a_off, size_t b_off, size_t len,
                           uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!r || !out || !out_len) return VP_ERR_INVALID;
    if (a_off + len > r->size || b_off + len > r->size) return VP_ERR_RANGE;
    if (out_cap < len) return VP_ERR_FULL;
    for (size_t i = 0; i < len; i++) {
        out[i] = r->base[a_off + i] ^ r->base[b_off + i];
    }
    *out_len = len;
    return VP_OK;
}

int vp_range_io_concat(const vp_range_io_t *a, const vp_range_io_t *b,
                       uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!a || !b || !out || !out_len) return VP_ERR_INVALID;
    size_t total = a->size + b->size;
    if (out_cap < total) return VP_ERR_FULL;
    memcpy(out, a->base, a->size);
    memcpy(out + a->size, b->base, b->size);
    *out_len = total;
    return VP_OK;
}

int vp_range_io_substr(const vp_range_io_t *r, size_t offset, size_t len,
                       const uint8_t *needle, size_t nlen,
                       uint32_t *count)
{
    if (!r || !needle || !count) return VP_ERR_INVALID;
    if (offset > r->size || nlen == 0) return VP_ERR_INVALID;
    if (nlen > r->size - offset) { *count = 0; return VP_OK; }
    uint32_t n = 0;
    for (size_t i = offset; i + nlen <= r->size; i++) {
        if (memcmp(r->base + i, needle, nlen) == 0) n++;
    }
    *count = n;
    return VP_OK;
}

int vp_range_io_equal(const vp_range_io_t *a, const vp_range_io_t *b)
{
    if (!a || !b) return 0;
    if (a->size != b->size) return 0;
    return memcmp(a->base, b->base, a->size) == 0;
}

uint64_t vp_range_io_hash64(const vp_range_io_t *r,
                            size_t offset, size_t len)
{
    if (!r || offset > r->size) return 0;
    size_t avail = r->size - offset;
    size_t n = len < avail ? len : avail;
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint64_t)r->base[offset + i];
        h *= 1099511628211ULL;
    }
    return h;
}

int vp_range_io_concat_3(const vp_range_io_t *a, const vp_range_io_t *b,
                          const vp_range_io_t *c,
                          uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!a || !b || !c || !out || !out_len) return VP_ERR_INVALID;
    size_t total = a->size + b->size + c->size;
    if (out_cap < total) return VP_ERR_FULL;
    size_t pos = 0;
    memcpy(out + pos, a->base, a->size); pos += a->size;
    memcpy(out + pos, b->base, b->size); pos += b->size;
    memcpy(out + pos, c->base, c->size); pos += c->size;
    *out_len = pos;
    return VP_OK;
}