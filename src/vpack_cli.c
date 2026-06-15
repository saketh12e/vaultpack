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
#include "vaultpack/repair.h"
#include "vaultpack/merge.h"
#include "vaultpack/range_io.h"

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
        "  vpack repair  <file.vpack>\n"
        "  vpack merge   <a.vpack> <b.vpack> <out.vpack>\n"
        "  vpack diff    <a.vpack> <b.vpack>\n"
        "  vpack inspect <file.vpack>\n"
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

static int cmd_repair(const char *path)
{
    uint8_t *buf;
    size_t   len;
    buf = read_file(path, &len);
    if (!buf) return 1;
    vp_repair_ctx_t r;
    vp_repair_ctx_init(&r, buf, len);
    int rc = vp_repair_scan(&r);
    if (rc != VP_OK) {
        fprintf(stderr, "vpack: repair scan failed: %s\n", vp_strerror(rc));
        vp_repair_ctx_destroy(&r);
        free(buf);
        return 1;
    }
    printf("repair: %u sections recovered\n", vp_repair_count(&r));
    printf("raw total: %u bytes\n", vp_repair_total_raw(&r));
    printf("comp total: %u bytes\n", vp_repair_total_comp(&r));
    for (uint32_t i = 0; i < r.count; i++) {
        vp_repair_record_t rec;
        vp_repair_at(&r, i, &rec);
        printf("  [%u] offset=0x%llx raw=%u comp=%u crc=0x%08x\n",
               i, (unsigned long long)rec.offset, rec.raw_size,
               rec.comp_size, rec.crc);
    }
    vp_repair_ctx_destroy(&r);
    free(buf);
    return 0;
}

static int cmd_merge(const char *a_path, const char *b_path, const char *out_path)
{
    uint8_t *a_buf, *b_buf;
    size_t   a_len, b_len;
    a_buf = read_file(a_path, &a_len);
    if (!a_buf) return 1;
    b_buf = read_file(b_path, &b_len);
    if (!b_buf) { free(a_buf); return 1; }

    vp_merge_ctx_t m;
    vp_merge_ctx_init(&m, a_buf, a_len, b_buf, b_len);
    int rc = vp_merge_run(&m);
    if (rc != VP_OK) {
        fprintf(stderr, "vpack: merge failed: %s\n", vp_strerror(rc));
        vp_merge_ctx_destroy(&m);
        free(a_buf); free(b_buf);
        return 1;
    }
    printf("merged %s + %s -> %u conflicts\n",
           a_path, b_path, vp_merge_conflict_count(&m));
    printf("(output to %s not yet implemented; use the merge library API)\n",
           out_path);
    vp_merge_ctx_destroy(&m);
    free(a_buf); free(b_buf);
    return 0;
}

static int cmd_diff(const char *a_path, const char *b_path)
{
    uint8_t *a_buf, *b_buf;
    size_t   a_len, b_len;
    a_buf = read_file(a_path, &a_len);
    if (!a_buf) return 1;
    b_buf = read_file(b_path, &b_len);
    if (!b_buf) { free(a_buf); return 1; }

    vp_diff_ctx_t d;
    int rc = vp_diff_run_full(&d, a_buf, a_len, b_buf, b_len);
    if (rc != VP_OK) {
        fprintf(stderr, "vpack: diff failed: %s\n", vp_strerror(rc));
        vp_diff_ctx_destroy(&d);
        free(a_buf); free(b_buf);
        return 1;
    }
    printf("diff: %u added, %u removed, %u changed\n",
           vp_diff_added_count(&d),
           vp_diff_removed_count(&d),
           vp_diff_changed_count(&d));
    char buf[4096];
    size_t out_len = 0;
    if (vp_diff_format_text(&d, buf, sizeof(buf), &out_len) == VP_OK) {
        fwrite(buf, 1, out_len, stdout);
    }
    vp_diff_ctx_destroy(&d);
    free(a_buf); free(b_buf);
    return 0;
}

static int cmd_inspect(const char *path)
{
    uint8_t *buf;
    size_t   len;
    buf = read_file(path, &len);
    if (!buf) return 1;
    vp_range_io_t r;
    vp_range_io_init(&r, buf, len);
    printf("file:       %s (%zu bytes)\n", path, len);
    printf("magic:      %c%c%c%c\n",
           (char)buf[0], (char)buf[1], (char)buf[2], (char)buf[3]);
    printf("crc32 of [0,32): 0x%08x\n",
           (unsigned)vp_range_io_crc32(&r, 0, 32));
    printf("crc32 of [0,size): 0x%08x\n",
           (unsigned)vp_range_io_crc32(&r, 0, len));
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
    if (strcmp(argv[1], "repair") == 0) {
        if (argc != 3) return usage();
        return cmd_repair(argv[2]);
    }
    if (strcmp(argv[1], "merge") == 0) {
        if (argc != 5) return usage();
        return cmd_merge(argv[2], argv[3], argv[4]);
    }
    if (strcmp(argv[1], "diff") == 0) {
        if (argc != 4) return usage();
        return cmd_diff(argv[2], argv[3]);
    }
    if (strcmp(argv[1], "inspect") == 0) {
        if (argc != 3) return usage();
        return cmd_inspect(argv[2]);
    }
    if (strcmp(argv[1], "create") == 0) {
        fprintf(stderr, "vpack: 'create' is deprecated; use 'pack'\n");
        return 1;
    }
    return usage();
}
