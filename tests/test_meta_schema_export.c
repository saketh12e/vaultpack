/*
 * tests/test_meta_schema_export.c
 *
 * Tests for schema serialization (export/import round-trip).
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/meta_schema.h"

static void test_export_import_round_trip(void)
{
    vp_meta_schema_t s;
    vp_meta_schema_init(&s);
    vp_meta_schema_register(&s, 0x494E464F, VP_META_STRING);
    vp_meta_schema_register(&s, 0x4E554D32, VP_META_INT32);
    vp_meta_schema_register(&s, 0x424F4F4C, VP_META_BOOL);
    assert(vp_meta_schema_count(&s) == 3);

    uint8_t buf[256];
    size_t  buf_len = 0;
    assert(vp_meta_schema_export(&s, buf, sizeof(buf), &buf_len) == VP_OK);
    assert(buf_len == 8 + 3 * 5);

    vp_meta_schema_t s2;
    assert(vp_meta_schema_import(&s2, buf, buf_len) == VP_OK);
    assert(vp_meta_schema_count(&s2) == 3);
    assert(vp_meta_schema_known(&s2, 0x494E464F));
    assert(vp_meta_schema_known(&s2, 0x4E554D32));
    assert(vp_meta_schema_known(&s2, 0x424F4F4C));
    printf("test_export_import_round_trip: ok\n");
}

static void test_clone(void)
{
    vp_meta_schema_t a, b;
    vp_meta_schema_init(&a);
    vp_meta_schema_register(&a, 0x11111111, VP_META_INT32);
    vp_meta_schema_register(&a, 0x22222222, VP_META_BYTES);
    assert(vp_meta_schema_clone(&a, &b) == VP_OK);
    assert(vp_meta_schema_count(&b) == 2);
    assert(vp_meta_schema_known(&b, 0x11111111));
    assert(vp_meta_schema_known(&b, 0x22222222));
    printf("test_clone: ok\n");
}

static void test_max_tag(void)
{
    vp_meta_schema_t s;
    vp_meta_schema_init(&s);
    vp_meta_schema_register(&s, 0x100, VP_META_INT32);
    vp_meta_schema_register(&s, 0x10000, VP_META_INT32);
    vp_meta_schema_register(&s, 0x1000, VP_META_INT32);
    assert(vp_meta_schema_max_tag(&s) == 0x10000);
    printf("test_max_tag: ok\n");
}

static void test_count_by_type(void)
{
    vp_meta_schema_t s;
    vp_meta_schema_init(&s);
    vp_meta_schema_register(&s, 1, VP_META_INT32);
    vp_meta_schema_register(&s, 2, VP_META_STRING);
    vp_meta_schema_register(&s, 3, VP_META_INT32);
    assert(vp_meta_schema_count_by_type(&s, VP_META_INT32) == 2);
    assert(vp_meta_schema_count_by_type(&s, VP_META_STRING) == 1);
    assert(vp_meta_schema_count_by_type(&s, VP_META_BOOL) == 0);
    printf("test_count_by_type: ok\n");
}

int main(void)
{
    test_export_import_round_trip();
    test_clone();
    test_max_tag();
    test_count_by_type();
    printf("test_meta_schema_export: ALL OK\n");
    return 0;
}