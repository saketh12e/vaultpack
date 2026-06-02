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

/* === Sort by secondary keys ===
 *
 * The v2 index supports sorting by offset, size, and type in
 * addition to the default name sort. These alternate orders are
 * useful for diagnostics and for tooling that wants to walk the
 * index in a non-alphabetical order.
 */

static int sort_by_offset(const void *a, const void *b)
{
    const vp_index2_slot_t *sa = (const vp_index2_slot_t *)a;
    const vp_index2_slot_t *sb = (const vp_index2_slot_t *)b;
    if (sa->offset < sb->offset) return -1;
    if (sa->offset > sb->offset) return 1;
    return 0;
}

static int sort_by_size(const void *a, const void *b)
{
    const vp_index2_slot_t *sa = (const vp_index2_slot_t *)a;
    const vp_index2_slot_t *sb = (const vp_index2_slot_t *)b;
    if (sa->size < sb->size) return -1;
    if (sa->size > sb->size) return 1;
    return 0;
}

static int sort_by_type(const void *a, const void *b)
{
    const vp_index2_slot_t *sa = (const vp_index2_slot_t *)a;
    const vp_index2_slot_t *sb = (const vp_index2_slot_t *)b;
    if (sa->type < sb->type) return -1;
    if (sa->type > sb->type) return 1;
    return 0;
}

/* Simple insertion sort by offset. */
void vp_index2_sort_by_offset(vp_index2_t *idx)
{
    if (!idx) return;
    for (uint32_t i = 1; i < idx->count; i++) {
        vp_index2_slot_t key = idx->slots[i];
        uint32_t j = i;
        while (j > 0 && sort_by_offset(&idx->slots[j - 1], &key) > 0) {
            idx->slots[j] = idx->slots[j - 1];
            j--;
        }
        idx->slots[j] = key;
    }
}

void vp_index2_sort_by_size(vp_index2_t *idx)
{
    if (!idx) return;
    for (uint32_t i = 1; i < idx->count; i++) {
        vp_index2_slot_t key = idx->slots[i];
        uint32_t j = i;
        while (j > 0 && sort_by_size(&idx->slots[j - 1], &key) > 0) {
            idx->slots[j] = idx->slots[j - 1];
            j--;
        }
        idx->slots[j] = key;
    }
}

void vp_index2_sort_by_type(vp_index2_t *idx)
{
    if (!idx) return;
    for (uint32_t i = 1; i < idx->count; i++) {
        vp_index2_slot_t key = idx->slots[i];
        uint32_t j = i;
        while (j > 0 && sort_by_type(&idx->slots[j - 1], &key) > 0) {
            idx->slots[j] = idx->slots[j - 1];
            j--;
        }
        idx->slots[j] = key;
    }
}

void vp_index2_reverse(vp_index2_t *idx)
{
    if (!idx) return;
    for (uint32_t i = 0, j = idx->count; i < j; i++, j--) {
        vp_index2_slot_t tmp = idx->slots[i];
        idx->slots[i] = idx->slots[j - 1];
        idx->slots[j - 1] = tmp;
    }
}