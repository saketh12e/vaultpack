/*
 * src/index_sort.c
 *
 * Sort helpers for the v2 section index. Insertion sort is used
 * because the v2 index is bounded to VP_INDEX2_SLOT_MAX entries and
 * the typical archive has at most a few hundred sections.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/index2.h"
#include "vaultpack/error.h"

void vp_index2_sort(vp_index2_t *idx)
{
    if (!idx) return;
    for (uint32_t i = 1; i < idx->count; i++) {
        vp_index2_slot_t key = idx->slots[i];
        uint32_t j = i;
        while (j > 0 && strcmp(idx->slots[j - 1].name, key.name) > 0) {
            idx->slots[j] = idx->slots[j - 1];
            j--;
        }
        idx->slots[j] = key;
    }
}