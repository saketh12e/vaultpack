/*
 * examples/read_archive.c
 *
 * Open a `.vpack` file from disk, walk its central directory, and print
 * each section. Compile manually:
 *
 *   cc -Iinclude examples/read_archive.c build/libvaultpack.a -o read_archive
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "vaultpack/vaultpack.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <file.vpack>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) { perror(argv[1]); return 1; }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t *buf = (uint8_t *)malloc((size_t)size);
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        fprintf(stderr, "short read\n");
        free(buf); fclose(fp); return 1;
    }
    fclose(fp);

    vp_archive_t *arc = NULL;
    int rc = vp_open_archive(buf, (size_t)size, &arc);
    if (rc != VP_OK) {
        fprintf(stderr, "open failed: %s\n", vp_strerror(rc));
        free(buf);
        return 1;
    }

    printf("vaultpack v%u — %u entries\n",
           arc->hdr.version, arc->hdr.entry_count);

    vp_dir_iter_t it;
    vp_dir_iter_init(&it, arc);
    const vp_dir_entry_t *e;
    while ((e = vp_dir_next(&it)) != NULL) {
        printf("  %-32s type=0x%02x size=%u\n",
               e->name, e->type, e->section_size);
    }

    vp_close_archive(arc);
    free(buf);
    return 0;
}
