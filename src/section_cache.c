/*
 * src/section_cache.c
 *
 * LRU section payload cache. Backed by a fixed pool of slots; on
 * overflow the least-recently-used slot is evicted to make room.
 *
 * Pins are ref counts. A pinned slot cannot be evicted while any
 * caller holds a pin; if all slots are pinned at insert time the
 * cache returns VP_ERR_FULL.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/section_cache.h"
#include "vaultpack/error.h"

void vp_section_cache_init(vp_section_cache_t *c)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
}

void vp_section_cache_destroy(vp_section_cache_t *c)
{
    if (!c) return;
    for (uint32_t i = 0; i < VP_SECTION_CACHE_SLOTS; i++) {
        free(c->slots[i].payload);
    }
    memset(c, 0, sizeof(*c));
}

static int vp_section_cache_slot_find(vp_section_cache_t *c, const char *name)
{
    for (uint32_t i = 0; i < VP_SECTION_CACHE_SLOTS; i++) {
        if (c->slots[i].payload && strcmp(c->slots[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static uint32_t vp_section_cache_lru_unpinned(vp_section_cache_t *c)
{
    uint32_t best = 0;
    uint64_t best_clock = UINT64_MAX;
    for (uint32_t i = 0; i < VP_SECTION_CACHE_SLOTS; i++) {
        if (c->slots[i].pin_count > 0) continue;
        if (!c->slots[i].payload) return i;
        if (c->slots[i].last_used < best_clock) {
            best = i;
            best_clock = c->slots[i].last_used;
        }
    }
    return best;
}

int vp_section_cache_put(vp_section_cache_t *c,
                         const char *name, const uint8_t *data, size_t len)
{
    if (!c || !name || len > VP_SECTION_CACHE_PAYLOAD_MAX) return VP_ERR_INVALID;
    int existing = vp_section_cache_slot_find(c, name);
    if (existing >= 0) {
        uint32_t idx = (uint32_t)existing;
        free(c->slots[idx].payload);
        c->slots[idx].payload = (uint8_t *)malloc(len);
        if (!c->slots[idx].payload) return VP_ERR_NOMEM;
        memcpy(c->slots[idx].payload, data, len);
        c->slots[idx].len = len;
        c->slots[idx].last_used = ++c->clock;
        return VP_OK;
    }

    uint32_t idx = vp_section_cache_lru_unpinned(c);
    /* DEFECT: the slot picked may already contain a payload that
     * is being held by an iterator which captured the slot's
     * payload pointer before this insert. The eviction frees the
     * old payload while the iterator still references it. */
    free(c->slots[idx].payload);
    c->slots[idx].payload = (uint8_t *)malloc(len);
    if (!c->slots[idx].payload) return VP_ERR_NOMEM;
    memcpy(c->slots[idx].payload, data, len);
    strncpy(c->slots[idx].name, name, sizeof(c->slots[idx].name) - 1);
    c->slots[idx].len = len;
    c->slots[idx].pin_count = 0;
    c->slots[idx].last_used = ++c->clock;
    return VP_OK;
}

int vp_section_cache_get(vp_section_cache_t *c,
                         const char *name,
                         const uint8_t **out_data, size_t *out_len)
{
    if (!c || !name || !out_data || !out_len) return VP_ERR_INVALID;
    int idx = vp_section_cache_slot_find(c, name);
    if (idx < 0) return VP_ERR_NOTFOUND;
    c->slots[idx].last_used = ++c->clock;
    *out_data = c->slots[idx].payload;
    *out_len  = c->slots[idx].len;
    return VP_OK;
}

int vp_section_cache_pin(vp_section_cache_t *c, const char *name)
{
    if (!c || !name) return VP_ERR_INVALID;
    int idx = vp_section_cache_slot_find(c, name);
    if (idx < 0) return VP_ERR_NOTFOUND;
    c->slots[idx].pin_count++;
    return VP_OK;
}

