/*
 * include/vaultpack/section_cache.h
 *
 * Section payload cache backed by an LRU slot pool. Each slot owns
 * a heap-allocated payload buffer keyed by section name. When a
 * new payload arrives that exceeds the pool capacity, the least-
 * recently-used slot is evicted; if the slot is still pinned (e.g.
 * an iterator is reading its payload), the eviction falls back to
 * the next slot in LRU order.
 *
 * Pins are reference counts held by callers. A pinned slot is not
 * evicted; if all slots are pinned and a new payload arrives, the
 * cache returns VP_ERR_FULL.
 */
#ifndef VAULTPACK_SECTION_CACHE_H
#define VAULTPACK_SECTION_CACHE_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VP_SECTION_CACHE_SLOTS 8
#define VP_SECTION_CACHE_PAYLOAD_MAX (1u << 20)

typedef struct vp_section_cache_slot {
    char     name[64];
    uint8_t *payload;
    size_t   len;
    uint32_t pin_count;
    uint64_t last_used;
} vp_section_cache_slot_t;

typedef struct vp_section_cache {
    vp_section_cache_slot_t slots[VP_SECTION_CACHE_SLOTS];
    uint64_t                clock;
} vp_section_cache_t;

void vp_section_cache_init(vp_section_cache_t *c);
void vp_section_cache_destroy(vp_section_cache_t *c);

int  vp_section_cache_put(vp_section_cache_t *c,
                          const char *name, const uint8_t *data, size_t len);
int  vp_section_cache_get(vp_section_cache_t *c,
                          const char *name,
                          const uint8_t **out_data, size_t *out_len);

int  vp_section_cache_pin(vp_section_cache_t *c, const char *name);
int  vp_section_cache_unpin(vp_section_cache_t *c, const char *name);

/* Test entry point used by the bridge. */
int vp_section_cache_run(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* VAULTPACK_SECTION_CACHE_H */