#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/index.h"
#include "vaultpack/archive.h"
#include "vaultpack/error.h"

static void test_build_and_insert(void)
{
    vp_index_t idx;
    int rc = vp_build_index(&idx, 2, 2);
    assert(rc == VP_OK);
    assert(idx.capacity >= 4);
    assert(idx.used == 1);

    rc = vp_index_insert(&idx, 0x100, 7);
    assert(rc == VP_OK);
    assert(idx.used == 2);

    vp_index_slot_t slot;
    rc = vp_index_lookup(&idx, 0x100, &slot);
    assert(rc == VP_OK);
    assert(slot.offset == 7);

    rc = vp_index_lookup(&idx, 0xDEAD, &slot);
    assert(rc != VP_OK);

    vp_index_destroy(&idx);
}

static void test_grow(void)
{
    vp_index_t idx;
    int rc = vp_build_index(&idx, 2, 2);
    assert(rc == VP_OK);

    /* Force several grow cycles */
    for (uint32_t i = 0; i < 100; i++) {
        rc = vp_index_insert(&idx, 0x1000 + i, i);
        assert(rc == VP_OK);
    }
    assert(idx.used == 101);

    vp_index_destroy(&idx);
}

int main(void)
{
    test_build_and_insert();
    test_grow();
    printf("test_index: all assertions passed\n");
    return 0;
}
