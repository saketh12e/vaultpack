#include <stdlib.h>
#include <string.h>

#include "vaultpack/archive.h"
#include "vaultpack/error.h"

int vp_check_header(const uint8_t *data, size_t size)
{
    if (!data || size < VP_ARCHIVE_HDR_SIZE)
        return VP_ERR_TRUNCATED;

    if (memcmp(data, VP_MAGIC_VPAK_BYTES, 4) != 0)
        return VP_ERR_INVALID;

    uint16_t version = (uint16_t)data[4] | ((uint16_t)data[5] << 8);
    if (version != VP_ARCHIVE_VERSION)
        return VP_ERR_INVALID;

    return VP_OK;
}

static int vp_read_directory(vp_archive_t *arc, const uint8_t *data, size_t size)
{
    if (arc->hdr.dir_offset > size)
        return VP_ERR_TRUNCATED;

    size_t need = (size_t)arc->hdr.entry_count * sizeof(vp_dir_entry_t);
    if (need > size - (size_t)arc->hdr.dir_offset)
        return VP_ERR_TRUNCATED;

    arc->dir = malloc(need);
    if (!arc->dir)
        return VP_ERR_NOMEM;

    memcpy(arc->dir, data + arc->hdr.dir_offset, need);
    arc->dir_count = arc->hdr.entry_count;
    return VP_OK;
}

static int vp_aggregate_metadata(vp_archive_t *arc,
                                 const uint8_t *data, size_t size)
{
    uint32_t total = 0;
    for (uint32_t i = 0; i < arc->dir_count; i++)
        total += arc->dir[i].meta_count;

    if (total == 0)
        return VP_OK;

    arc->meta_flat = calloc(total, sizeof(vp_meta_entry_t));
    if (!arc->meta_flat)
        return VP_ERR_NOMEM;
    arc->meta_total = total;

    uint32_t dst = 0;
    for (uint32_t i = 0; i < arc->dir_count; i++) {
        size_t base = (size_t)arc->dir[i].section_offset
                    + arc->dir[i].section_size;

        for (uint32_t j = 0; j < arc->dir[i].meta_count; j++) {
            if (base + sizeof(vp_meta_entry_t) > size)
                break;
            memcpy(&arc->meta_flat[dst],
                   data + base,
                   sizeof(vp_meta_entry_t));
            dst++;
            base += sizeof(vp_meta_entry_t);
        }
    }

    return VP_OK;
}

int vp_open_archive(const uint8_t *data, size_t size, vp_archive_t **out)
{
    if (!out || !data)
        return VP_ERR_INVALID;

    int rc = vp_check_header(data, size);
    if (rc != VP_OK)
        return rc;

    vp_archive_t *arc = calloc(1, sizeof(*arc));
    if (!arc)
        return VP_ERR_NOMEM;

    memcpy(&arc->hdr, data, sizeof(arc->hdr));

    rc = vp_read_directory(arc, data, size);
    if (rc != VP_OK) {
        vp_close_archive(arc);
        return rc;
    }

    rc = vp_aggregate_metadata(arc, data, size);
    if (rc != VP_OK) {
        vp_close_archive(arc);
        return rc;
    }

    *out = arc;
    return VP_OK;
}

void vp_close_archive(vp_archive_t *arc)
{
    if (!arc)
        return;
    free(arc->dir);
    free(arc->meta_flat);
    free(arc);
}

void vp_dir_iter_init(vp_dir_iter_t *it, const vp_archive_t *arc)
{
    if (!it)
        return;
    it->arc = arc;
    it->pos = 0;
}

const vp_dir_entry_t *vp_dir_next(vp_dir_iter_t *it)
{
    if (!it || !it->arc || it->pos >= it->arc->dir_count)
        return NULL;
    return &it->arc->dir[it->pos++];
}
