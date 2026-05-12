/*
 * examples/repair_merge_workflow.c
 *
 * Demonstrates the archive repair scanner: open a blob, scan for
 * recoverable SECT headers, then build a section table from the
 * found records. Also demonstrates the simple two-archive merger
 * with a synthetic input.
 *
 * Build: cc examples/repair_merge_workflow.c src/*.c -I include -o /tmp/rmw
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/repair.h"
#include "vaultpack/merge.h"

static void build_blob_with_sects(uint8_t *out, size_t *out_len)
{
    size_t pos = 0;
    /* Place three SECT headers with payloads. */
    for (int i = 0; i < 3; i++) {
        memcpy(out + pos, "SECT", 4); pos += 4;
        out[pos++] = 1;
        out[pos++] = 0;
        out[pos++] = 0;
        out[pos++] = 0;
        /* raw_size, comp_size, crc, flags as LE u32 */
        uint32_t s = 8;
        memcpy(out + pos, &s, 4); pos += 4;
        memcpy(out + pos, &s, 4); pos += 4;
        uint32_t crc = 0;
        memcpy(out + pos, &crc, 4); pos += 4;
        uint32_t flags = 0;
        memcpy(out + pos, &flags, 4); pos += 4;
        for (int k = 0; k < 8; k++) out[pos++] = (uint8_t)('A' + i * 8 + k);
    }
    *out_len = pos;
}

int main(void)
{
    uint8_t blob[1024];
    size_t  blob_len = 0;
    build_blob_with_sects(blob, &blob_len);
    printf("built synthetic archive blob of %zu bytes\n", blob_len);

    vp_repair_ctx_t r;
    vp_repair_ctx_init(&r, blob, blob_len);
    if (vp_repair_scan(&r) != VP_OK) {
        fprintf(stderr, "scan failed\n");
        return 1;
    }
    printf("repair found %u section records\n", r.count);
    for (uint32_t i = 0; i < r.count; i++) {
        printf("  offset=%llu raw_size=%u comp_size=%u\n",
               (unsigned long long)r.records[i].offset,
               r.records[i].raw_size,
               r.records[i].comp_size);
    }
    vp_repair_ctx_destroy(&r);

    /* Build two minimal archives and merge them. */
    uint8_t a[32] = {0};
    uint8_t b[32] = {0};
    memcpy(a, "VPAK", 4);
    memcpy(b, "VPAK", 4);
    vp_merge_ctx_t m;
    vp_merge_ctx_init(&m, a, sizeof(a), b, sizeof(b));
    if (vp_merge_run(&m) != VP_OK) {
        fprintf(stderr, "merge failed\n");
    } else {
        printf("merge succeeded\n");
    }
    vp_merge_ctx_destroy(&m);
    return 0;
}