int vp_section_cache_unpin(vp_section_cache_t *c, const char *name)
{
    if (!c || !name) return VP_ERR_INVALID;
    int idx = vp_section_cache_slot_find(c, name);
    if (idx < 0) return VP_ERR_NOTFOUND;
    if (c->slots[idx].pin_count == 0) return VP_ERR_INVALID;
    c->slots[idx].pin_count--;
    return VP_OK;
}

int vp_section_cache_run(const uint8_t *data, size_t size)
{
    if (!data || size < 4) return 0;
    vp_section_cache_t c;
    vp_section_cache_init(&c);

    uint32_t op_count = (uint32_t)data[0]
                      | ((uint32_t)data[1] << 8);
    if (op_count > 128) op_count = 128;
    size_t pos = 2;

    for (uint32_t i = 0; i < op_count && pos < size; i++) {
        uint8_t op = data[pos++];
        if (op == 0) {
            /* put: name + len + payload */
            if (pos >= size) break;
            uint8_t namelen = data[pos++];
            if (pos + namelen + 4 > size) break;
            char name[64];
            size_t copy = namelen < 63 ? namelen : 63;
            for (size_t k = 0; k < copy; k++) name[k] = (char)data[pos + k];
            name[copy] = '\0';
            pos += namelen;
            uint32_t len = (uint32_t)data[pos]
                         | ((uint32_t)data[pos + 1] << 8)
                         | ((uint32_t)data[pos + 2] << 16)
                         | ((uint32_t)data[pos + 3] << 24);
            pos += 4;
            if (len > size - pos) len = (uint32_t)(size - pos);
            vp_section_cache_put(&c, name, data + pos, len);
            pos += len;
        } else if (op == 1) {
            /* get: name -> pin to keep alive, then unpin */
            if (pos >= size) break;
            uint8_t namelen = data[pos++];
            if (pos + namelen > size) break;
            char name[64];
            size_t copy = namelen < 63 ? namelen : 63;
            for (size_t k = 0; k < copy; k++) name[k] = (char)data[pos + k];
            name[copy] = '\0';
            pos += namelen;
            const uint8_t *p = NULL;
            size_t pl = 0;
            vp_section_cache_get(&c, name, &p, &pl);
            /* pin/unpin to exercise the eviction under-pin path */
            vp_section_cache_pin(&c, name);
            vp_section_cache_unpin(&c, name);
        } else {
            break;
        }
    }

    vp_section_cache_destroy(&c);
    return 0;
}

/* === Cache statistics ===
 *
 * The cache maintains a few derived counters that callers (and the
 * harness) can read for instrumentation. Statistics are computed
 * lazily from the underlying slots; they are not maintained
 * incrementally.
 */

uint32_t vp_section_cache_slot_count(const vp_section_cache_t *c)
{
    if (!c) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < VP_SECTION_CACHE_SLOTS; i++) {
        if (c->slots[i].payload) n++;
    }
    return n;
}

uint64_t vp_section_cache_total_bytes(const vp_section_cache_t *c)
{
    if (!c) return 0;
    uint64_t total = 0;
    for (uint32_t i = 0; i < VP_SECTION_CACHE_SLOTS; i++) {
        total += c->slots[i].len;
    }
    return total;
}

uint32_t vp_section_cache_pinned_count(const vp_section_cache_t *c)
{
    if (!c) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < VP_SECTION_CACHE_SLOTS; i++) {
        if (c->slots[i].payload && c->slots[i].pin_count > 0) n++;
    }
    return n;
}

int vp_section_cache_remove(vp_section_cache_t *c, const char *name)
{
    if (!c || !name) return VP_ERR_INVALID;
    int idx = vp_section_cache_slot_find(c, name);
    if (idx < 0) return VP_ERR_NOTFOUND;
    if (c->slots[idx].pin_count > 0) return VP_ERR_FULL;
    free(c->slots[idx].payload);
    c->slots[idx].payload = NULL;
    c->slots[idx].len = 0;
    c->slots[idx].name[0] = '\0';
    c->slots[idx].last_used = 0;
    return VP_OK;
}

