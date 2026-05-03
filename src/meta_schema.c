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
        uint32_t len = (uint32_t)data[pos + 4]
                     | ((uint32_t)data[pos + 5] << 8)
                     | ((uint32_t)data[pos + 6] << 16)
                     | ((uint32_t)data[pos + 7] << 24);
        pos += 8;
        if (len > size - pos) len = (uint32_t)(size - pos);
        vp_meta_schema_validate(&s, tag, data + pos, len);
        pos += len;
    }
    return 0;
}