/*
 * src/index_lookup.c
 *
 * Lookup helpers for the v2 section index. Uses binary search after
 * a sort pass to keep the lookup cost log(n) in the index size.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/index2.h"
#include "vaultpack/error.h"

int vp_index2_lookup(const vp_index2_t *idx, const char *name,
                     vp_index2_slot_t *out)
{
    if (!idx || !name || !out) return VP_ERR_INVALID;
    /* Binary search. */
    int32_t lo = 0;
    int32_t hi = (int32_t)idx->count - 1;
    while (lo <= hi) {
        int32_t mid = (lo + hi) / 2;
        int cmp = strcmp(idx->slots[mid].name, name);
        if (cmp == 0) {
            *out = idx->slots[mid];
            return VP_OK;
        }
        if (cmp < 0) lo = mid + 1;
        else         hi = mid - 1;
    }
    return VP_ERR_NOTFOUND;
}