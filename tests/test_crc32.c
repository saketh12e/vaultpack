#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "vaultpack/crc32.h"

/* Known CRC32 vectors (IEEE polynomial). */
int main(void)
{
    /* Empty input */
    uint32_t c0 = vp_crc32_bytes(NULL, 0);
    assert(c0 == 0);

    /* "a" -> 0xE8B7BE43 */
    const char *a = "a";
    uint32_t c1 = vp_crc32_bytes((const uint8_t *)a, 1);
    assert(c1 == 0xE8B7BE43u);

    /* "123456789" -> 0xCBF43926 */
    const char *s = "123456789";
    uint32_t c2 = vp_crc32_bytes((const uint8_t *)s, 9);
    assert(c2 == 0xCBF43926u);

    /* two-slice should match single concatenated */
    const char *s1 = "1234";
    const char *s2 = "56789";
    uint32_t c3 = vp_crc32_two((const uint8_t *)s1, 4,
                               (const uint8_t *)s2, 5);
    assert(c3 == c2);

    /* incremental update matches single-shot */
    uint32_t inc = vp_crc32_init();
    inc = vp_crc32_update(inc, (const uint8_t *)"12345", 5);
    inc = vp_crc32_update(inc, (const uint8_t *)"6789", 4);
    inc = vp_crc32_final(inc);
    assert(inc == c2);

    printf("test_crc32: all assertions passed\n");
    return 0;
}
