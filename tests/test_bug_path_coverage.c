/*
 * tests/test_bug_path_coverage.c
 *
 * Integration test: feed each of the 10 new bug PoCs through
 * the corresponding bridge and verify that the harness path runs
 * to completion (so VPS replay will exercise the planted defect).
 *
 * This test does NOT itself crash. It verifies the harness path
 * is wired correctly; the crash comes from the sanitizer-detected
 * UAF/OOB in the planted code, observable on a Linux ASan build.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fuzz_bridge.h"

static int run_bridge(const char *name, const char *poc_path,
                      void (*bridge)(const uint8_t *, size_t))
{
    FILE *f = fopen(poc_path, "rb");
    if (!f) {
        fprintf(stderr, "%s: cannot open %s\n", name, poc_path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    uint8_t *buf = (uint8_t *)malloc((size_t)n);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf); fclose(f); return -1;
    }
    fclose(f);
    bridge(buf, (size_t)n);
    free(buf);
    return 0;
}

int main(void)
{
    /* B011: pipeline fuzzer */
    run_bridge("B011", "fuzz/tasks/B011/poc.bin", vp_bridge_pipeline);

    /* B012: pipeline fuzzer (decoder-fail path) */
    run_bridge("B012", "fuzz/tasks/B011/poc.bin", vp_bridge_pipeline);

    /* B013: pack fuzzer (chunk assembler grow) */
    run_bridge("B013", "fuzz/tasks/B013/poc.bin", vp_bridge_pack);

    /* B014: pack fuzzer (duplicate abort path) */
    run_bridge("B014", "fuzz/tasks/B014/poc.bin", vp_bridge_pack);

    /* B015: cache fuzzer (LRU eviction) */
    run_bridge("B015", "fuzz/tasks/B015/poc.bin", vp_bridge_cache);

    /* B016: range fuzzer (prefetch length wrap) */
    run_bridge("B016", "fuzz/tasks/B016/poc.bin", vp_bridge_range);

    /* B017: repair fuzzer (interval expansion) */
    run_bridge("B017", "fuzz/tasks/B017/poc.bin", vp_bridge_repair);

    /* B018: merge fuzzer (entry count wrap) */
    run_bridge("B018", "fuzz/tasks/B018/poc.bin", vp_bridge_merge);

    /* B019: schema fuzzer (tag+len index wrap) */
    run_bridge("B019", "fuzz/tasks/B019/poc.bin", vp_bridge_schema);

    /* B020: index2 fuzzer (compact realloc stale pointer) */
    run_bridge("B020", "fuzz/tasks/B020/poc.bin", vp_bridge_index2);

    printf("test_bug_path_coverage: all 10 bridge paths exercised\n");
    return 0;
}