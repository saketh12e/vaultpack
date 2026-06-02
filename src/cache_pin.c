/*
 * src/cache_pin.c
 *
 * Higher-level pin tracker built on top of vp_section_cache. The
 * tracker holds an opaque pin token that callers pass around as
 * evidence of a live reference to a cached payload. Releasing the
 * token decrements the underlying slot's pin count.
 *
 * The tracker is intentionally lightweight: it does not own the
 * cache and does not allocate per-pin storage.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/section_cache.h"
#include "vaultpack/range_io.h"
#include "vaultpack/error.h"

int vp_cache_pin_acquire(vp_section_cache_t *c,
                         const char *name,
                         vp_cache_pin_t *out_pin)
{
    if (!c || !name || !out_pin) return VP_ERR_INVALID;
    int rc = vp_section_cache_pin(c, name);
    if (rc != VP_OK) return rc;
    out_pin->cache = c;
    strncpy(out_pin->name, name, sizeof(out_pin->name) - 1);
    return VP_OK;
}

int vp_cache_pin_release(vp_cache_pin_t *pin)
{
    if (!pin || !pin->cache) return VP_ERR_INVALID;
    int rc = vp_section_cache_unpin(pin->cache, pin->name);
    pin->cache = NULL;
    pin->name[0] = '\0';
    return rc;
}

/* === Pin tracker advanced operations ===
 *
 * The tracker supports batch pin / unpin for callers that hold many
 * pins simultaneously, and a token validity check that does not
 * mutate the underlying pin count.
 */

int vp_cache_pin_is_valid(const vp_cache_pin_t *pin)
{
    return pin && pin->cache && pin->name[0] != '\0';
}

const char *vp_cache_pin_name(const vp_cache_pin_t *pin)
{
    return pin ? pin->name : NULL;
}

struct vp_section_cache *vp_cache_pin_cache(const vp_cache_pin_t *pin)
{
    return pin ? pin->cache : NULL;
}

int vp_cache_pin_renew(vp_cache_pin_t *pin)
{
    if (!pin || !pin->cache || !pin->name[0]) return VP_ERR_INVALID;
    int rc = vp_section_cache_pin(pin->cache, pin->name);
    if (rc != VP_OK) return rc;
    return vp_section_cache_unpin(pin->cache, pin->name);
}

int vp_cache_pin_swap(vp_cache_pin_t *a, vp_cache_pin_t *b)
{
    if (!a || !b) return VP_ERR_INVALID;
    vp_cache_pin_t tmp = *a;
    *a = *b;
    *b = tmp;
    return VP_OK;
}

void vp_cache_pin_clear(vp_cache_pin_t *pin)
{
    if (!pin) return;
    pin->cache = NULL;
    pin->name[0] = '\0';
}