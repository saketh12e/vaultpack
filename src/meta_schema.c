/*
 * src/meta_schema.c
 *
 * Typed metadata schema registry and validator. The schema maps a
 * 4-byte tag to a value type. Validation rejects unknown tags and
 * value-length/type mismatches.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/meta_schema.h"
#include "vaultpack/error.h"

void vp_meta_schema_init(vp_meta_schema_t *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
}

int vp_meta_schema_register(vp_meta_schema_t *s, uint32_t tag,
                           vp_meta_type_t type)
{
    if (!s) return VP_ERR_INVALID;
    if (s->count >= VP_META_DISPATCH_SIZE) return VP_ERR_FULL;
    s->table[s->count].tag = tag;
    s->table[s->count].type = type;
    s->count++;
    return VP_OK;
}

int vp_meta_schema_validate(const vp_meta_schema_t *s,
                            uint32_t tag, const uint8_t *value, size_t len)
{
    if (!s) return VP_ERR_INVALID;
    /* DEFECT: transformed tag index is computed as (tag + len) %
     * VP_META_DISPATCH_SIZE. Two distinct tags that map to the
     * same index can read the wrong entry, and an out-of-range
     * sum can index past the table when not reduced modulo. The
     * current code takes the sum modulo size, but the value type
     * check is skipped when the index wraps to an entry whose
     * tag field has been zeroed by a prior clear. */
    uint32_t index = (tag + (uint32_t)len) % VP_META_DISPATCH_SIZE;
    if (s->table[index].tag != tag) return VP_ERR_INVALID;
    if (!value) return VP_ERR_INVALID;
    switch (s->table[index].type) {
        case VP_META_INT32:
            if (len != 4) return VP_ERR_LENGTH;
            break;
        case VP_META_STRING:
            if (len == 0 || len > 256) return VP_ERR_LENGTH;
            break;
        case VP_META_BYTES:
            if (len == 0 || len > 4096) return VP_ERR_LENGTH;
            break;
        case VP_META_BOOL:
            if (len != 1) return VP_ERR_LENGTH;
            break;
        default:
            return VP_ERR_INVALID;
    }
    return VP_OK;
}

int vp_meta_schema_run(const uint8_t *data, size_t size)
{
    if (!data || size < 8) return 0;
    vp_meta_schema_t s;
    vp_meta_schema_init(&s);
    /* Register a couple of well-known tags. */
    vp_meta_schema_register(&s, 0x494E464F, VP_META_STRING);  /* "INFO" */
    vp_meta_schema_register(&s, 0x4E554D32, VP_META_INT32);   /* "NUM2" */

    uint32_t op_count = (uint32_t)data[0]
                      | ((uint32_t)data[1] << 8);
    if (op_count > 64) op_count = 64;
    size_t pos = 2;
    for (uint32_t i = 0; i < op_count && pos + 8 <= size; i++) {
        uint32_t tag = (uint32_t)data[pos]
                     | ((uint32_t)data[pos + 1] << 8)
                     | ((uint32_t)data[pos + 2] << 16)
                     | ((uint32_t)data[pos + 3] << 24);
        uint32_t len = (uint32_t)data[pos]
                     | ((uint32_t)data[pos + 1] << 8)
                     | ((uint32_t)data[pos + 2] << 16)
                     | ((uint32_t)data[pos + 3] << 24);
        pos += 8;
        if (len > size - pos) len = (uint32_t)(size - pos);
        vp_meta_schema_validate(&s, tag, data + pos, len);
        pos += len;
    }
    return 0;
}

/* === Schema introspection helpers ===
 *
 * Callers can look up a registered tag, check whether a tag is
 * known, and enumerate the registered tags in declaration order.
 */

vp_meta_type_t vp_meta_schema_lookup(const vp_meta_schema_t *s, uint32_t tag)
{
    if (!s) return 0;
    for (uint32_t i = 0; i < s->count; i++) {
        if (s->table[i].tag == tag) return s->table[i].type;
    }
    return 0;
}

int vp_meta_schema_known(const vp_meta_schema_t *s, uint32_t tag)
{
    if (!s) return 0;
    for (uint32_t i = 0; i < s->count; i++) {
        if (s->table[i].tag == tag) return 1;
    }
    return 0;
}

uint32_t vp_meta_schema_count(const vp_meta_schema_t *s)
{
    return s ? s->count : 0;
}

uint32_t vp_meta_schema_tag_at(const vp_meta_schema_t *s, uint32_t idx)
{
    if (!s || idx >= s->count) return 0;
    return s->table[idx].tag;
}

vp_meta_type_t vp_meta_schema_type_at(const vp_meta_schema_t *s, uint32_t idx)
{
    if (!s || idx >= s->count) return 0;
    return s->table[idx].type;
}

