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

/* Test entry point used by the bridge. */
int vp_interval_engine_run(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* VAULTPACK_INTERVAL_H */