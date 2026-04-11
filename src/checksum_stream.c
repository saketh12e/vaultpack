/*
 * src/checksum_stream.c
 *
 * Streaming CRC32 helper. The verifier feeds bytes incrementally and
 * reads the running digest at any time. The state is intended to be
 * stack-allocated; the stream does not retain its input.
 *
 * Used by the section pipeline to validate the declared CRC32 of an
 * inflated payload without having to buffer the whole payload first
 * (although the current pipeline does buffer it; this type exists so
 * future streamed callers can adopt it).
 */

#include <stddef.h>
#include <stdint.h>

#include "vaultpack/crc32.h"
#include "vaultpack/error.h"

void vp_checksum_stream_init(vp_checksum_stream_t *cs)
{
    if (!cs) return;
    cs->crc = 0;
    cs->bytes_seen = 0;
}

void vp_checksum_stream_update(vp_checksum_stream_t *cs,
                               const uint8_t *data, size_t len)
{
    if (!cs || !data) return;
    cs->crc = vp_crc32_update(cs->crc, data, len);
    cs->bytes_seen += len;
}

uint32_t vp_checksum_stream_finalize(vp_checksum_stream_t *cs)
{
    if (!cs) return 0;
    return cs->crc;
}

int vp_checksum_stream_verify(vp_checksum_stream_t *cs, uint32_t expected)
{
    if (!cs) return VP_ERR_INVALID;
    if (cs->crc != expected) return VP_ERR_CHECKSUM;
    return VP_OK;
}
