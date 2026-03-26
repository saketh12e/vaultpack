/*
 * src/vpack_cli.c
 *
 * VaultPack command-line tool.
 *
 *   vpack list   <file>
 *   vpack info   <file>
 *   vpack verify <file>
 *   vpack pack   <manifest> <out.vpack>
 *   vpack extract <archive> <section>
 *   vpack create <out.vpack> <files...>      (deprecated; prints warning)
 */

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
        "  vpack list    <file.vpack>\n"
        "  vpack info    <file.vpack>\n"
        "  vpack verify  <file.vpack>\n"
        "  vpack pack    <manifest> <out.vpack>\n"
        "  vpack extract <file.vpack> <section>\n"
        "\n");
    return 1;
}

static uint8_t *read_file(const char *path, size_t *out_size)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "vpack: cannot open %s\n", path);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(fp);
        fprintf(stderr, "vpack: empty file %s\n", path);
        return NULL;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        free(buf);
        fclose(fp);
        fprintf(stderr, "vpack: short read on %s\n", path);
        return NULL;
    }
    fclose(fp);
    *out_size = (size_t)sz;
    return buf;
}

static int cmd_list(const char *path)
{
    uint8_t *buf;
    size_t   len;
    buf = read_file(path, &len);
    if (!buf) return 1;
    vp_archive_t *arc = NULL;
    int rc = vp_open_archive(buf, len, &arc);
    if (rc != VP_OK) {
        fprintf(stderr, "vpack: open failed: %s\n", vp_strerror(rc));
        free(buf);
        return 1;
    }
    printf("vaultpack v%u — %u entries\n",
           arc->hdr.version, arc->hdr.entry_count);
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

static int cmd_info(const char *path)
{
    uint8_t *buf;
    size_t   len;
    buf = read_file(path, &len);
    if (!buf) return 1;
    vp_archive_t *arc = NULL;
    int rc = vp_open_archive(buf, len, &arc);
    if (rc != VP_OK) {
        fprintf(stderr, "vpack: open failed: %s\n", vp_strerror(rc));
        free(buf);
        return 1;
    }
    printf("file:       %s\n", path);
    printf("version:    0x%04x\n", arc->hdr.version);
    printf("flags:      0x%04x\n", arc->hdr.flags);
    printf("entries:    %u\n", arc->hdr.entry_count);
    printf("dir_offset: 0x%llx\n", (unsigned long long)arc->hdr.dir_offset);
    printf("index:      %u x %u\n", arc->hdr.index_width, arc->hdr.index_depth);
    printf("size:       %zu bytes\n", len);
    vp_close_archive(arc);
    free(buf);
    return 0;
}

static int cmd_verify(const char *path)
{
    uint8_t *buf;
    size_t   len;
    buf = read_file(path, &len);
    if (!buf) return 1;
    vp_archive_t *arc = NULL;
    int rc = vp_open_archive(buf, len, &arc);
    if (rc != VP_OK) {
        fprintf(stderr, "vpack: open failed: %s\n", vp_strerror(rc));
        free(buf);
        return 1;
    }
    vp_validate_report_t r;
    vp_validate_report_init(&r);
    int vrc = vp_validate_archive(arc, &r);
    printf("validate: %s\n", vrc == VP_OK ? "OK" : "ISSUES");
    printf("issues:   %u\n", r.issue_count);
    for (uint32_t i = 0; i < r.issue_count; i++) {
        const char *sev = "INFO";
        if (r.issues[i].severity == VP_VALIDATE_WARN) sev = "WARN";
        else if (r.issues[i].severity == VP_VALIDATE_ERROR) sev = "ERROR";
        printf("  [%s] sec=%u: %s\n", sev,
               r.issues[i].section_index, r.issues[i].message);
    }
    vp_validate_report_free(&r);
    vp_close_archive(arc);
    free(buf);
    return vrc == VP_OK ? 0 : 1;
}

static int cmd_pack(const char *manifest_path, const char *out_path)
{
    uint8_t *buf;
    size_t   len;
    buf = read_file(manifest_path, &len);
    if (!buf) return 1;

    vp_manifest_t m;
    vp_manifest_init(&m);
    int rc = vp_manifest_parse(&m, (const char *)buf, len);
    free(buf);
    if (rc != VP_OK) {
        fprintf(stderr, "vpack: manifest parse failed: %s\n",
                vp_strerror(rc));
        vp_manifest_free(&m);
        return 1;
    }

    vp_writer_t w;
    vp_writer_init(&w);
    rc = vp_manifest_apply(&m, &w);
    vp_manifest_free(&m);
    if (rc != VP_OK) {
        fprintf(stderr, "vpack: manifest apply failed: %s\n",
                vp_strerror(rc));
        vp_writer_destroy(&w);
        return 1;
    }

    uint8_t *out_bytes;
    size_t   out_len;
    rc = vp_writer_emit(&w, &out_bytes, &out_len);
    vp_writer_destroy(&w);
    if (rc != VP_OK) {
        fprintf(stderr, "vpack: emit failed: %s\n", vp_strerror(rc));
        return 1;
    }

    FILE *fp = fopen(out_path, "wb");
    if (!fp) {
        fprintf(stderr, "vpack: cannot open %s for write\n", out_path);
        free(out_bytes);
        return 1;
    }
    if (fwrite(out_bytes, 1, out_len, fp) != out_len) {
        fprintf(stderr, "vpack: short write to %s\n", out_path);
        fclose(fp);
        free(out_bytes);
        return 1;
    }
    fclose(fp);
    free(out_bytes);
    printf("wrote %s (%zu bytes)\n", out_path, out_len);
    return 0;
}

static int cmd_extract(const char *archive, const char *section_name)
{
    uint8_t *buf;
    size_t   len;
    buf = read_file(archive, &len);
    if (!buf) return 1;
    vp_archive_t *arc = NULL;
    int rc = vp_open_archive(buf, len, &arc);
    if (rc != VP_OK) {
        fprintf(stderr, "vpack: open failed: %s\n", vp_strerror(rc));
        free(buf);
        return 1;
    }

    const vp_dir_entry_t *found = NULL;
    for (uint32_t i = 0; i < arc->dir_count; i++) {
        if (strcmp(arc->dir[i].name, section_name) == 0) {
            found = &arc->dir[i];
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "vpack: section '%s' not found\n", section_name);
        vp_close_archive(arc);
        free(buf);
        return 1;
    }

    if (found->section_offset + found->section_size > len) {
        fprintf(stderr, "vpack: section truncated\n");
        vp_close_archive(arc);
        free(buf);
        return 1;
    }
    fwrite(buf + found->section_offset, 1, found->section_size, stdout);
    vp_close_archive(arc);
    free(buf);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return usage();

    if (strcmp(argv[1], "list") == 0) {
        if (argc != 3) return usage();
        return cmd_list(argv[2]);
    }
    if (strcmp(argv[1], "info") == 0) {
        if (argc != 3) return usage();
        return cmd_info(argv[2]);
    }
    if (strcmp(argv[1], "verify") == 0) {
        if (argc != 3) return usage();
        return cmd_verify(argv[2]);
    }
    if (strcmp(argv[1], "pack") == 0) {
        if (argc != 4) return usage();
        return cmd_pack(argv[2], argv[3]);
    }
    if (strcmp(argv[1], "extract") == 0) {
        if (argc != 4) return usage();
        return cmd_extract(argv[2], argv[3]);
    }
    if (strcmp(argv[1], "create") == 0) {
        fprintf(stderr, "vpack: 'create' is deprecated; use 'pack'\n");
        return 1;
    }
    return usage();
}
