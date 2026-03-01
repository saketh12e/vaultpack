#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/vaultpack.h"

static int usage(void)
{
    fprintf(stderr,
        "vpack — vaultpack command-line tool\n"
        "\n"
        "Usage:\n"
        "  vpack create <out.vpack> <files...>\n"
        "  vpack list   <in.vpack>\n"
        "\n");
    return 1;
}

static int cmd_list(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "vpack: cannot open %s\n", path);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) {
        fclose(fp);
        fprintf(stderr, "vpack: empty file %s\n", path);
        return 1;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)size);
    if (!buf) {
        fclose(fp);
        fprintf(stderr, "vpack: out of memory\n");
        return 1;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        fprintf(stderr, "vpack: short read on %s\n", path);
        return 1;
    }
    fclose(fp);

    vp_archive_t *arc = NULL;
    int rc = vp_open_archive(buf, (size_t)size, &arc);
    if (rc != VP_OK) {
        fprintf(stderr, "vpack: open failed: %s\n", vp_strerror(rc));
        free(buf);
        return 1;
    }

    printf("version=0x%04x flags=0x%04x entries=%u dir_offset=0x%llx\n",
           arc->hdr.version, arc->hdr.flags,
           arc->hdr.entry_count,
           (unsigned long long)arc->hdr.dir_offset);

    vp_dir_iter_t it;
    vp_dir_iter_init(&it, arc);
    const vp_dir_entry_t *e;
    while ((e = vp_dir_next(&it)) != NULL) {
        printf("  %-32s type=0x%02x size=%u offset=0x%llx\n",
               e->name, e->type, e->section_size,
               (unsigned long long)e->section_offset);
    }

    vp_close_archive(arc);
    free(buf);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return usage();

    if (strcmp(argv[1], "create") == 0) {
        if (argc < 4)
            return usage();
        fprintf(stderr, "vpack: create is not implemented yet\n");
        return 1;
    }

    if (strcmp(argv[1], "list") == 0) {
        if (argc != 3)
            return usage();
        return cmd_list(argv[2]);
    }

    return usage();
}