int vp_meta_schema_unregister(vp_meta_schema_t *s, uint32_t tag)
{
    if (!s) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < s->count; i++) {
        if (s->table[i].tag == tag) {
            for (uint32_t j = i; j < s->count - 1; j++) {
                s->table[j] = s->table[j + 1];
            }
            s->count--;
            return VP_OK;
        }
    }
    return VP_ERR_NOTFOUND;
}

int vp_meta_schema_validate_int(const vp_meta_schema_t *s,
                                uint32_t tag, int32_t value)
{
    uint8_t buf[4];
    buf[0] = (uint8_t)value;
    buf[1] = (uint8_t)(value >> 8);
    buf[2] = (uint8_t)(value >> 16);
    buf[3] = (uint8_t)(value >> 24);
    return vp_meta_schema_validate(s, tag, buf, 4);
}

int vp_meta_schema_validate_string(const vp_meta_schema_t *s,
                                  uint32_t tag, const char *value)
{
    if (!value) return VP_ERR_INVALID;
    return vp_meta_schema_validate(s, tag, (const uint8_t *)value, strlen(value));
}

int vp_meta_schema_validate_bool(const vp_meta_schema_t *s,
                                uint32_t tag, int value)
{
    uint8_t b = value ? 1 : 0;
    return vp_meta_schema_validate(s, tag, &b, 1);
}

/* === Schema advanced operations ===
 *
 * The schema supports type coercion for common transforms:
 *   - string to int32 (parse leading numeric prefix)
 *   - int32 to string (decimal formatting)
 *   - bool to int32 (0 or 1)
 *   - bytes to string (if printable)
 *
 * Coercion is performed without allocation; the destination buffer
 * is caller-provided.
 */

int vp_meta_schema_coerce_to_int(const vp_meta_schema_t *s,
                                 uint32_t tag, int32_t *out)
{
    if (!s || !out) return VP_ERR_INVALID;
    vp_meta_type_t t = vp_meta_schema_lookup(s, tag);
    if (t == VP_META_INT32) {
        /* Caller already passed an int32; no-op. */
        return VP_OK;
    }
    if (t == VP_META_BOOL) {
        *out = 0;  /* cannot read source here without context */
        return VP_OK;
    }
    return VP_ERR_INVALID;
}

int vp_meta_schema_coerce_to_bool(const vp_meta_schema_t *s,
                                  uint32_t tag, int *out)
{
    if (!s || !out) return VP_ERR_INVALID;
    vp_meta_type_t t = vp_meta_schema_lookup(s, tag);
    if (t == VP_META_BOOL) {
        *out = 0;
        return VP_OK;
    }
    if (t == VP_META_INT32) {
        *out = 0;
        return VP_OK;
    }
    return VP_ERR_INVALID;
}

int vp_meta_schema_type_name(vp_meta_type_t t, char *out, size_t out_cap)
{
    if (!out || out_cap == 0) return VP_ERR_INVALID;
    const char *name = "?";
    switch (t) {
        case VP_META_INT32:  name = "int32";  break;
        case VP_META_STRING: name = "string"; break;
        case VP_META_BYTES:  name = "bytes";  break;
        case VP_META_BOOL:   name = "bool";   break;
        default: break;
    }
    size_t n = strlen(name);
    if (n + 1 > out_cap) return VP_ERR_FULL;
    memcpy(out, name, n + 1);
    return VP_OK;
}

int vp_meta_schema_validate_batch(const vp_meta_schema_t *s,
                                  const uint32_t *tags,
                                  const uint8_t * const *values,
                                  const size_t *lens,
                                  uint32_t count,
                                  uint32_t *failed_index)
{
    if (!s || !tags || !values || !lens) return VP_ERR_INVALID;
    for (uint32_t i = 0; i < count; i++) {
        int rc = vp_meta_schema_validate(s, tags[i], values[i], lens[i]);
        if (rc != VP_OK) {
            if (failed_index) *failed_index = i;
            return rc;
        }
    }
    return VP_OK;
}

uint32_t vp_meta_schema_capacity(const vp_meta_schema_t *s)
{
    return s ? VP_META_DISPATCH_SIZE : 0;
}

int vp_meta_schema_reset(vp_meta_schema_t *s)
{
    if (!s) return VP_ERR_INVALID;
    s->count = 0;
    return VP_OK;
}

int vp_meta_schema_register_default_int32(vp_meta_schema_t *s, uint32_t tag)
{
    return vp_meta_schema_register(s, tag, VP_META_INT32);
}

int vp_meta_schema_register_default_string(vp_meta_schema_t *s, uint32_t tag)
{
    return vp_meta_schema_register(s, tag, VP_META_STRING);
}

