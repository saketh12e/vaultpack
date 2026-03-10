/*
 * examples/create_archive.c
 *
 * Construct a tiny in-memory `.vpack` archive and write it to disk. Used by the
 * README quick-start. Not part of the built library — compile manually:
 *
 *   cc -Iinclude examples/create_archive.c build/libvaultpack.a -o create_archive
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "vaultpack/vaultpack.h"

static void put_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_le64(uint8_t *p, uint64_t v) {
    put_le32(p,     (uint32_t)v);
    put_le32(p + 4, (uint32_t)(v >> 32));
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "out.vpack";

    /* Minimal archive with one section named "hello" containing "world\n". */
    uint8_t buf[64 + 16] = {0};

    memcpy(buf,     "VPAK",      4);
    put_le16(buf+4, 1);              /* version */
    put_le16(buf+6, 0);              /* flags */
    put_le32(buf+8, 1);              /* entry_count */
    put_le64(buf+12, 32);            /* dir_offset */
    put_le16(buf+20, 1);             /* index_width */
    put_le16(buf+22, 1);             /* index_depth */

    /* Central directory entry: 52 bytes starting at offset 32 */
    uint8_t *ent = buf + 32;
    memcpy(ent,    "hello", 5);
    put_le64(ent+32, 32 + 52);       /* section_offset */
    put_le32(ent+40, 16);            /* section_size */
    put_le16(ent+44, 0);
    ent[46] = 1;                     /* type = raw */
    ent[47] = 0;
    put_le32(ent+48, 0);             /* meta_count */

    /* Section data: a 20-byte SECT header + "world\n\0..." */
    uint8_t *sec = buf + 32 + 52;
    memcpy(sec, "SECT", 4);
    sec[4] = 1;                      /* type = raw */
    put_le32(sec+12, 16);            /* comp_size */
    memcpy(sec + 20, "world\n", 6);

    FILE *fp = fopen(path, "wb");
    if (!fp) { perror(path); return 1; }
    fwrite(buf, 1, sizeof(buf), fp);
    fclose(fp);

    printf("wrote %s (%zu bytes)\n", path, sizeof(buf));
    return 0;
}
