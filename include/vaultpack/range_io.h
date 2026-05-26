/*
 * include/vaultpack/range_io.h
 *
 * Bounded range reader over an in-memory archive blob. The reader
 * does not retain a pointer to the underlying blob across calls; each
 * read returns a pointer into the caller-supplied buffer.
 *
 * Range I/O supports two operations:
 *   - vp_range_open: bind a range to a base address and a length
 *   - vp_range_read: copy `len` bytes starting at `offset` to `out`
 *
 * The reader rejects reads past the bound; callers must split
 * requests into chunks they can satisfy individually.
 */
#ifndef VAULTPACK_RANGE_IO_H
#define VAULTPACK_RANGE_IO_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vp_range_io {
    const uint8_t *base;
    size_t         size;
} vp_range_io_t;

void vp_range_io_init(vp_range_io_t *r, const uint8_t *base, size_t size);

int  vp_range_io_read(const vp_range_io_t *r,
                      size_t offset, size_t len,
                      uint8_t *out, size_t *out_len);

int  vp_range_io_peek(const vp_range_io_t *r,
                      size_t offset, const uint8_t **ptr, size_t len);

/* Test entry point used by the bridge. */
int vp_range_io_run(const uint8_t *data, size_t size);

/* Additional operations. */
uint32_t vp_range_io_crc32(const vp_range_io_t *r, size_t offset, size_t len);
int vp_range_io_find(const vp_range_io_t *r,
                     size_t offset, const uint8_t *needle, size_t nlen,
                     size_t *found);

typedef int (*vp_range_record_cb)(const uint8_t *rec, size_t size, void *user);

int vp_range_io_iter_records(const vp_range_io_t *r,
                             size_t offset, size_t rec_size,
                             size_t count, vp_range_record_cb cb,
                             void *user);

/* Range I/O extended operations. */
int     vp_range_io_copy(const vp_range_io_t *r, size_t offset, size_t len,
                         uint8_t *out, size_t out_cap, size_t *out_len);
uint32_t vp_range_io_count_byte(const vp_range_io_t *r,
                                size_t offset, size_t len, uint8_t target);
size_t  vp_range_io_first_nonzero(const vp_range_io_t *r,
                                  size_t offset, size_t len);
size_t  vp_range_io_last_nonzero(const vp_range_io_t *r,
                                 size_t offset, size_t len);
int     vp_range_io_compare(const vp_range_io_t *r,
                            size_t a_off, size_t b_off, size_t len);
int     vp_range_io_is_all(const vp_range_io_t *r,
                           size_t offset, size_t len, uint8_t value);
size_t  vp_range_io_total_size(const vp_range_io_t *r);
int     vp_range_io_slice(const vp_range_io_t *r, size_t offset, size_t len,
                          vp_range_io_t *out);

/* Archive view: open an archive via the range reader and read out
 * its directory. The view borrows the section payloads from the
 * range reader. */
typedef struct vp_view_entry {
    char     name[32];
    uint64_t offset;
    uint32_t size;
    uint8_t  type;
} vp_view_entry_t;

typedef struct vp_archive_view {
    vp_view_entry_t *entries;
    uint32_t         entry_count;
    uint64_t         dir_offset;
} vp_archive_view_t;

void vp_archive_view_init(vp_archive_view_t *v);
void vp_archive_view_destroy(vp_archive_view_t *v);
int  vp_archive_view_open(vp_archive_view_t *v, const vp_range_io_t *r);

/* Prefetch helpers (declared here to keep the cache headers slim). */
int  vp_prefetch_directory(const vp_range_io_t *r,
                           size_t entry_count,
                           uint8_t *out, size_t out_cap, size_t *out_len);
int  vp_prefetch_metadata_window(const vp_range_io_t *r,
                                 size_t offset, size_t len,
                                 uint8_t *out, size_t out_cap, size_t *out_len);

/* Pin tracker. */
typedef struct vp_cache_pin {
    struct vp_section_cache *cache;
    char                     name[64];
} vp_cache_pin_t;

int  vp_cache_pin_acquire(struct vp_section_cache *c,
                          const char *name, vp_cache_pin_t *out_pin);
int  vp_cache_pin_release(vp_cache_pin_t *pin);

#ifdef __cplusplus
}
#endif

#endif /* VAULTPACK_RANGE_IO_H */