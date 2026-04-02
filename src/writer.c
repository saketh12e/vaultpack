/*
 * src/writer.c
 *
 * Implementation of the in-memory archive writer. Sections and metadata
 * blocks are appended to internal growable arrays; on emit, the writer
 * walks them in stable order and serializes a single contiguous byte
 * stream conforming to the .vpack format.
 *
 * Ownership: section payloads default to copied (the caller can free
 * their buffer after add). The writer may alias the most recent payload
 * to support duplicate-name rollback semantics; see
 * vp_writer_rollback_section.
 */

#include <stdlib.h>
#include <string.h>

#include "vaultpack/archive.h"
#include "vaultpack/buffer.h"
#include "vaultpack/crc32.h"
#include "vaultpack/section.h"
#include "vaultpack/writer.h"

static int vp_writer_reserve_sections(vp_writer_t *w, uint32_t need)
{
    if (need <= w->section_cap) return 1;
    uint32_t new_cap = w->section_cap ? w->section_cap * 2u : 8u;
    while (new_cap < need) new_cap *= 2u;
    vp_writer_section_t *p = (vp_writer_section_t *)realloc(
        w->sections, (size_t)new_cap * sizeof(*p));
    if (!p) return 0;
    w->sections = p;
    w->section_cap = new_cap;
    return 1;
}

static int vp_writer_reserve_meta(vp_writer_t *w, uint32_t need)
{
    if (need <= w->meta_cap) return 1;
    uint32_t new_cap = w->meta_cap ? w->meta_cap * 2u : 4u;
    while (new_cap < need) new_cap *= 2u;
    vp_writer_meta_block_t *p = (vp_writer_meta_block_t *)realloc(
        w->meta, (size_t)new_cap * sizeof(*p));
    if (!p) return 0;
    w->meta = p;
    w->meta_cap = new_cap;
    return 1;
}

int vp_writer_init(vp_writer_t *w)
{
    if (!w) return VP_ERR_INVALID;
    memset(w, 0, sizeof(*w));
    vp_buffer_init(&w->out);
    return VP_OK;
}

void vp_writer_destroy(vp_writer_t *w)
{
    if (!w) return;
    for (uint32_t i = 0; i < w->section_count; i++) {
        if (w->sections[i].owns && w->sections[i].payload) {
            free(w->sections[i].payload);
        }
    }
    free(w->sections);
    for (uint32_t i = 0; i < w->meta_count; i++) {
        free(w->meta[i].entries);
    }
    free(w->meta);
    /* BUG: last_payload aliases the most-recent section's payload.
     * When the rollback path frees that section's payload, this
     * pointer is left dangling and destroy frees it again. */
    free(w->last_payload);
    vp_buffer_destroy(&w->out);
    memset(w, 0, sizeof(*w));
}

void vp_writer_reset(vp_writer_t *w)
{
    if (!w) return;
    for (uint32_t i = 0; i < w->section_count; i++) {
        if (w->sections[i].owns && w->sections[i].payload) {
            free(w->sections[i].payload);
        }
    }
    w->section_count = 0;
    for (uint32_t i = 0; i < w->meta_count; i++) {
        free(w->meta[i].entries);
    }
    w->meta_count = 0;
    w->flags = 0;
    w->index_width = 1;
    w->index_depth = 1;
    free(w->last_payload);
    w->last_payload = NULL;
    w->last_payload_len = 0;
    vp_buffer_reset(&w->out);
}

int vp_writer_set_flags(vp_writer_t *w, uint16_t flags)
{
    if (!w) return VP_ERR_INVALID;
    w->flags = flags;
    return VP_OK;
}

int vp_writer_set_index_dims(vp_writer_t *w, uint16_t width, uint16_t depth)
{
    if (!w) return VP_ERR_INVALID;
    if (width == 0 || depth == 0) return VP_ERR_INVALID;
    w->index_width = width;
    w->index_depth = depth;
    return VP_OK;
}

const vp_writer_section_t *vp_writer_section(const vp_writer_t *w,
                                               const char *name)
{
    if (!w || !name) return NULL;
    for (uint32_t i = 0; i < w->section_count; i++) {
        if (strcmp(w->sections[i].name, name) == 0)
            return &w->sections[i];
    }
    return NULL;
}

