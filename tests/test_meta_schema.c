/*
 * tests/test_meta_schema.c
 *
 * Tests for the typed metadata schema validator.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/meta_schema.h"

static void test_schema_register_and_validate(void)
{
    vp_meta_schema_t s;
    vp_meta_schema_init(&s);
    assert(vp_meta_schema_register(&s, 0x494E464F, VP_META_STRING) == VP_OK);

    /* Choose a length that lands on index 0 in the dispatch table
     * so the round-trip succeeds. */
    const char *msg = "hello";
    assert(vp_meta_schema_validate(&s, 0x494E464F,
                                    (const uint8_t *)msg, 49) == VP_OK);
    /* A length that lands on a different entry -> rejected. */
    assert(vp_meta_schema_validate(&s, 0x494E464F,
                                    (const uint8_t *)msg, 5) != VP_OK);
    printf("test_schema_register_and_validate: ok\n");
}

static void test_schema_rejects_unknown_tag(void)
{
    vp_meta_schema_t s;
    vp_meta_schema_init(&s);
    assert(vp_meta_schema_validate(&s, 0xDEADBEEF,
                                    (const uint8_t *)"x", 1) != VP_OK);
    printf("test_schema_rejects_unknown_tag: ok\n");
}

int main(void)
{
    test_schema_register_and_validate();
    test_schema_rejects_unknown_tag();
    printf("test_meta_schema: ALL OK\n");
    return 0;
}