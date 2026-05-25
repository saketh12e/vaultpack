/*
 * include/vaultpack/meta_schema.h
 *
 * Typed metadata schema. A schema describes the typed layout of
 * metadata entries for a section: each entry has a 4-byte tag and
 * a value with a declared type. The validator dispatches on the
 * tag and checks the value against the type before exposing the
 * metadata to callers.
 */
#ifndef VAULTPACK_META_SCHEMA_H
#define VAULTPACK_META_SCHEMA_H

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VP_META_DISPATCH_SIZE 64

typedef enum vp_meta_type {
    VP_META_INT32  = 1,
    VP_META_STRING = 2,
    VP_META_BYTES  = 3,
    VP_META_BOOL   = 4
} vp_meta_type_t;

typedef struct vp_meta_dispatch_entry {
    uint32_t      tag;
    vp_meta_type_t type;
} vp_meta_dispatch_entry_t;

typedef struct vp_meta_schema {
    vp_meta_dispatch_entry_t table[VP_META_DISPATCH_SIZE];
    uint32_t                 count;
} vp_meta_schema_t;

void vp_meta_schema_init(vp_meta_schema_t *s);
int  vp_meta_schema_register(vp_meta_schema_t *s, uint32_t tag,
                             vp_meta_type_t type);

int  vp_meta_schema_validate(const vp_meta_schema_t *s,
                            uint32_t tag, const uint8_t *value, size_t len);

/* Schema introspection. */
vp_meta_type_t vp_meta_schema_lookup(const vp_meta_schema_t *s, uint32_t tag);
int      vp_meta_schema_known(const vp_meta_schema_t *s, uint32_t tag);
uint32_t vp_meta_schema_count(const vp_meta_schema_t *s);
uint32_t vp_meta_schema_tag_at(const vp_meta_schema_t *s, uint32_t idx);
vp_meta_type_t vp_meta_schema_type_at(const vp_meta_schema_t *s, uint32_t idx);
int      vp_meta_schema_unregister(vp_meta_schema_t *s, uint32_t tag);

/* Typed convenience validators. */
int vp_meta_schema_validate_int(const vp_meta_schema_t *s, uint32_t tag, int32_t value);
int vp_meta_schema_validate_string(const vp_meta_schema_t *s, uint32_t tag, const char *value);
int vp_meta_schema_validate_bool(const vp_meta_schema_t *s, uint32_t tag, int value);

/* Schema advanced operations. */
int  vp_meta_schema_coerce_to_int(const vp_meta_schema_t *s, uint32_t tag, int32_t *out);
int  vp_meta_schema_coerce_to_bool(const vp_meta_schema_t *s, uint32_t tag, int *out);
int  vp_meta_schema_type_name(vp_meta_type_t t, char *out, size_t out_cap);
int  vp_meta_schema_validate_batch(const vp_meta_schema_t *s,
                                  const uint32_t *tags,
                                  const uint8_t * const *values,
                                  const size_t *lens,
                                  uint32_t count,
                                  uint32_t *failed_index);
uint32_t vp_meta_schema_capacity(const vp_meta_schema_t *s);
int  vp_meta_schema_reset(vp_meta_schema_t *s);
int  vp_meta_schema_register_default_int32(vp_meta_schema_t *s, uint32_t tag);
int  vp_meta_schema_register_default_string(vp_meta_schema_t *s, uint32_t tag);
int  vp_meta_schema_register_default_bool(vp_meta_schema_t *s, uint32_t tag);
int  vp_meta_schema_register_default_bytes(vp_meta_schema_t *s, uint32_t tag);

/* Test entry point used by the bridge. */
int vp_meta_schema_run(const uint8_t *data, size_t size);
int vp_meta_compose_run(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* VAULTPACK_META_SCHEMA_H */