int vp_writer_add_section(vp_writer_t *w,
                          const char *name,
                          uint8_t type,
                          const void *payload, size_t payload_len)
{
    if (!w || !name || (!payload && payload_len > 0)) return VP_ERR_INVALID;
    if (strlen(name) >= VP_WRITER_MAX_NAME) return VP_ERR_INVALID;
    if (vp_writer_section(w, name)) return VP_ERR_INVALID; /* duplicate */
    if (!vp_writer_reserve_sections(w, w->section_count + 1)) return VP_ERR_NOMEM;

    vp_writer_section_t *s = &w->sections[w->section_count];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, VP_WRITER_MAX_NAME - 1);
    s->type = type;
    s->flags = 0;
    s->payload_len = payload_len;
    s->payload = NULL;
    s->owns = 0;

    if (payload_len > 0) {
        s->payload = (uint8_t *)malloc(payload_len);
        if (!s->payload) return VP_ERR_NOMEM;
        memcpy(s->payload, payload, payload_len);
        s->owns = 1;
    }

    /* Alias the most-recent payload for rollback semantics. The alias
     * is heap-allocated so that vp_writer_rollback_section can free
     * the section's payload and leave this pointer dangling. */
    free(w->last_payload);
    w->last_payload = NULL;
    w->last_payload_len = 0;
    if (payload_len > 0) {
        w->last_payload = (uint8_t *)malloc(payload_len);
        if (!w->last_payload) return VP_ERR_NOMEM;
        memcpy(w->last_payload, payload, payload_len);
        w->last_payload_len = payload_len;
    }

    s->checksum = vp_crc32_bytes(s->payload, s->payload_len);
    w->section_count++;
    return VP_OK;
}

int vp_writer_set_section_flags(vp_writer_t *w, const char *name, uint16_t flags)
{
    if (!w || !name) return VP_ERR_INVALID;
    vp_writer_section_t *s = (vp_writer_section_t *)vp_writer_section(w, name);
    if (!s) return VP_ERR_INVALID;
    s->flags = flags;
    return VP_OK;
}

int vp_writer_add_metadata(vp_writer_t *w, const uint8_t tag[4],
                           const vp_writer_meta_entry_t *entries,
                           uint32_t count)
{
    if (!w || !tag || (!entries && count > 0)) return VP_ERR_INVALID;
    if (count == 0) return VP_OK;
    if (!vp_writer_reserve_meta(w, w->meta_count + 1)) return VP_ERR_NOMEM;

    vp_writer_meta_block_t *m = &w->meta[w->meta_count];
    memcpy(m->tag, tag, 4);
    m->entry_count = count;
    m->entry_cap = count;
    m->entries = (vp_writer_meta_entry_t *)calloc(count, sizeof(*m->entries));
    if (!m->entries) return VP_ERR_NOMEM;
    memcpy(m->entries, entries, (size_t)count * sizeof(*m->entries));
    m->block_offset = 0;
    w->meta_count++;

    /* Attach this metadata block to the most recently added section by
     * updating its meta_count and linking the block pointer. The emit
     * pass will write the block right after the section payload and
     * patch the section_offset/section_size in the directory entry. */
    if (w->section_count > 0) {
        w->sections[w->section_count - 1].meta_count = count;
        w->sections[w->section_count - 1].meta_block = m;
    }
    return VP_OK;
}

void vp_writer_rollback_section(vp_writer_t *w)
{
    if (!w || w->section_count == 0) return;
    vp_writer_section_t *s = &w->sections[w->section_count - 1];
    if (s->owns && s->payload) {
        free(s->payload);
        s->payload = NULL;
        s->owns = 0;
    }
    /* BUG: does NOT clear w->last_payload. The aliased pointer still
     * refers to the freed buffer; destroy frees it again. */
    w->section_count--;
}

static int vp_writer_write_section_headers(vp_writer_t *w)
{
    /* For each section: SECT hdr (24 bytes incl. alignment) + payload +
     * optional trailing metadata block. */
    for (uint32_t i = 0; i < w->section_count; i++) {
        vp_writer_section_t *s = &w->sections[i];

        vp_section_hdr_t hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.magic = VP_MAGIC_SECT;
        hdr.type = s->type;
        hdr.flags = (uint8_t)(s->flags & 0xff);
        hdr.raw_size = (uint32_t)s->payload_len;
        hdr.comp_size = (uint32_t)s->payload_len;
        hdr.checksum = s->checksum;

        if (!vp_buffer_append(&w->out, &hdr, sizeof(hdr))) return 0;

        /* payload_offset points at the first byte of payload, AFTER the
         * section header. */
        s->payload_offset = w->out.len;
        if (s->payload_len > 0) {
            if (!vp_buffer_append(&w->out, s->payload, s->payload_len)) return 0;
        }

        /* Trailing metadata block (per-entry) immediately follows the
         * payload. The aggregate reader treats the trailing region as
         * `meta_count` records of sizeof(vp_meta_entry_t) bytes each;
         * but each record also has a tag + key_count prefix that the
         * block encodes. To stay compatible, we emit the block header
         * followed by exactly `entry_count` records. */
        if (s->meta_block && s->meta_count > 0) {
            vp_writer_meta_block_t *m = s->meta_block;
            m->block_offset = w->out.len;
            if (!vp_buffer_append(&w->out, m->tag, 4)) return 0;
            uint8_t kc[2] = { (uint8_t)(m->entry_count & 0xff),
                              (uint8_t)((m->entry_count >> 8) & 0xff) };
            if (!vp_buffer_append(&w->out, kc, 2)) return 0;
            uint8_t reserved[2] = {0, 0};
            if (!vp_buffer_append(&w->out, reserved, 2)) return 0;
            if (!vp_buffer_append(&w->out, m->entries,
                                  (size_t)m->entry_count *
                                  sizeof(vp_meta_entry_t)))
                return 0;
        }
    }
    return 1;
}