int vp_section_cache_clear(vp_section_cache_t *c)
{
    if (!c) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < VP_SECTION_CACHE_SLOTS; i++) {
        if (c->slots[i].pin_count > 0) continue;
        free(c->slots[i].payload);
        c->slots[i].payload = NULL;
        c->slots[i].len = 0;
        c->slots[i].name[0] = '\0';
    }
    return VP_OK;
}

/* === Cache iteration and inspection ===
 *
 * Callers can iterate over slots without disturbing their pin state,
 * look up a slot index by name, and read the cached payload's
 * metadata. The iteration API is read-only.
 */

uint32_t vp_section_cache_find_slot(const vp_section_cache_t *c, const char *name)
{
    if (!c || !name) return (uint32_t)-1;
    for (uint32_t i = 0; i < VP_SECTION_CACHE_SLOTS; i++) {
        if (c->slots[i].payload && strcmp(c->slots[i].name, name) == 0) {
            return i;
        }
    }
    return (uint32_t)-1;
}

const char *vp_section_cache_slot_name(const vp_section_cache_t *c, uint32_t idx)
{
    if (!c || idx >= VP_SECTION_CACHE_SLOTS) return NULL;
    return c->slots[idx].payload ? c->slots[idx].name : NULL;
}

size_t vp_section_cache_slot_len(const vp_section_cache_t *c, uint32_t idx)
{
    if (!c || idx >= VP_SECTION_CACHE_SLOTS) return 0;
    return c->slots[idx].len;
}

uint32_t vp_section_cache_slot_pins(const vp_section_cache_t *c, uint32_t idx)
{
    if (!c || idx >= VP_SECTION_CACHE_SLOTS) return 0;
    return c->slots[idx].pin_count;
}

int vp_section_cache_contains(const vp_section_cache_t *c, const char *name)
{
    return vp_section_cache_find_slot(c, name) != (uint32_t)-1;
}

int vp_section_cache_resize_slot(vp_section_cache_t *c,
                                 const char *name, size_t new_len)
{
    if (!c || !name) return VP_ERR_INVALID;
    uint32_t idx = vp_section_cache_find_slot(c, name);
    if (idx == (uint32_t)-1) return VP_ERR_NOTFOUND;
    if (c->slots[idx].pin_count > 0) return VP_ERR_FULL;
    if (new_len <= c->slots[idx].cap) {
        c->slots[idx].len = new_len;
        return VP_OK;
    }
    uint8_t *p = (uint8_t *)realloc(c->slots[idx].payload, new_len);
    if (!p) return VP_ERR_NOMEM;
    c->slots[idx].payload = p;
    c->slots[idx].cap = new_len;
    c->slots[idx].len = new_len;
    return VP_OK;
}

int vp_section_cache_iter(const vp_section_cache_t *c,
                          vp_section_cache_iter_cb cb, void *user)
{
    if (!c) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < VP_SECTION_CACHE_SLOTS; i++) {
        if (c->slots[i].payload && cb
            && !cb(c->slots[i].name, c->slots[i].payload,
                   c->slots[i].len, c->slots[i].pin_count, user)) {
            return VP_ERR_TRUNCATED;
        }
    }
    return VP_OK;
}

uint64_t vp_section_cache_hit_clock(const vp_section_cache_t *c)
{
    return c ? c->clock : 0;
}

/* === Cache management operations ===
 *
 * Bulk operations and quota management. The cache supports a
 * configurable per-slot payload cap; inserts that exceed the cap
 * are split across multiple slots.
 */

int vp_section_cache_set_cap(vp_section_cache_t *c, size_t new_cap)
{
    if (!c) return VP_ERR_INVALID;
    if (new_cap == 0) return VP_ERR_INVALID;
    if (new_cap > VP_SECTION_CACHE_PAYLOAD_MAX) return VP_ERR_OVERFLOW;
    for (uint32_t i = 0; i < VP_SECTION_CACHE_SLOTS; i++) {
        if (c->slots[i].cap > new_cap) c->slots[i].cap = new_cap;
    }
    return VP_OK;
}

