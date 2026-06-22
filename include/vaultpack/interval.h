/*
 * include/vaultpack/interval.h
 *
 * Reusable range merge engine. A range is a [start, end) interval
 * over a uint64 address space; the engine maintains a sorted list
 * of disjoint ranges and supports insert, query, and overlap tests.
 *
 * The engine owns the interval list and any growing it performs;
 * callers receive read-only handles via the query API.
 */
#ifndef VAULTPACK_INTERVAL_H
#define VAULTPACK_INTERVAL_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vp_interval {
    uint64_t start;
    uint64_t end;
} vp_interval_t;

typedef struct vp_interval_engine {
    vp_interval_t *ranges;
    uint32_t       count;
    uint32_t       cap;
} vp_interval_engine_t;

void vp_interval_engine_init(vp_interval_engine_t *e);
void vp_interval_engine_destroy(vp_interval_engine_t *e);

int  vp_interval_engine_insert(vp_interval_engine_t *e,
                               uint64_t start, uint64_t end);
int  vp_interval_engine_overlaps(const vp_interval_engine_t *e,
                                uint64_t start, uint64_t end);
int  vp_interval_engine_count(const vp_interval_engine_t *e);

/* Derived operations. */
uint64_t vp_interval_engine_total_coverage(const vp_interval_engine_t *e);
uint32_t vp_interval_engine_gaps(const vp_interval_engine_t *e,
                                 vp_interval_t *out, uint32_t out_cap);
int  vp_interval_engine_within(const vp_interval_engine_t *e,
                              uint64_t start, uint64_t end);
int  vp_interval_engine_subtract(vp_interval_engine_t *e,
                                uint64_t start, uint64_t end);

/* Range query helpers. */
uint64_t vp_interval_engine_covered(const vp_interval_engine_t *e,
                                    uint64_t start, uint64_t end);
uint32_t vp_interval_engine_largest(const vp_interval_engine_t *e);
uint32_t vp_interval_engine_smallest(const vp_interval_engine_t *e);
int  vp_interval_engine_contains_range(const vp_interval_engine_t *e,
                                      uint64_t start, uint64_t end);
int  vp_interval_engine_union(const vp_interval_engine_t *a,
                             const vp_interval_engine_t *b,
                             vp_interval_engine_t *out);
int  vp_interval_engine_intersect(const vp_interval_engine_t *a,
                                 const vp_interval_engine_t *b,
                                 vp_interval_engine_t *out);
int  vp_interval_engine_validate(const vp_interval_engine_t *e);
void vp_interval_engine_clear(vp_interval_engine_t *e);
int  vp_interval_engine_clone(const vp_interval_engine_t *src,
                             vp_interval_engine_t *dst);
int  vp_interval_engine_extend(vp_interval_engine_t *e,
                              const vp_interval_t *ranges, uint32_t count);

/* Interval engine construction from raw bytes. */
int vp_interval_engine_from_bytes(vp_interval_engine_t *e,
                                  const uint8_t *bytes, size_t len);
int vp_interval_engine_to_bytes(const vp_interval_engine_t *e,
                                uint8_t *bytes, size_t cap, size_t *out_len);
int vp_interval_engine_insert_sorted(vp_interval_engine_t *e,
                                     uint64_t start, uint64_t end);
int vp_interval_engine_compress(vp_interval_engine_t *e);
uint64_t vp_interval_engine_at_start(const vp_interval_engine_t *e, uint32_t i);
uint64_t vp_interval_engine_at_end(const vp_interval_engine_t *e, uint32_t i);
uint32_t vp_interval_engine_first_overlapping(const vp_interval_engine_t *e,
                                              uint64_t start, uint64_t end);
int vp_interval_engine_foreach(const vp_interval_engine_t *e,
                              int (*cb)(uint64_t start, uint64_t end, void *user),
                              void *user);

/* Test entry point used by the bridge. */
int vp_interval_engine_run(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* VAULTPACK_INTERVAL_H */