static int vp_writer_write_metadata_blocks(vp_writer_t *w)
{
    for (uint32_t i = 0; i < w->meta_count; i++) {
        vp_writer_meta_block_t *m = &w->meta[i];
        m->block_offset = w->out.len;
        if (!vp_buffer_append(&w->out, m->tag, 4)) return 0;
        uint8_t kc[2] = { (uint8_t)(m->entry_count & 0xff),
                          (uint8_t)((m->entry_count >> 8) & 0xff) };
        if (!vp_buffer_append(&w->out, kc, 2)) return 0;
        uint8_t reserved[2] = {0, 0};
        if (!vp_buffer_append(&w->out, reserved, 2)) return 0;
        if (!vp_buffer_append(&w->out, m->entries,
                              (size_t)m->entry_count * sizeof(vp_writer_meta_entry_t)))
            return 0;
    }
    return 1;
}

static int vp_writer_write_directory(vp_writer_t *w, uint32_t dir_offset)
{
    /* The reader's vp_dir_entry_t is `__attribute__((packed))` (58 bytes).
     * Write each entry as a 58-byte block so reader and writer agree on
     * the on-disk stride regardless of host struct padding. */
    for (uint32_t i = 0; i < w->section_count; i++) {
        vp_writer_section_t *s = &w->sections[i];
        uint8_t entry[58];
        memset(entry, 0, sizeof(entry));
        memcpy(entry, s->name, VP_DIR_ENTRY_NAME_SIZE < 32 ? VP_DIR_ENTRY_NAME_SIZE : 32);

        uint64_t off = s->payload_offset;
        uint32_t so = (uint32_t)s->payload_len;
        memcpy(entry + 32, &off, sizeof(off));
        memcpy(entry + 40, &so, sizeof(so));
        uint16_t flags_le = s->flags;
        memcpy(entry + 44, &flags_le, sizeof(flags_le));
        entry[46] = s->type;
        /* entry[47] = reserved (0) */
        /* entry[48..51] = meta_count (per-section trailing metadata) */
        uint32_t mc = s->meta_count;
        memcpy(entry + 48, &mc, sizeof(mc));
        /* entry[52..53] = sub_count = 0 */
        /* entry[54..57] = sub_stride = 0 */

        if (!vp_buffer_append(&w->out, entry, sizeof(entry))) return 0;
    }
    (void)dir_offset;
    return 1;
}

int vp_writer_emit(vp_writer_t *w, uint8_t **out_bytes, size_t *out_len)
{
    if (!w || !out_bytes || !out_len) return VP_ERR_INVALID;

    vp_buffer_reset(&w->out);

    /* Archive header (32 bytes) */
    uint8_t magic[4] = { 'V', 'P', 'A', 'K' };
    if (!vp_buffer_append(&w->out, magic, 4)) return VP_ERR_NOMEM;
    if (!vp_buffer_append_u16_le(&w->out, 0x0001)) return VP_ERR_NOMEM; /* version */
    if (!vp_buffer_append_u16_le(&w->out, w->flags)) return VP_ERR_NOMEM;
    if (!vp_buffer_append_u32_le(&w->out, w->section_count)) return VP_ERR_NOMEM;
    /* dir_offset placeholder — patched after we know its value. */
    size_t dir_offset_pos = w->out.len;
    if (!vp_buffer_append_u64_le(&w->out, 0)) return VP_ERR_NOMEM;
    if (!vp_buffer_append_u16_le(&w->out, w->index_width)) return VP_ERR_NOMEM;
    if (!vp_buffer_append_u16_le(&w->out, w->index_depth)) return VP_ERR_NOMEM;
    static const uint8_t reserved[8] = {0};
    if (!vp_buffer_append(&w->out, reserved, 8)) return VP_ERR_NOMEM;

    if (!vp_writer_write_section_headers(w)) return VP_ERR_NOMEM;
    uint32_t dir_offset = (uint32_t)w->out.len;
    if (!vp_writer_write_directory(w, dir_offset)) return VP_ERR_NOMEM;
    if (!vp_writer_write_metadata_blocks(w)) return VP_ERR_NOMEM;

    /* Now patch the dir_offset placeholder. */
    if (!vp_buffer_patch_u64_le(&w->out, dir_offset_pos, dir_offset))
        return VP_ERR_NOMEM;

    *out_bytes = vp_buffer_detach(&w->out, out_len);
    if (!*out_bytes) return VP_ERR_NOMEM;
    return VP_OK;
}