int vp_section_cache_walk(vp_section_cache_t *c,
                          vp_section_cache_walk_cb cb, void *user)
{
    if (!c || !cb) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < VP_SECTION_CACHE_SLOTS; i++) {
        if (c->slots[i].payload) {
            if (!cb(c->slots[i].name, c->slots[i].payload,
                     c->slots[i].len, c->slots[i].pin_count, user)) {
                return VP_ERR_TRUNCATED;
            }
        }
    }
    return VP_OK;
}

int vp_section_cache_promote(vp_section_cache_t *c, const char *name)
{
    if (!c || !name) return VP_ERR_INVALID;
    uint32_t idx = vp_section_cache_find_slot(c, name);
    if (idx == (uint32_t)-1) return VP_ERR_NOTFOUND;
    c->slots[idx].last_used = ++c->clock;
    return VP_OK;
}

int vp_section_cache_evict_lru(vp_section_cache_t *c)
{
    if (!c) return VP_ERR_INVALID;
    uint32_t idx = vp_section_cache_lru_unpinned(c);
    if (c->slots[idx].payload == NULL) return VP_ERR_NOTFOUND;
    if (c->slots[idx].pin_count > 0) return VP_ERR_FULL;
    free(c->slots[idx].payload);
    c->slots[idx].payload = NULL;
    c->slots[idx].len = 0;
    c->slots[idx].name[0] = '\0';
    c->slots[idx].last_used = 0;
    return VP_OK;
}

int vp_section_cache_evict_named(vp_section_cache_t *c, const char *name)
{
    if (!c || !name) return VP_ERR_INVALID;
    return vp_section_cache_remove(c, name);
}

int vp_section_cache_swap(vp_section_cache_t *c,
                           const char *a, const char *b)
{
    if (!c || !a || !b) return VP_ERR_INVALID;
    uint32_t ai = vp_section_cache_find_slot(c, a);
    uint32_t bi = vp_section_cache_find_slot(c, b);
    if (ai == (uint32_t)-1 || bi == (uint32_t)-1) return VP_ERR_NOTFOUND;
    vp_section_cache_slot_t tmp = c->slots[ai];
    c->slots[ai] = c->slots[bi];
    c->slots[bi] = tmp;
    return VP_OK;
}

int vp_section_cache_rename(vp_section_cache_t *c,
                            const char *old_name, const char *new_name)
{
    if (!c || !old_name || !new_name) return VP_ERR_INVALID;
    if (vp_section_cache_find_slot(c, new_name) != (uint32_t)-1) return VP_ERR_DUPLICATE;
    uint32_t idx = vp_section_cache_find_slot(c, old_name);
    if (idx == (uint32_t)-1) return VP_ERR_NOTFOUND;
    strncpy(c->slots[idx].name, new_name, sizeof(c->slots[idx].name) - 1);
    c->slots[idx].name[sizeof(c->slots[idx].name) - 1] = '\0';
    return VP_OK;
}

int vp_section_cache_resize_all(vp_section_cache_t *c, size_t new_cap)
{
    if (!c) return VP_ERR_INVALID;
    if (new_cap == 0) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < VP_SECTION_CACHE_SLOTS; i++) {
        if (c->slots[i].cap > new_cap) c->slots[i].cap = new_cap;
    }
    return VP_OK;
}

int vp_section_cache_validate(const vp_section_cache_t *c)
{
    if (!c) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < VP_SECTION_CACHE_SLOTS; i++) {
        if (!c->slots[i].payload && c->slots[i].len != 0) return VP_ERR_INVALID;
        if (c->slots[i].payload && c->slots[i].cap < c->slots[i].len) return VP_ERR_INVALID;
    }
    return VP_OK;
}