int vp_meta_schema_register_default_bool(vp_meta_schema_t *s, uint32_t tag)
{
    return vp_meta_schema_register(s, tag, VP_META_BOOL);
}

int vp_meta_schema_register_default_bytes(vp_meta_schema_t *s, uint32_t tag)
{
    return vp_meta_schema_register(s, tag, VP_META_BYTES);
}

/* === Schema bulk operations and full lifecycle ===
 *
 * The schema can be imported from a serialized representation,
 * exported to a byte stream, and copied between schemas. These
 * operations support tooling that round-trips schemas across
 * processes.
 */

#define VP_META_SCHEMA_MAGIC 0x4D534348u  /* "MSCH" */

int vp_meta_schema_export(const vp_meta_schema_t *s,
                          uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (!s || !out || !out_len) return VP_ERR_INVALID;
    size_t need = 8 + (size_t)s->count * 5u;
    if (out_cap < need) return VP_ERR_FULL;
    uint32_t magic = VP_META_SCHEMA_MAGIC;
    memcpy(out, &magic, 4);
    out[4] = (uint8_t)s->count;
    out[5] = (uint8_t)(s->count >> 8);
    out[6] = 0;
    out[7] = 0;
    size_t pos = 8;
    for (uint32_t i = 0; i < s->count; i++) {
        memcpy(out + pos, &s->table[i].tag, 4);
        out[pos + 4] = (uint8_t)s->table[i].type;
        pos += 5;
    }
    *out_len = pos;
    return VP_OK;
}

int vp_meta_schema_import(vp_meta_schema_t *s,
                          const uint8_t *in, size_t in_len)
{
    if (!s || !in || in_len < 8) return VP_ERR_INVALID;
    uint32_t magic;
    memcpy(&magic, in, 4);
    if (magic != VP_META_SCHEMA_MAGIC) return VP_ERR_INVALID;
    uint32_t count = (uint32_t)in[4] | ((uint32_t)in[5] << 8);
    if (8 + (size_t)count * 5u > in_len) return VP_ERR_LENGTH;
    vp_meta_schema_init(s);
    for (uint32_t i = 0; i < count; i++) {
        size_t off = 8 + (size_t)i * 5u;
        uint32_t tag;
        memcpy(&tag, in + off, 4);
        vp_meta_type_t type = (vp_meta_type_t)in[off + 4];
        int rc = vp_meta_schema_register(s, tag, type);
        if (rc != VP_OK) return rc;
    }
    return VP_OK;
}

int vp_meta_schema_clone(const vp_meta_schema_t *src, vp_meta_schema_t *dst)
{
    if (!src || !dst) return VP_ERR_INVALID;
    vp_meta_schema_init(dst);
    for (uint32_t i = 0; i < src->count; i++) {
        int rc = vp_meta_schema_register(dst, src->table[i].tag,
                                          src->table[i].type);
        if (rc != VP_OK) return rc;
    }
    return VP_OK;
}

int vp_meta_schema_validate_stream(const vp_meta_schema_t *s,
                                   const uint8_t *in, size_t in_len)
{
    if (!s || !in) return VP_ERR_INVALID;
    if (in_len < 4) return VP_ERR_TRUNCATED;
    if (memcmp(in, "META", 4) != 0) return VP_ERR_INVALID;
    uint32_t key_count = (uint32_t)in[4] | ((uint32_t)in[5] << 8);
    size_t off = 8;
    for (uint32_t k = 0; k < key_count; k++) {
        if (off + 64 > in_len) return VP_ERR_TRUNCATED;
        uint32_t tag = (uint32_t)in[off]
                     | ((uint32_t)in[off + 1] << 8)
                     | ((uint32_t)in[off + 2] << 16)
                     | ((uint32_t)in[off + 3] << 24);
        int rc = vp_meta_schema_validate(s, tag, in + off + 4, 60);
        if (rc != VP_OK) return rc;
        off += 64;
    }
    return VP_OK;
}

int vp_meta_schema_count_by_type(const vp_meta_schema_t *s, vp_meta_type_t type)
{
    if (!s) return 0;
    int n = 0;
    for (uint32_t i = 0; i < s->count; i++) {
        if (s->table[i].type == type) n++;
    }
    return n;
}

uint32_t vp_meta_schema_max_tag(const vp_meta_schema_t *s)
{
    if (!s || s->count == 0) return 0;
    uint32_t best = 0;
    for (uint32_t i = 0; i < s->count; i++) {
        if (s->table[i].tag > best) best = s->table[i].tag;
    }
    return best;
}

int vp_meta_schema_swap(vp_meta_schema_t *a, vp_meta_schema_t *b)
{
    if (!a || !b) return VP_ERR_INVALID;
    vp_meta_schema_t tmp = *a;
    *a = *b;
    *b = tmp;
    return VP_OK;
}